#include <iostream>
#include <thread>
#include <chrono>
#include "RedisConsumer.h"
#include "PdfParser.h"
#include "Chunker.h"
#include "MilvusClient.h"

auto main(int argc, char** argv) -> int {
    std::cout << "cpp-injestor starting..." << std::endl;

    PdfParser parser;
    Chunker chunker(1024, 256);
    auto ieee_config = PdfParser::get_ieee_config();

    // Dry-run mode: process a single file if provided as an argument
    if (argc > 1) {
        std::string pdf_path = argv[1];
        std::cout << "[Dry-Run] Processing file: " << pdf_path << std::endl;

        std::string markdown = parser.extract_text(pdf_path, ieee_config);
        if (markdown.empty()) {
            std::cerr << "[Dry-Run] Failed to extract text from " << pdf_path << std::endl;
            return 1;
        }

        std::string doc_name = pdf_path;
        size_t last_slash = doc_name.find_last_of("/\\");
        if (last_slash != std::string::npos) doc_name = doc_name.substr(last_slash + 1);

        auto chunks = chunker.chunk(markdown, doc_name);
        std::cout << "[Dry-Run] Generated " << chunks.size() << " chunks" << std::endl;
        
        if (!chunks.empty()) {
            std::cout << "[Dry-Run] Sample Chunk Content (first 100 chars):" << std::endl;
            std::cout << chunks[0].content.substr(0, 100) << "..." << std::endl;
        }
        
        std::cout << "[Dry-Run] Completed successfully." << std::endl;
        return 0;
    }

    RedisConsumer redis("127.0.0.1", 6379, "ingestion:jobs");
    MilvusClient milvus("127.0.0.1", 19530);

    if (!redis.connect()) {
        std::cerr << "Failed to connect to Redis" << std::endl;
        return 1;
    }

    std::cout << "Waiting for jobs on 'ingestion:jobs'..." << std::endl;

    while (true) {
        auto job_opt = redis.consume_job();
        if (!job_opt) {
            // Check for context errors or just retry
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        auto job = *job_opt;
        std::cout << "Processing job: " << job.job_id << " for file: " << job.pdf_path << std::endl;

        // 1. Parse PDF to Markdown
        std::string markdown = parser.extract_text(job.pdf_path, ieee_config);
        if (markdown.empty()) {
            std::cerr << "Failed to extract text from " << job.pdf_path << std::endl;
            continue;
        }

        // 2. Chunk Markdown
        std::string doc_name = job.pdf_path; // Simple doc name
        size_t last_slash = doc_name.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            doc_name = doc_name.substr(last_slash + 1);
        }

        auto chunks = chunker.chunk(markdown, doc_name);
        std::cout << "Generated " << chunks.size() << " chunks" << std::endl;

        // 3. Push to Milvus
        if (milvus.insert_chunks(job.job_id, job.target_collection, chunks)) {
            std::cout << "Successfully ingested job: " << job.job_id << std::endl;
        } else {
            std::cerr << "Failed to insert chunks into Milvus for job: " << job.job_id << std::endl;
        }
    }

    return 0;
}
