#include "Chunker.h"
#include <sstream>

Chunker::Chunker(size_t chunk_size, size_t chunk_overlap)
    : chunk_size_(chunk_size), chunk_overlap_(chunk_overlap) {}

auto Chunker::chunk(const std::string& markdown, const std::string& document_name) -> std::vector<Chunk> {
    auto header_chunks = split_by_headers(markdown);
    std::vector<Chunk> final_chunks;

    for (auto& hc : header_chunks) {
        hc.metadata["name"] = document_name;
        auto sub_chunks = split_recursively(hc);
        final_chunks.insert(final_chunks.end(), sub_chunks.begin(), sub_chunks.end());
    }

    return final_chunks;
}

auto Chunker::split_by_headers(const std::string& markdown) -> std::vector<Chunk> {
    std::vector<Chunk> chunks;
    std::stringstream ss(markdown);
    std::string line;

    Chunk current_chunk;
    std::map<std::string, std::string> current_headers;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;

        if (line[0] == '#') {
            // New header
            if (!current_chunk.content.empty()) {
                chunks.push_back(current_chunk);
                current_chunk.content = "";
            }

            int level = 0;
            while (level < line.size() && (size_t)level < line.size() && line[level] == '#') level++;

            std::string header_text = line.substr(level);
            // Trim leading space
            if (!header_text.empty() && header_text[0] == ' ') header_text = header_text.substr(1);

            std::string key = "Header " + std::to_string(level);
            current_headers[key] = header_text;

            // Clear sub-headers
            constexpr int subheader_max_level = 5;
            for (int i = level + 1; i <= subheader_max_level; ++i) {
                current_headers.erase("Header " + std::to_string(i));
            }

            current_chunk.metadata = current_headers;
        } else {
            if (!current_chunk.content.empty()) current_chunk.content += "\n";
            current_chunk.content += line;
        }
    }

    if (!current_chunk.content.empty()) {
        chunks.push_back(current_chunk);
    }

    return chunks;
}

auto Chunker::split_recursively(const Chunk& parent_chunk) -> std::vector<Chunk> {
    if (parent_chunk.content.size() <= chunk_size_) {
        return {parent_chunk};
    }

    std::vector<Chunk> chunks;
    const std::string& text = parent_chunk.content;
    size_t start = 0;

    while (start < text.size()) {
        size_t end = std::min(start + chunk_size_, text.size());

        // Try to find a good break point (newline or space) if not at end
        if (end < text.size()) {
            size_t last_space = text.find_last_of(" \n", end);
            // Ensure the break point is far enough from start to make progress
            if (last_space != std::string::npos && last_space > start + chunk_overlap_) {
                end = last_space;
            }
        }

        Chunk c;
        c.content = text.substr(start, end - start);
        c.metadata = parent_chunk.metadata;
        chunks.push_back(c);

        if (end >= text.size()) break;

        // Next start is end minus overlap
        size_t next_start = end > chunk_overlap_ ? end - chunk_overlap_ : end;

        // Force progress if we're stuck
        if (next_start <= start) {
            start = end;
        } else {
            start = next_start;
        }
    }

    return chunks;
}
