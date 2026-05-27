#include "Embedder.h"
#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
    auto WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) -> size_t {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }
}

Embedder::Embedder(const std::string& host, int port)
    : host_(host), port_(port) {}

auto Embedder::embed(const std::string& text) -> std::vector<float> {
    CURL* curl = curl_easy_init();
    if (!curl) return {};

    std::string url = "http://" + host_ + ":" + std::to_string(port_) + "/api/embeddings";

    json payload;
    payload["model"] = "nomic-embed-text";
    payload["prompt"] = text;

    std::string payload_str = payload.dump();
    std::string response_string;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

    CURLcode res = curl_easy_perform(curl);
    bool success = (res == CURLE_OK);

    std::vector<float> embedding;

    if (!success) {
        std::cerr << "CURL Error in Embedder: " << curl_easy_strerror(res) << std::endl;
    } else {
        try {
            auto resp_json = json::parse(response_string);
            if (resp_json.contains("embedding")) {
                embedding = resp_json["embedding"].get<std::vector<float>>();
            } else {
                std::cerr << "Ollama Error: embedding not found in response: " << response_string << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse Ollama response: " << e.what() << std::endl;
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return embedding;
}
