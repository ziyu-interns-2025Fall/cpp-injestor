#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <sstream>
#include <mutex>
#include <vector>
#include <fstream>
#include <iomanip>
#include <openssl/evp.h>
#include "RedisConsumer.h"
#include "PdfParser.h"
#include "Chunker.h"
#include "MilvusClient.h"
#include "Embedder.h"

namespace {
constexpr auto kQueueName = "ingestion:jobs";

auto get_env_or_default(const char* env_var, const char* default_value) -> std::string {
    const char* val = std::getenv(env_var);
    return val ? std::string(val) : std::string(default_value);
}

auto get_env_int_or_default(const char* env_var, int default_value) -> int {
    const char* val = std::getenv(env_var);
    if (val) {
        try {
            return std::stoi(val);
        } catch (...) {
            return default_value;
        }
    }
    return default_value;
}
constexpr unsigned int kMaxDefaultWorkers = 4;
constexpr unsigned long kMaxWorkerCount = 128;
std::mutex log_mutex;

auto basename(const std::string& path) -> std::string {
    auto doc_name = path;
    size_t last_slash = doc_name.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        doc_name = doc_name.substr(last_slash + 1);
    }
    return doc_name;
}

auto compute_sha256(const std::string& filepath) -> std::string {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return "";

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);

    char buffer[8192];
    while (file.read(buffer, sizeof(buffer))) {
        EVP_DigestUpdate(ctx, buffer, file.gcount());
    }
    if (file.gcount() > 0) {
        EVP_DigestUpdate(ctx, buffer, file.gcount());
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int lengthOfHash = 0;
    EVP_DigestFinal_ex(ctx, hash, &lengthOfHash);
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    for (unsigned int i = 0; i < lengthOfHash; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return oss.str();
}

auto generate_job_id() -> std::string {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<unsigned long long> dist;

    std::ostringstream oss;
    oss << std::hex << now << "-" << dist(gen);
    return oss.str();
}

auto default_worker_count() -> unsigned int {
    auto hardware_workers = std::thread::hardware_concurrency();
    if (hardware_workers == 0) {
        return 2;
    }
    if (hardware_workers < 2) {
        return 2;
    }
    if (hardware_workers > kMaxDefaultWorkers) {
        return kMaxDefaultWorkers;
    }
    return hardware_workers;
}

auto parse_worker_count(const std::string& value, unsigned int& worker_count) -> bool {
    try {
        size_t parsed_chars = 0;
        auto parsed = std::stoul(value, &parsed_chars);
        if (parsed_chars != value.size() || parsed == 0 || parsed > kMaxWorkerCount) {
            return false;
        }

        worker_count = static_cast<unsigned int>(parsed);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

auto log_info(unsigned int worker_id, const std::string& message) -> void {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cout << "[Worker " << worker_id << "] " << message << std::endl;
}

auto log_error(unsigned int worker_id, const std::string& message) -> void {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cerr << "[Worker " << worker_id << "] " << message << std::endl;
}

auto process_jobs(unsigned int worker_id) -> void {
    PdfParser parser;
    Chunker chunker(1024, 256);
    std::string redis_host = get_env_or_default("REDIS_HOST", "127.0.0.1");
    int redis_port = get_env_int_or_default("REDIS_PORT", 6379);
    RedisConsumer redis(redis_host, redis_port, kQueueName);
    
    std::string milvus_host = get_env_or_default("MILVUS_HOST", "127.0.0.1");
    int milvus_port = get_env_int_or_default("MILVUS_PORT", 19530);
    MilvusClient milvus(milvus_host, milvus_port);

    if (!redis.connect()) {
        log_error(worker_id, "Failed to connect to Redis");
        return;
    }

    log_info(worker_id, "Waiting for jobs on '" + std::string(kQueueName) + "'");

    while (true) {
        auto job_opt = redis.consume_job();
        if (!job_opt) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        auto job = *job_opt;
        log_info(worker_id, "Processing job: " + job.job_id + " for file: " + job.pdf_path);

        std::string file_hash = compute_sha256(job.pdf_path);
        if (file_hash.empty()) {
            log_error(worker_id, "Failed to read file for hashing: " + job.pdf_path);
            continue;
        }

        if (!redis.acquire_file_lock(file_hash)) {
            log_info(worker_id, "File already processed, skipping: " + job.pdf_path);
            continue;
        }

        ParserConfig config = (job.config_type == "ieee") ? PdfParser::get_ieee_config() : PdfParser::get_generic_config();
        std::string markdown = parser.extract_text(job.pdf_path, config);
        if (markdown.empty()) {
            log_error(worker_id, "Failed to extract text from " + job.pdf_path);
            continue;
        }

        auto chunks = chunker.chunk(markdown, basename(job.pdf_path));
        log_info(worker_id, "Generated " + std::to_string(chunks.size()) + " chunks");

        std::string ollama_host = get_env_or_default("OLLAMA_HOST", "127.0.0.1");
        int ollama_port = get_env_int_or_default("OLLAMA_PORT", 11434);
        Embedder embedder(ollama_host, ollama_port);
        for (auto& chunk : chunks) {
            chunk.embedding = embedder.embed(chunk.content);
        }

        if (milvus.insert_chunks(job.job_id, job.target_collection, chunks)) {
            log_info(worker_id, "Successfully ingested job: " + job.job_id);
        } else {
            log_error(worker_id, "Failed to insert chunks into Milvus for job: " + job.job_id);
        }
    }
}

auto run_workers(unsigned int worker_count) -> int {
    std::string redis_host = get_env_or_default("REDIS_HOST", "127.0.0.1");
    int redis_port = get_env_int_or_default("REDIS_PORT", 6379);

    RedisConsumer redis_probe(redis_host, redis_port, kQueueName);
    if (!redis_probe.connect()) {
        std::cerr << "Failed to connect to Redis" << std::endl;
        return 1;
    }

    std::cout << "Starting " << worker_count << " ingestion workers on '"
              << kQueueName << "'..." << std::endl;

    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    for (unsigned int i = 0; i < worker_count; ++i) {
        workers.emplace_back(process_jobs, i + 1);
    }

    for (auto& worker : workers) {
        worker.join();
    }

    return 0;
}

auto print_usage(const char* program_name) -> void {
    std::cout
        << "Usage:\n"
        << "  " << program_name << "                         Run ingestion workers (default: " << default_worker_count() << ")\n"
        << "  " << program_name << " --workers <count>         Run ingestion workers with explicit concurrency\n"
        << "  " << program_name << " --enqueue <pdf_path> <target_collection> [job_id] [config_type]\n"
        << "  " << program_name << " --dry-run <pdf_path>       Parse and chunk one PDF without Redis/Milvus\n"
        << "  " << program_name << " <pdf_path>                 Legacy dry-run shorthand\n";
}
}

auto main(int argc, char** argv) -> int {
    std::cout << "cpp-ingestor starting..." << std::endl;

    if (argc > 1 && std::string(argv[1]) == "--help") {
        print_usage(argv[0]);
        return 0;
    }

    if (argc > 1 && std::string(argv[1]) == "--workers") {
        unsigned int worker_count = 0;
        if (argc < 3 || !parse_worker_count(argv[2], worker_count)) {
            print_usage(argv[0]);
            return 1;
        }

        return run_workers(worker_count);
    }

    if (argc > 1 && std::string(argv[1]) == "--enqueue") {
        if (argc < 4) {
            print_usage(argv[0]);
            return 1;
        }

        JobPayload job{
            argc > 4 ? argv[4] : generate_job_id(),
            argv[2],
            argc > 5 ? argv[5] : "ieee",
            argv[3]
        };

        std::string redis_host = get_env_or_default("REDIS_HOST", "127.0.0.1");
        int redis_port = get_env_int_or_default("REDIS_PORT", 6379);

        RedisConsumer redis(redis_host, redis_port, kQueueName);
        if (!redis.connect()) {
            std::cerr << "Failed to connect to Redis" << std::endl;
            return 1;
        }

        if (!redis.enqueue_job(job)) {
            std::cerr << "Failed to enqueue ingestion job" << std::endl;
            return 1;
        }

        std::cout << "Queued ingestion job " << job.job_id
                  << " for file: " << job.pdf_path
                  << " on '" << kQueueName << "'" << std::endl;
        return 0;
    }

    if (argc > 1 && std::string(argv[1]) == "--ingest-direct") {
        if (argc < 4) {
            print_usage(argv[0]);
            return 1;
        }

        JobPayload job{
            argc > 4 ? argv[4] : generate_job_id(),
            argv[2],
            argc > 5 ? argv[5] : "generic",
            argv[3]
        };

        PdfParser parser;
        Chunker chunker(1024, 256);
        std::string milvus_host = get_env_or_default("MILVUS_HOST", "127.0.0.1");
        int milvus_port = get_env_int_or_default("MILVUS_PORT", 19530);
        MilvusClient milvus(milvus_host, milvus_port);

        std::string redis_host = get_env_or_default("REDIS_HOST", "127.0.0.1");
        int redis_port = get_env_int_or_default("REDIS_PORT", 6379);
        RedisConsumer redis(redis_host, redis_port, kQueueName);
        if (redis.connect()) {
            std::string file_hash = compute_sha256(job.pdf_path);
            if (!file_hash.empty() && !redis.acquire_file_lock(file_hash)) {
                std::cout << "File already processed, skipping: " << job.pdf_path << std::endl;
                return 0;
            }
        }

        std::cout << "Directly processing file: " << job.pdf_path << std::endl;

        ParserConfig config = (job.config_type == "ieee") ? PdfParser::get_ieee_config() : PdfParser::get_generic_config();
        std::string markdown = parser.extract_text(job.pdf_path, config);
        if (markdown.empty()) {
            std::cerr << "Failed to extract text from " << job.pdf_path << std::endl;
            return 1;
        }

        auto doc_name = basename(job.pdf_path);
        auto chunks = chunker.chunk(markdown, doc_name);
        std::cout << "Generated " << chunks.size() << " chunks" << std::endl;

        std::string ollama_host = get_env_or_default("OLLAMA_HOST", "127.0.0.1");
        int ollama_port = get_env_int_or_default("OLLAMA_PORT", 11434);
        Embedder embedder(ollama_host, ollama_port);
        for (auto& chunk : chunks) {
            chunk.embedding = embedder.embed(chunk.content);
        }

        if (milvus.insert_chunks(job.job_id, job.target_collection, chunks)) {
            std::cout << "Successfully ingested job: " << job.job_id << std::endl;
        } else {
            std::cerr << "Failed to insert chunks into Milvus for job: " << job.job_id << std::endl;
            return 1;
        }

        return 0;
    }

    // Dry-run mode: process a single file if provided as an argument
    if (argc > 1) {
        PdfParser parser;
        Chunker chunker(1024, 256);
        auto ieee_config = PdfParser::get_ieee_config();

        if (std::string(argv[1]) == "--dry-run" && argc < 3) {
            print_usage(argv[0]);
            return 1;
        }

        std::string pdf_path = std::string(argv[1]) == "--dry-run" && argc > 2 ? argv[2] : argv[1];
        std::cout << "[Dry-Run] Processing file: " << pdf_path << std::endl;

        ParserConfig config = PdfParser::get_generic_config();
        std::string markdown = parser.extract_text(pdf_path, config);
        if (markdown.empty()) {
            std::cerr << "[Dry-Run] Failed to extract text from " << pdf_path << std::endl;
            return 1;
        }

        auto doc_name = basename(pdf_path);

        auto chunks = chunker.chunk(markdown, doc_name);
        std::cout << "[Dry-Run] Generated " << chunks.size() << " chunks" << std::endl;

        std::string ollama_host = get_env_or_default("OLLAMA_HOST", "127.0.0.1");
        int ollama_port = get_env_int_or_default("OLLAMA_PORT", 11434);
        Embedder embedder(ollama_host, ollama_port);
        for (auto& chunk : chunks) {
            chunk.embedding = embedder.embed(chunk.content);
        }

        if (!chunks.empty()) {
            std::cout << "[Dry-Run] Sample Chunk Content (first 100 chars):" << std::endl;
            std::cout << chunks[0].content.substr(0, 100) << "..." << std::endl;
            std::cout << "[Dry-Run] Sample Embedding size: " << chunks[0].embedding.size() << std::endl;
        }

        std::cout << "[Dry-Run] Completed successfully." << std::endl;
        return 0;
    }

    return run_workers(default_worker_count());
}
