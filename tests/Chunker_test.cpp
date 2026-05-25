#include <gtest/gtest.h>
#include "../src/Chunker.h"

TEST(ChunkerTest, SplitByHeaders) {
    Chunker chunker;
    std::string markdown = "# H1\nContent 1\n## H2\nContent 2";
    auto chunks = chunker.chunk(markdown, "doc1");
    
    ASSERT_EQ(chunks.size(), 2);
    EXPECT_EQ(chunks[0].metadata["Header 1"], "H1");
    EXPECT_EQ(chunks[0].content, "Content 1");
    EXPECT_EQ(chunks[1].metadata["Header 1"], "H1");
    EXPECT_EQ(chunks[1].metadata["Header 2"], "H2");
    EXPECT_EQ(chunks[1].content, "Content 2");
}

TEST(ChunkerTest, SplitRecursively) {
    Chunker chunker(10, 2); // Small size for testing
    std::string text = "This is a long sentence that should be split.";
    auto chunks = chunker.chunk(text, "doc1");
    
    EXPECT_GT(chunks.size(), 1);
    for (const auto& c : chunks) {
        EXPECT_LE(c.content.size(), 10);
    }
}
