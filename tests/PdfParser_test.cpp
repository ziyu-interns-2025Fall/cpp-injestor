#include <gtest/gtest.h>
#include "../src/PdfParser.h"
#include <fstream>

TEST(PdfParserTest, ExtractTextFromRealPdf) {
    PdfParser parser;
    auto config = PdfParser::get_ieee_config();
    std::string pdf_path = "/Users/alfred/work/pyapi/src/rag/public/IEEE1584-2018-31-36.pdf";
    std::string text = parser.extract_text(pdf_path, config);
    EXPECT_FALSE(text.empty());
    EXPECT_NE(text.find("IEEE"), std::string::npos);
}

TEST(PdfParserTest, HandlesNonExistentFile) {
    PdfParser parser;
    auto config = PdfParser::get_ieee_config();
    std::string text = parser.extract_text("/nonexistent.pdf", config);
    EXPECT_EQ(text, "");
}

TEST(PdfParserTest, DetectsIEEEHeaders) {
    PdfParser parser;
    auto config = PdfParser::get_ieee_config();
    std::string pdf_path = "/Users/alfred/work/pyapi/src/rag/public/IEEE1584-2018-31-36.pdf";
    std::string text = parser.extract_text(pdf_path, config);
    
    // Check for some expected headers in that PDF (Markdown formatted)
    EXPECT_NE(text.find("# 4."), std::string::npos);
    EXPECT_NE(text.find("## 4.1"), std::string::npos);
}
