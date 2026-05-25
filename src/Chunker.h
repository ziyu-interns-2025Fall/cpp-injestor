#pragma once
#include <string>
#include <vector>
#include <map>

struct Chunk {
    std::string content;
    std::map<std::string, std::string> metadata;
};

class Chunker {
public:
    Chunker(size_t chunk_size = 1024, size_t chunk_overlap = 256);
    auto chunk(const std::string& markdown, const std::string& document_name) -> std::vector<Chunk>;

private:
    size_t chunk_size_;
    size_t chunk_overlap_;

    auto split_by_headers(const std::string& markdown) -> std::vector<Chunk>;
    auto split_recursively(const Chunk& parent_chunk) -> std::vector<Chunk>;
};
