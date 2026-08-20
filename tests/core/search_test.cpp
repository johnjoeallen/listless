#include "Search.hpp"

#include <gtest/gtest.h>

using listless::HorspoolSearcher;

TEST(HorspoolSearcher, FindsSimpleMatch) {
    HorspoolSearcher s("world");
    EXPECT_EQ(s.find("hello world"), 6u);
}

TEST(HorspoolSearcher, NoMatchReturnsNullopt) {
    HorspoolSearcher s("xyz");
    EXPECT_EQ(s.find("hello world"), std::nullopt);
}

TEST(HorspoolSearcher, EmptyPatternNeverMatches) {
    HorspoolSearcher s("");
    EXPECT_EQ(s.find("hello"), std::nullopt);
    EXPECT_EQ(s.find(""), std::nullopt);
}

TEST(HorspoolSearcher, SingleCharacterPattern) {
    HorspoolSearcher s("o");
    EXPECT_EQ(s.find("hello world"), 4u);
}

TEST(HorspoolSearcher, PatternLongerThanHaystackNeverMatches) {
    HorspoolSearcher s("a very long pattern");
    EXPECT_EQ(s.find("short"), std::nullopt);
}

TEST(HorspoolSearcher, MatchAtVeryStartAndEnd) {
    HorspoolSearcher s("ab");
    EXPECT_EQ(s.find("abxyz"), 0u);
    EXPECT_EQ(s.find("xyzab"), 3u);
}

TEST(HorspoolSearcher, StartOffsetSkipsEarlierMatches) {
    HorspoolSearcher s("ab");
    EXPECT_EQ(s.find("ababab", 0), 0u);
    EXPECT_EQ(s.find("ababab", 1), 2u);
    EXPECT_EQ(s.find("ababab", 5), std::nullopt);
}

TEST(HorspoolSearcher, RepeatedCharactersInPattern) {
    HorspoolSearcher s("aaab");
    EXPECT_EQ(s.find("aaaaab"), 2u);
}

TEST(HorspoolSearcher, CaseSensitiveByDefault) {
    HorspoolSearcher s("World");
    EXPECT_EQ(s.find("hello world"), std::nullopt);
}

TEST(HorspoolSearcher, CaseInsensitiveOptIn) {
    HorspoolSearcher s("World", /*case_sensitive=*/false);
    EXPECT_EQ(s.find("hello world"), 6u);
}
