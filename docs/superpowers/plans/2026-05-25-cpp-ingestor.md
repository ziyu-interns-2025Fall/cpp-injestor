# C++ Ingestor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a high-performance C++ worker that consumes ingestion jobs from Redis, parses PDFs using Poppler, chunks the text according to header configurations, and pushes the raw chunks to a Milvus staging collection.

**Architecture:** A Producer-Consumer model where the `cpp-ingestor` is the consumer. It uses `hiredis` for blocking queue reads (`BLPOP`), `poppler-cpp` for text and layout extraction, and the `milvus-sdk-cpp` for database insertions.

**Tech Stack:** C++20, CMake, Poppler, Hiredis, Milvus C++ SDK, nlohmann/json, GoogleTest (GTest).

---

## File Structure

- `CMakeLists.txt`: Build configuration.
- `src/main.cpp`: Entry point.
- `src/RedisConsumer.h/cpp`: Manages connection to Redis and job consumption.
- `src/JobPayload.h`: Defines the job structure.
- `src/PdfParser.h/cpp`: Handles Poppler integration and text/layout extraction.
- `src/Chunker.h/cpp`: Implements the MarkdownHeaderTextSplitter and RecursiveCharacterTextSplitter logic.
- `src/MilvusClient.h/cpp`: Manages connection and insertions into the `ingestion_staging` collection.
- `tests/`: Directory for GTest files corresponding to each module.

---

### Task 1: Project Initialization & Build Setup

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/main.cpp`
- Create: `tests/main_test.cpp`

- [ ] **Step 1: Write the basic build configuration**
```cmake
cmake_minimum_required(VERSION 3.14)
project(cpp_ingestor CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER REQUIRED poppler-cpp)
find_package(hiredis REQUIRED)
find_package(nlohmann_json REQUIRED)

# GTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(ingestor src/main.cpp)
target_include_directories(ingestor PRIVATE ${POPPLER_INCLUDE_DIRS})
target_link_libraries(ingestor PRIVATE ${POPPLER_LIBRARIES} hiredis::hiredis nlohmann_json::nlohmann_json)

add_executable(ingestor_tests tests/main_test.cpp)
target_link_libraries(ingestor_tests gtest_main)
```

- [ ] **Step 2: Create a dummy test**
```cpp
#include <gtest/gtest.h>

TEST(DummyTest, EnvironmentIsSetup) {
    EXPECT_TRUE(true);
}
```

- [ ] **Step 3: Create basic main.cpp**
```cpp
#include <iostream>

int main() {
    std::cout << "cpp-ingestor starting..." << std::endl;
    return 0;
}
```

- [ ] **Step 4: Verify build and test pass**
Run: `mkdir build && cd build && cmake .. && make && ./ingestor_tests`
Expected: PASS with "1 test from 1 test suite ran"

- [ ] **Step 5: Commit**
```bash
git add CMakeLists.txt src/main.cpp tests/main_test.cpp
git commit -m "chore: initialize project with cmake and gtest"
```

---

### Task 2: Define Job Payload and Redis Consumer Stub

**Files:**
- Create: `src/JobPayload.h`
- Create: `src/RedisConsumer.h`
- Create: `src/RedisConsumer.cpp`
- Create: `tests/RedisConsumer_test.cpp`

- [ ] **Step 1: Define JobPayload structure**
```cpp
// src/JobPayload.h
#pragma once
#include <string>

struct JobPayload {
    std::string job_id;
    std::string pdf_path;
    std::string config_type;
    std::string target_collection;
};
```

- [ ] **Step 2: Write failing test for RedisConsumer parsing**
```cpp
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
```

- [ ] **Step 3: Implement RedisConsumer (focusing only on parse_job for now to make test pass)**
```cpp
// src/RedisConsumer.h
#pragma once
#include "JobPayload.h"
#include <string>
#include <nlohmann/json.hpp>

class RedisConsumer {
public:
    RedisConsumer(const std::string& host, int port, const std::string& queue_name);
    JobPayload parse_job(const std::string& json_str);
private:
    std::string host_;
    int port_;
    std::string queue_name_;
};

// src/RedisConsumer.cpp
#include "RedisConsumer.h"

RedisConsumer::RedisConsumer(const std::string& host, int port, const std::string& queue_name) 
    : host_(host), port_(port), queue_name_(queue_name) {}

JobPayload RedisConsumer::parse_job(const std::string& json_str) {
    auto j = nlohmann::json::parse(json_str);
    return JobPayload{
        j["job_id"],
        j["pdf_path"],
        j["config_type"],
        j["target_collection"]
    };
}
```

- [ ] **Step 4: Update CMakeLists and verify test passes**
Modify CMakeLists.txt to include the new files in the test executable.
Run: `cd build && cmake .. && make && ./ingestor_tests`
Expected: PASS.

- [ ] **Step 5: Commit**
```bash
git add src/JobPayload.h src/RedisConsumer.h src/RedisConsumer.cpp tests/RedisConsumer_test.cpp CMakeLists.txt
git commit -m "feat: implement job payload and redis parsing logic"
```

---

### Task 3: PDF Parsing with Poppler (Basic Extraction)

**Files:**
- Create: `src/PdfParser.h`
- Create: `src/PdfParser.cpp`
- Create: `tests/PdfParser_test.cpp`

- [ ] **Step 1: Write failing test for PdfParser**
(Assuming you will create a dummy PDF file `/tmp/dummy.pdf` during the test setup)
```cpp
// tests/PdfParser_test.cpp
#include <gtest/gtest.h>
#include "../src/PdfParser.h"
#include <fstream>

class PdfParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // In a real scenario, create a minimal valid PDF byte sequence here.
        // For plan brevity, assuming a mock file or minimal implementation.
        std::ofstream out("/tmp/test_doc.txt");
        out << "test";
    }
};

TEST_F(PdfParserTest, ExtractText) {
    PdfParser parser;
    // We will mock the poppler call in the actual implementation if a real PDF isn't handy,
    // but the API surface should look like this:
    std::string text = parser.extract_text("/nonexistent.pdf");
    EXPECT_EQ(text, ""); // Expect empty or throw on non-existent
}
```

- [ ] **Step 2: Implement basic PdfParser interface**
```cpp
// src/PdfParser.h
#pragma once
#include <string>

class PdfParser {
public:
    std::string extract_text(const std::string& filepath);
};

// src/PdfParser.cpp
#include "PdfParser.h"
#include <poppler-document.h>
#include <poppler-page.h>

std::string PdfParser::extract_text(const std::string& filepath) {
    auto doc = poppler::document::load_from_file(filepath);
    if (!doc) return "";
    
    std::string full_text;
    for (int i = 0; i < doc->pages(); ++i) {
        auto page = doc->create_page(i);
        if (page) {
            full_text += page->text().to_latin1();
            full_text += "\n";
        }
    }
    return full_text;
}
```

- [ ] **Step 3: Update CMakeLists and verify test passes**
Modify CMakeLists.txt.
Run: `cd build && cmake .. && make && ./ingestor_tests`
Expected: PASS.

- [ ] **Step 4: Commit**
```bash
git add src/PdfParser.h src/PdfParser.cpp tests/PdfParser_test.cpp CMakeLists.txt
git commit -m "feat: implement basic pdf text extraction using poppler"
```

---

### Task 4: Header Detection and Markdown Formatting

**Files:**
- Modify: `src/PdfParser.h`
- Modify: `src/PdfParser.cpp`
- Test: `tests/PdfParser_test.cpp`

- [ ] **Step 1: Define HeaderRule and Config structures**
```cpp
// Update src/PdfParser.h
struct HeaderRule {
    std::pair<double, double> size_range;
    std::string pattern; // Regex
    bool is_bold;
    int level;
};

struct ParserConfig {
    std::vector<HeaderRule> rules;
    double footer_zone; // e.g. 0.88
    double header_zone; // e.g. 0.08
};
```

- [ ] **Step 2: Implement Configurable Header Detection**
Replicate the `ConfigurableHeaderDetector` from `pyapi`.
```cpp
// Update src/PdfParser.cpp
int get_header_level(const poppler::text_box& box, const ParserConfig& config) {
    // Check zones
    // Check rules (size, bold, pattern)
    // Return level (1-6) or 0 if not a header
}
```

- [ ] **Step 3: Update `extract_text` to return Markdown**
Instead of just joining text, prepend `#` based on detected header levels.

- [ ] **Step 4: Verify with tests**
Expected: PASS.

- [ ] **Step 5: Commit**
```bash
git add src/PdfParser.h src/PdfParser.cpp tests/PdfParser_test.cpp
git commit -m "feat: add header detection and markdown formatting"
```

---

### Task 5: Implement Text Chunking

**Files:**
- Create: `src/Chunker.h`
- Create: `src/Chunker.cpp`
- Test: `tests/Chunker_test.cpp`

- [ ] **Step 1: Implement MarkdownHeaderSplitter logic**
Split the markdown text by detected headers while preserving the hierarchy in metadata.

- [ ] **Step 2: Implement RecursiveCharacterTextSplitter logic**
Further split large chunks into smaller ones (target size 1024, overlap 256) while keeping headers attached.

- [ ] **Step 3: Verify with tests**
Expected: PASS.

- [ ] **Step 4: Commit**
```bash
git add src/Chunker.h src/Chunker.cpp tests/Chunker_test.cpp
git commit -m "feat: implement markdown and recursive character chunking"
```

---

### Task 6: Implement Milvus Staging Client

**Files:**
- Create: `src/MilvusClient.h`
- Create: `src/MilvusClient.cpp`
- Test: `tests/MilvusClient_test.cpp`

- [ ] **Step 1: Setup Milvus C++ SDK connection**
Implement connection logic to `localhost:19530`.

- [ ] **Step 2: Implement Staging Collection Insertion**
Write logic to insert chunks into the `ingestion_staging` collection.

- [ ] **Step 3: Commit**
```bash
git add src/MilvusClient.h src/MilvusClient.cpp tests/MilvusClient_test.cpp
git commit -m "feat: implement milvus staging client"
```

---

### Task 7: Main Hiredis Loop and Integration

**Files:**
- Modify: `src/main.cpp`
- Modify: `src/RedisConsumer.cpp`

- [ ] **Step 1: Implement blocking Redis consumer**
Use `BLPOP` in `RedisConsumer` to wait for jobs.

- [ ] **Step 2: Wire everything together in `main.cpp`**
Loop: Wait for job -> Parse PDF -> Chunk -> Push to Milvus.

- [ ] **Step 3: Final verification**
Run the full pipeline with a sample job in Redis.

- [ ] **Step 4: Commit**
```bash
git add src/main.cpp src/RedisConsumer.cpp
git commit -m "feat: implement main consumer loop and integration"
```

