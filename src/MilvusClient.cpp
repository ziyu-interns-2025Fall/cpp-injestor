#include "MilvusClient.h"
#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

MilvusClient::MilvusClient(const std::string& host, int port)
    : host_(host), port_(port) {}

auto MilvusClient::connect() -> bool {
    // REST API is stateless, but we can check if the server is up
    return true;
}

auto WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) -> size_t {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

auto MilvusClient::insert_chunks(const std::string& job_id, const std::string& target_collection, const std::vector<Chunk>& chunks) -> bool {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string url = "http://" + host_ + ":" + std::to_string(port_) + "/v2/vectordb/entities/insert";

    json data = json::array();
    for (const auto& chunk : chunks) {
        json entity;
        // Map fields for ingestion_staging
        entity["job_id"] = job_id;
        entity["content"] = chunk.content;
        entity["target_collection"] = target_collection;
        entity["status"] = 1; // Embedded

        if (chunk.embedding.empty()) {
            std::cerr << "Warning: chunk embedding is empty, using zeros" << std::endl;
            std::vector<float> dummy_vector(768, 0.0f);
            entity["vector"] = dummy_vector;
        } else {
            entity["vector"] = chunk.embedding;
        }

        json meta = json::object();
        for (const auto& [key, value] : chunk.metadata) {
            meta[key] = value;
        }
        entity["metadata_json"] = meta.dump(-1, ' ', false, json::error_handler_t::replace);

        data.push_back(entity);
    }

    json payload;
    payload["collectionName"] = "ingestion_staging";
    payload["data"] = data;

    std::string payload_str = payload.dump(-1, ' ', false, json::error_handler_t::replace);
    std::string response_string;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Authorization: Bearer root:Milvus");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

    CURLcode res = curl_easy_perform(curl);
    bool success = (res == CURLE_OK);

    if (!success) {
        std::cerr << "CURL Error: " << curl_easy_strerror(res) << std::endl;
    } else {
        auto resp_json = json::parse(response_string);
        if (resp_json.contains("code") && resp_json["code"] != 0) {
            std::cerr << "Milvus Error: " << response_string << std::endl;
            success = false;
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return success;
}
