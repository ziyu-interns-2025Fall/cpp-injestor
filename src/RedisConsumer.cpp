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

auto RedisConsumer::enqueue_job(const JobPayload& job) -> bool {
    if (!context_) return false;

    const auto payload = serialize_job(job);
    redisReply* reply = (redisReply*)redisCommand(
        context_,
        "RPUSH %s %b",
        queue_name_.c_str(),
        payload.data(),
        payload.size()
    );

    if (reply == nullptr) {
        if (context_ && context_->err) {
            std::cerr << "Redis enqueue error: " << context_->errstr << std::endl;
        }
        return false;
    }

    bool success = reply->type == REDIS_REPLY_INTEGER && reply->integer > 0;
    if (!success && reply->type == REDIS_REPLY_ERROR && reply->str) {
        std::cerr << "Redis enqueue error: " << reply->str << std::endl;
    }

    freeReplyObject(reply);
    return success;
}

auto RedisConsumer::consume_job() -> std::optional<JobPayload> {
    if (!context_) return std::nullopt;

    // BLPOP key timeout (0 = block indefinitely)
    redisReply* reply = (redisReply*)redisCommand(context_, "BLPOP %s 0", queue_name_.c_str());
    if (reply == nullptr) return std::nullopt;

    if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 2) {
        std::string json_str(reply->element[1]->str, reply->element[1]->len);
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

auto RedisConsumer::acquire_file_lock(const std::string& file_hash) -> bool {
    if (!context_) return false;

    std::string key = "processed_files:" + file_hash;
    redisReply* reply = (redisReply*)redisCommand(
        context_, 
        "SETNX %s 1", 
        key.c_str()
    );

    if (reply == nullptr) return false;

    bool success = reply->type == REDIS_REPLY_INTEGER && reply->integer == 1;
    freeReplyObject(reply);
    return success;
}

auto RedisConsumer::serialize_job(const JobPayload& job) -> std::string {
    nlohmann::json j = {
        {"job_id", job.job_id},
        {"pdf_path", job.pdf_path},
        {"config_type", job.config_type},
        {"target_collection", job.target_collection}
    };
    return j.dump();
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
