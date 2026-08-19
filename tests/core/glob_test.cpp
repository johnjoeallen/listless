#include "glob.hpp"

#include <gtest/gtest.h>

using listless::glob_match;
using listless::glob_match_any;

TEST(GlobMatch, LiteralMatch) {
    EXPECT_TRUE(glob_match("hello", "hello"));
    EXPECT_FALSE(glob_match("hello", "hellx"));
    EXPECT_FALSE(glob_match("hello", "hell"));
}

TEST(GlobMatch, StarMatchesZeroOrMoreCharacters) {
    EXPECT_TRUE(glob_match("*.cpp", "main.cpp"));
    EXPECT_TRUE(glob_match("*.cpp", ".cpp"));
    EXPECT_FALSE(glob_match("*.cpp", "main.hpp"));
    EXPECT_TRUE(glob_match("a*b*c", "abc"));
    EXPECT_TRUE(glob_match("a*b*c", "aXXbYYc"));
    EXPECT_TRUE(glob_match("*", ""));
    EXPECT_TRUE(glob_match("*", "anything"));
}

TEST(GlobMatch, QuestionMarkMatchesExactlyOneCharacter) {
    EXPECT_TRUE(glob_match("a?c", "abc"));
    EXPECT_FALSE(glob_match("a?c", "ac"));
    EXPECT_FALSE(glob_match("a?c", "abbc"));
}

TEST(GlobMatch, DotMatchesLiterally) {
    EXPECT_TRUE(glob_match("a.b", "a.b"));
    EXPECT_FALSE(glob_match("a.b", "axb"));
}

TEST(GlobMatch, BracketCharacterSet) {
    EXPECT_TRUE(glob_match("[qa]", "q"));
    EXPECT_TRUE(glob_match("[qa]", "a"));
    EXPECT_FALSE(glob_match("[qa]", "z"));
}

TEST(GlobMatch, BracketRange) {
    EXPECT_TRUE(glob_match("[a-z]", "m"));
    EXPECT_FALSE(glob_match("[a-z]", "M"));
    EXPECT_FALSE(glob_match("[a-z]", "5"));
}

TEST(GlobMatch, UnterminatedBracketIsLiteral) {
    EXPECT_TRUE(glob_match("a[b", "a[b"));
    EXPECT_FALSE(glob_match("a[b", "axb"));
}

TEST(GlobMatch, ManPageExample) {
    // os.man: "xxx.[qa]*.xyz will match files beginning with xxx.
    // followed by a q or an a and zero of more occurrences of any
    // character followed by .xyz."
    EXPECT_TRUE(glob_match("xxx.[qa]*.xyz", "xxx.qFOO.xyz"));
    EXPECT_TRUE(glob_match("xxx.[qa]*.xyz", "xxx.a.xyz"));
    EXPECT_FALSE(glob_match("xxx.[qa]*.xyz", "xxx.bFOO.xyz"));
}

TEST(GlobMatch, CaseSensitiveByDefault) { EXPECT_FALSE(glob_match("*.CPP", "main.cpp")); }

TEST(GlobMatch, CaseInsensitiveOptIn) {
    EXPECT_TRUE(glob_match("*.CPP", "main.cpp", /*case_sensitive=*/false));
}

TEST(GlobMatchAny, MatchesAnySemicolonSeparatedPattern) {
    EXPECT_TRUE(glob_match_any("*.cpp;*.hpp", "main.cpp"));
    EXPECT_TRUE(glob_match_any("*.cpp;*.hpp", "main.hpp"));
    EXPECT_FALSE(glob_match_any("*.cpp;*.hpp", "main.txt"));
}

TEST(GlobMatchAny, SinglePatternNoSemicolon) { EXPECT_TRUE(glob_match_any("*.cpp", "main.cpp")); }
