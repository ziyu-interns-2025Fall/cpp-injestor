#pragma once
#include "JobPayload.h"
#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include <hiredis/hiredis.h>

class RedisConsumer {
public:
    RedisConsumer(const std::string& host, int port, const std::string& queue_name);
    ~RedisConsumer();
    auto connect() -> bool;
    auto enqueue_job(const JobPayload& job) -> bool;
    auto consume_job() -> std::optional<JobPayload>;
    auto acquire_file_lock(const std::string& file_hash) -> bool;
    auto serialize_job(const JobPayload& job) -> std::string;
    auto parse_job(const std::string& json_str) -> JobPayload;

private:
    std::string host_;
    int port_;
    std::string queue_name_;
    redisContext* context_ = nullptr;
};
