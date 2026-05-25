# PDF Parsing with Poppler Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement basic PDF text extraction using the Poppler library.

**Architecture:** Create a `PdfParser` class with a simple interface `extract_text(filepath)` that returns a `std::string`. It uses `poppler-cpp` for extraction.

**Tech Stack:** C++20, Poppler (poppler-cpp), GoogleTest.

---

### Task 1: Create PdfParser failing test

**Files:**
- Create: `tests/PdfParser_test.cpp`

- [x] **Step 1: Write the failing test**

```cpp
#include <gtest/gtest.h>
#include "../src/PdfParser.h"
#include <fstream>

TEST(PdfParserTest, ExtractTextFromRealPdf) {
    PdfParser parser;
    // Use the existing PDF in pyapi for testing
    std::string pdf_path = "/Users/alfred/work/pyapi/src/rag/public/IEEE1584-2018-31-36.pdf";
    std::string text = parser.extract_text(pdf_path);
    EXPECT_FALSE(text.empty());
    EXPECT_NE(text.find("IEEE"), std::string::npos);
}

TEST(PdfParserTest, HandlesNonExistentFile) {
    PdfParser parser;
    std::string text = parser.extract_text("/nonexistent.pdf");
    EXPECT_EQ(text, "");
}
```

- [x] **Step 2: Update CMakeLists.txt to include the new test**

```cmake
add_executable(injestor_tests 
    tests/main_test.cpp 
    tests/RedisConsumer_test.cpp 
    tests/PdfParser_test.cpp 
    src/RedisConsumer.cpp
)
```

- [x] **Step 3: Run test to verify it fails**

Run: `cd build && cmake .. && make && ./injestor_tests`
Expected: FAIL (Compilation error: PdfParser.h not found)

- [x] **Step 4: Commit**

```bash
git add tests/PdfParser_test.cpp CMakeLists.txt
git commit -m "test: add failing tests for PdfParser"
```

---

### Task 2: Implement PdfParser Interface and Minimal Code

**Files:**
- Create: `src/PdfParser.h`
- Create: `src/PdfParser.cpp`

- [x] **Step 1: Create PdfParser.h**

```cpp
#pragma once
#include <string>

class PdfParser {
public:
    std::string extract_text(const std::string& filepath);
};
```

- [x] **Step 2: Create PdfParser.cpp with minimal implementation**

```cpp
#include "PdfParser.h"
#include <poppler-document.h>
#include <poppler-page.h>
#include <iostream>

std::string PdfParser::extract_text(const std::string& filepath) {
    auto doc = poppler::document::load_from_file(filepath);
    if (!doc) {
        return "";
    }
    
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

- [x] **Step 3: Update CMakeLists.txt to include PdfParser.cpp**

```cmake
add_executable(injestor src/main.cpp src/RedisConsumer.cpp src/PdfParser.cpp)
# ...
add_executable(injestor_tests 
    tests/main_test.cpp 
    tests/RedisConsumer_test.cpp 
    tests/PdfParser_test.cpp 
    src/RedisConsumer.cpp 
    src/PdfParser.cpp
)
```

- [x] **Step 4: Run test to verify it passes**

Run: `cd build && cmake .. && make && ./injestor_tests`
Expected: PASS

- [x] **Step 5: Commit**

```bash
git add src/PdfParser.h src/PdfParser.cpp CMakeLists.txt
git commit -m "feat: implement basic pdf text extraction using poppler"
```

---

### Task 3: Final Verification and Cleanup

- [x] **Step 1: Run all tests one last time**

Run: `./build/injestor_tests`
Expected: All tests pass

- [x] **Step 2: Self-review changes**

Verify file headers, formatting, and adherence to C++20 standards.
