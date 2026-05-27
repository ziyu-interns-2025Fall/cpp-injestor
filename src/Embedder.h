#pragma once
#include <string>
#include <vector>

class Embedder {
public:
    Embedder(const std::string& host = "127.0.0.1", int port = 11434);
    auto embed(const std::string& text) -> std::vector<float>;

private:
    std::string host_;
    int port_;
};
