#include "key.hpp"

#include <gtest/gtest.h>

using namespace listless;

TEST(AltKey, LettersMatchOriginalVkaltTable) {
    EXPECT_EQ(alt_key('A'), 0xFF1E);
    EXPECT_EQ(alt_key('N'), 0xFF31);
    EXPECT_EQ(alt_key('O'), 0xFF18);
    EXPECT_EQ(alt_key('P'), 0xFF19);
    EXPECT_EQ(alt_key('Q'), 0xFF10);
    EXPECT_EQ(alt_key('V'), 0xFF2F);
    EXPECT_EQ(alt_key('X'), 0xFF2D);
    EXPECT_EQ(alt_key('Z'), 0xFF2C);
}

TEST(AltKey, LowercaseMatchesUppercase) {
    EXPECT_EQ(alt_key('a'), alt_key('A'));
    EXPECT_EQ(alt_key('n'), alt_key('N'));
}

TEST(AltKey, DigitsMatchOriginalVkaltTable) {
    EXPECT_EQ(alt_key('1'), 0xFF78);
    EXPECT_EQ(alt_key('9'), 0xFF80);
}

TEST(AltKey, ZeroAndOtherCharactersAreUnknown) {
    EXPECT_EQ(alt_key('0'), Key::Unknown);
    EXPECT_EQ(alt_key(' '), Key::Unknown);
    EXPECT_EQ(alt_key('!'), Key::Unknown);
}

TEST(Key, ExtendedConstantsMatchOriginalLiterals) {
    // Cross-checked directly against os.cpp's literal case labels.
    EXPECT_EQ(Key::F1, 0xFF3B);
    EXPECT_EQ(Key::F2, 0xFF3C);
    EXPECT_EQ(Key::Home, 0xFF47);
    EXPECT_EQ(Key::Up, 0xFF48);
    EXPECT_EQ(Key::PageUp, 0xFF49);
    EXPECT_EQ(Key::Left, 0xFF4B);
    EXPECT_EQ(Key::Right, 0xFF4D);
    EXPECT_EQ(Key::End, 0xFF4F);
    EXPECT_EQ(Key::Down, 0xFF50);
    EXPECT_EQ(Key::PageDown, 0xFF51);
    EXPECT_EQ(Key::Insert, 0xFF52);
    EXPECT_EQ(Key::Delete, 0xFF53);
    EXPECT_EQ(Key::ShiftTab, 0xFF0F);
    EXPECT_EQ(Key::F11, 0xFF85);
    EXPECT_EQ(Key::F12, 0xFF86);
}
