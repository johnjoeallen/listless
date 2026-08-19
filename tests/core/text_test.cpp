#include "text.hpp"

#include <gtest/gtest.h>

TEST(CompareIgnoreCase, EqualIgnoringCase) {
    EXPECT_EQ(listless::compare_ignore_case("Hello", "hello"), 0);
    EXPECT_EQ(listless::compare_ignore_case("", ""), 0);
}

TEST(CompareIgnoreCase, OrdersLikeStrcmp) {
    EXPECT_LT(listless::compare_ignore_case("abc", "abd"), 0);
    EXPECT_GT(listless::compare_ignore_case("abd", "abc"), 0);
}

TEST(CompareIgnoreCase, ShorterPrefixSortsFirst) {
    EXPECT_LT(listless::compare_ignore_case("ab", "abc"), 0);
    EXPECT_GT(listless::compare_ignore_case("abc", "ab"), 0);
}

TEST(Trim, StripsLeadingAndTrailingWhitespace) {
    EXPECT_EQ(listless::trim("  hello  "), "hello");
    EXPECT_EQ(listless::trim("\thello\n"), "hello");
}

TEST(Trim, PreservesInteriorWhitespace) {
    EXPECT_EQ(listless::trim("  hello world  "), "hello world");
}

TEST(Trim, EmptyAndAllWhitespaceYieldEmpty) {
    EXPECT_EQ(listless::trim(""), "");
    EXPECT_EQ(listless::trim("   "), "");
}
