#include "PdfParser.h"
#include <poppler-document.h>
#include <poppler-page.h>
#include <algorithm>
#include <cmath>
#include <regex>

auto PdfParser::get_ieee_config() -> ParserConfig {
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

auto PdfParser::get_generic_config() -> ParserConfig {
    ParserConfig config;
    config.header_zone = 0.05;
    config.footer_zone = 0.95;
    config.min_header_size = 10.0;

    // A generic rule: anything size 14+ is level 1
    config.rules.push_back({{14.0, 100.0}, "", false, false, false, 1});
    // size 12-14 is level 2
    config.rules.push_back({{12.0, 13.99}, "", false, false, false, 2});

    return config;
}

auto PdfParser::extract_text(const std::string& filepath, const ParserConfig& config) -> std::string {
    auto doc = poppler::document::load_from_file(filepath);
    if (!doc) {
        return "";
    }

    std::string full_text;
    for (int i = 0; i < doc->pages(); ++i) {
        auto *page = doc->create_page(i);
        if (!page) continue;

        double page_height = page->page_rect().height();
        auto boxes = page->text_list(poppler::page::text_list_include_font);

        if (boxes.empty()) continue;

        // Group into lines - using indices to handle move-only poppler::text_box
        std::vector<std::vector<poppler::text_box>> lines;
        for (auto& box : boxes) {
            bool placed = false;
            for (auto& line : lines) {
                if (std::abs(line[0].bbox().top() - box.bbox().top()) < 3.0) {
                    line.push_back(std::move(box));
                    placed = true;
                    break;
                }
            }
            if (!placed) {
                std::vector<poppler::text_box> new_line;
                new_line.push_back(std::move(box));
                lines.push_back(std::move(new_line));
            }
        }

        // Sort lines by vertical position
        std::sort(lines.begin(), lines.end(), [](const std::vector<poppler::text_box>& a, const std::vector<poppler::text_box>& b) {
            return a[0].bbox().top() < b[0].bbox().top();
        });

        for (auto& line : lines) {
            // Sort boxes in line by horizontal position
            std::sort(line.begin(), line.end(), [](const poppler::text_box& a, const poppler::text_box& b) {
                return a.bbox().left() < b.bbox().left();
            });

            std::string line_text;
            double max_font_size = 0;
            bool is_bold = false;

            for (const auto& box : line) {
                if (!line_text.empty()) line_text += " ";
                auto utf8_bytes = box.text().to_utf8();
                line_text += std::string(utf8_bytes.begin(), utf8_bytes.end());

                max_font_size = std::max(max_font_size, box.get_font_size());

                std::string font_name = box.get_font_name();
                std::ranges::transform(font_name, font_name.begin(), ::tolower);
                if (font_name.find("bold") != std::string::npos) {
                    is_bold = true;
                }
            }

            double y_pos = line[0].bbox().top() / page_height;

            // Header/Footer zone check
            if (y_pos < config.header_zone) {
                if (max_font_size < config.min_header_size) continue;
            } else if (y_pos > config.footer_zone) {
                if (line_text.find("Copyright") != std::string::npos) continue;
                bool only_digits = true;
                for (char c : line_text) {
                    if (std::isdigit(c) || std::isspace(c)) continue;
                    only_digits = false;
                    break;
                }
                if (only_digits) continue;
            }

            // Header level detection
            int header_level = 0;
            for (const auto& rule : config.rules) {
                if (max_font_size >= rule.size_range.first && max_font_size <= rule.size_range.second) {
                    bool pattern_match = true;
                    if (rule.has_pattern) {
                        std::regex re(rule.pattern);
                        pattern_match = std::regex_search(line_text, re);
                    }

                    bool bold_match = true;
                    if (rule.check_bold) {
                        bold_match = (is_bold == rule.is_bold);
                    }

                    if (pattern_match && bold_match) {
                        header_level = rule.level;
                        break;
                    }
                }
            }

            if (header_level > 0) {
                full_text += std::string(header_level, '#') + " " + line_text + "\n";
            } else {
                full_text += line_text + "\n";
            }
        }
        full_text += "\n";
    }
    return full_text;
}
