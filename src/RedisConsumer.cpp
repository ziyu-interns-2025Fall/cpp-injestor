#include "RedisConsumer.h"
#include <iostream>

RedisConsumer::RedisConsumer(const std::string& host, int port, const std::string& queue_name) 
    : host_(host), port_(port), queue_name_(queue_name) {}

RedisConsumer::~RedisConsumer() {
    if (context_) {
        redisFree(context_);
    }
}

auto RedisConsumer::connect() -> bool {
    context_ = redisConnect(host_.c_str(), port_);
    if (context_ == nullptr || context_->err) {
        if (context_) {
            std::cerr << "Error: " << context_->errstr << std::endl;
        } else {
            std::cerr << "Can't allocate redis context" << std::endl;
        }
        return false;
    }
    return true;
}

auto RedisConsumer::consume_job() -> std::optional<JobPayload> {
    if (!context_) return std::nullopt;

    // BLPOP key timeout (0 = block indefinitely)
    redisReply* reply = (redisReply*)redisCommand(context_, "BLPOP %s 0", queue_name_.c_str());
    if (reply == nullptr) return std::nullopt;

    if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 2) {
        std::string json_str = reply->element[1]->str;
        freeReplyObject(reply);
        try {
            return parse_job(json_str);
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse job: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    freeReplyObject(reply);
    return std::nullopt;
}

auto RedisConsumer::parse_job(const std::string& json_str) -> JobPayload {
    auto j = nlohmann::json::parse(json_str);
    return JobPayload{
        j.value("job_id", ""),
        j.value("pdf_path", ""),
        j.value("config_type", "ieee"),
        j.value("target_collection", "")
    };
}
