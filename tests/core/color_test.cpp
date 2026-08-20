#include "Color.hpp"

#include <gtest/gtest.h>

using listless::Color;
using listless::to_ansi;

struct ColorCase {
    Color color;
    int expected_base;
    bool expected_bright;
};

class ToAnsiTest : public ::testing::TestWithParam<ColorCase> {};

TEST_P(ToAnsiTest, MapsToExpectedAnsiBaseAndBrightness) {
    auto ansi = to_ansi(GetParam().color);
    EXPECT_EQ(ansi.base, GetParam().expected_base);
    EXPECT_EQ(ansi.bright, GetParam().expected_bright);
}

INSTANTIATE_TEST_SUITE_P(
    DosPalette, ToAnsiTest,
    ::testing::Values(ColorCase{Color::Black, 0, false}, ColorCase{Color::Blue, 4, false},
                      ColorCase{Color::Green, 2, false}, ColorCase{Color::Cyan, 6, false},
                      ColorCase{Color::Red, 1, false}, ColorCase{Color::Magenta, 5, false},
                      ColorCase{Color::Brown, 3, false}, ColorCase{Color::LightGray, 7, false},
                      ColorCase{Color::DarkGray, 0, true}, ColorCase{Color::LightBlue, 4, true},
                      ColorCase{Color::LightGreen, 2, true}, ColorCase{Color::LightCyan, 6, true},
                      ColorCase{Color::LightRed, 1, true}, ColorCase{Color::LightMagenta, 5, true},
                      ColorCase{Color::Yellow, 3, true}, ColorCase{Color::White, 7, true}));
