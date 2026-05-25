# Header Detection and Markdown Formatting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement header detection based on font size, bold status, and regex patterns to format PDF text into Markdown.

**Architecture:** Update `PdfParser` to use `poppler::text_box` for granular text analysis. Group words into lines based on Y-coordinates. Implement logic to identify headers and prepend `#`.

**Tech Stack:** C++20, Poppler (poppler-cpp), Google Test, `<regex>`.

---

### Task 1: Update `PdfParser.h` with Header Detection Structures

**Files:**
- Modify: `src/PdfParser.h`

- [ ] **Step 1: Define `HeaderRule` and `ParserConfig`**

Update `src/PdfParser.h` to include the necessary structures for header detection.

```cpp
#include <vector>
#include <regex>
#include <string>

struct HeaderRule {
    std::pair<double, double> size_range;
    std::string pattern;
    bool has_pattern = false;
    bool is_bold = false;
    bool check_bold = false;
    int level;
};

struct ParserConfig {
    std::vector<HeaderRule> rules;
    double footer_zone; // Proportion of page height (e.g., 0.88)
    double header_zone; // Proportion of page height (e.g., 0.08)
    double min_header_size = 9.0;
};
```

- [ ] **Step 2: Update `PdfParser` class interface**

```cpp
class PdfParser {
public:
    std::string extract_text(const std::string& filepath, const ParserConfig& config);
    static ParserConfig get_ieee_config();
};
```

- [ ] **Step 3: Commit**

```bash
git add src/PdfParser.h
git commit -m "feat: define header detection structures in PdfParser.h"
```

---

### Task 2: Implement IEEE Configuration and Helper Logic

**Files:**
- Modify: `src/PdfParser.cpp`

- [ ] **Step 1: Implement `get_ieee_config()`**

Add the implementation for `get_ieee_config()` to `src/PdfParser.cpp`.

```cpp
ParserConfig PdfParser::get_ieee_config() {
    ParserConfig config;
    config.header_zone = 0.08;
    config.footer_zone = 0.88;
    config.min_header_size = 9.0;

    // Level 1: size 10-14, bold, pattern ^\d+\.\s
    config.rules.push_back({{10.0, 14.0}, R"(^\d+\.\s)", true, true, true, 1});
    // Level 1: size 11.5-16, bold
    config.rules.push_back({{11.5, 16.0}, "", false, true, true, 1});
    // Level 2: size 10-14, pattern ^\d+\.\d+
    config.rules.push_back({{10.0, 14.0}, R"(^\d+\.\d+)", true, false, false, 2});
    // Level 3: size 9.5-12, pattern ^\d+\.\d+\.\d+
    config.rules.push_back({{9.5, 12.0}, R"(^\d+\.\d+\.\d+)", true, false, false, 3});

    return config;
}
```

- [ ] **Step 2: Commit**

```bash
git add src/PdfParser.cpp
git commit -m "feat: implement get_ieee_config in PdfParser.cpp"
```

---

### Task 3: Implement Line Grouping and Markdown Extraction

**Files:**
- Modify: `src/PdfParser.cpp`

- [ ] **Step 1: Update `extract_text` to use `text_list()`**

Modify `extract_text` to iterate over pages and use `page->text_list(poppler::page::text_list_include_font)`.

- [ ] **Step 2: Implement word grouping into lines**

Group `text_box`es into lines based on their `bbox().top()` coordinate. Use a small threshold (e.g., 3.0) for vertical alignment.

- [ ] **Step 3: Implement header detection for each line**

For each line:
1. Check if the line is in the header or footer zone.
2. If it's a footer and contains "Copyright" or only digits, skip it.
3. If it's a header and the font size is small, skip it.
4. Iterate through `config.rules` and check if the line matches any rule.
   - Bold check: Look for "bold" in the font name.
   - Regex check: Match the line text against the rule's pattern.
5. If a rule matches, prepend `#` (e.g., `## `) to the line.

- [ ] **Step 4: Commit**

```bash
git add src/PdfParser.cpp
git commit -m "feat: implement line grouping and header detection in extract_text"
```

---

### Task 4: Verify with IEEE PDF

**Files:**
- Modify: `tests/PdfParser_test.cpp`

- [ ] **Step 1: Update existing tests and add new one**

Update `ExtractTextFromRealPdf` to use the new `extract_text` signature.

```cpp
TEST(PdfParserTest, DetectsIEEEHeaders) {
    PdfParser parser;
    auto config = PdfParser::get_ieee_config();
    std::string pdf_path = "/Users/alfred/work/pyapi/src/rag/public/IEEE1584-2018-31-36.pdf";
    std::string text = parser.extract_text(pdf_path, config);
    
    // Check for some expected headers in that PDF
    EXPECT_NE(text.find("# 4."), std::string::npos);
    EXPECT_NE(text.find("## 4.1"), std::string::npos);
}
```

- [ ] **Step 2: Run tests**

Run: `cd build && make && ./injestor_tests`

- [ ] **Step 3: Commit**

```bash
git add tests/PdfParser_test.cpp
git commit -m "test: add header detection test for IEEE PDF"
```
