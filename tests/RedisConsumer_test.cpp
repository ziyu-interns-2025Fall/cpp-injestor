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

TEST(RedisConsumerTest, SerializeJobForQueue) {
    RedisConsumer consumer("127.0.0.1", 6379, "test_queue");
    JobPayload job{
        "job-123",
        "/tmp/input.pdf",
        "ieee",
        "papers"
    };

    auto serialized = consumer.serialize_job(job);
    auto parsed = nlohmann::json::parse(serialized);

    EXPECT_EQ(parsed["job_id"], "job-123");
    EXPECT_EQ(parsed["pdf_path"], "/tmp/input.pdf");
    EXPECT_EQ(parsed["config_type"], "ieee");
    EXPECT_EQ(parsed["target_collection"], "papers");
}

TEST(RedisConsumerTest, SerializedJobRoundTripsThroughParser) {
    RedisConsumer consumer("127.0.0.1", 6379, "test_queue");
    JobPayload original{
        "job-456",
        "/tmp/source.pdf",
        "ieee",
        "standards"
    };

    auto parsed = consumer.parse_job(consumer.serialize_job(original));

    EXPECT_EQ(parsed.job_id, original.job_id);
    EXPECT_EQ(parsed.pdf_path, original.pdf_path);
    EXPECT_EQ(parsed.config_type, original.config_type);
    EXPECT_EQ(parsed.target_collection, original.target_collection);
}
