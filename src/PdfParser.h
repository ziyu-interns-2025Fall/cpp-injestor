#pragma once
#include <string>
#include <vector>
#include <utility>

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

class PdfParser {
public:
    auto extract_text(const std::string& filepath, const ParserConfig& config) -> std::string;
    static auto get_ieee_config() -> ParserConfig;
    static auto get_generic_config() -> ParserConfig;
};
