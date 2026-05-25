// tests/RedisConsumer_test.cpp
#include <gtest/gtest.h>
#include "../src/RedisConsumer.h"
#include <nlohmann/json.hpp>

TEST(RedisConsumerTest, ParseValidJob) {
    std::string json_str = R"({"job_id":"123","pdf_path":"/tmp/a.pdf","config_type":"ieee","target_collection":"col"})";
    RedisConsumer consumer("127.0.0.1", 6379, "test_queue");
    auto job = consumer.parse_job(json_str);
    EXPECT_EQ(job.job_id, "123");
    EXPECT_EQ(job.pdf_path, "/tmp/a.pdf");
}
