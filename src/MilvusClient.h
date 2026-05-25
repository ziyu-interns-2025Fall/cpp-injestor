#pragma once
#include <string>
#include <vector>
#include "Chunker.h"

class MilvusClient {
public:
    MilvusClient(const std::string& host, int port);
    auto connect() -> bool;
    auto insert_chunks(const std::string& job_id, const std::string& target_collection, const std::vector<Chunk>& chunks) -> bool;

private:
    std::string host_;
    int port_;
    // Pointer to implementation or client object if using SDK
};
