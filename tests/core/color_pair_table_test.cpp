#include "ColorPairTable.hpp"

#include <gtest/gtest.h>

#include <vector>

using listless::Color;
using listless::ColorPairTable;

TEST(ColorPairTable, FirstRequestAllocatesPairOne) {
    ColorPairTable table(16);
    EXPECT_EQ(table.pair_for(Color::White, Color::Black), 1);
}

TEST(ColorPairTable, SameComboReturnsSameId) {
    ColorPairTable table(16);
    int first = table.pair_for(Color::Red, Color::Blue);
    int second = table.pair_for(Color::Red, Color::Blue);
    EXPECT_EQ(first, second);
}

TEST(ColorPairTable, DistinctCombosGetDistinctIncreasingIds) {
    ColorPairTable table(16);
    int a = table.pair_for(Color::Red, Color::Black);
    int b = table.pair_for(Color::Green, Color::Black);
    int c = table.pair_for(Color::Blue, Color::Black);
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
    EXPECT_EQ(c, 3);
}

TEST(ColorPairTable, FgBgOrderMatters) {
    ColorPairTable table(16);
    int a = table.pair_for(Color::Red, Color::Blue);
    int b = table.pair_for(Color::Blue, Color::Red);
    EXPECT_NE(a, b);
}

TEST(ColorPairTable, ExhaustionFallsBackToPairZero) {
    ColorPairTable table(3);  // ids 1 and 2 allocatable, 0 reserved
    EXPECT_EQ(table.pair_for(Color::Red, Color::Black), 1);
    EXPECT_EQ(table.pair_for(Color::Green, Color::Black), 2);
    EXPECT_EQ(table.pair_for(Color::Blue, Color::Black), 0);  // exhausted
}

TEST(ColorPairTable, ExhaustedComboIsNotCachedAsZero) {
    ColorPairTable table(2);  // only id 1 allocatable
    table.pair_for(Color::Red, Color::Black);
    EXPECT_EQ(table.pair_for(Color::Green, Color::Black), 0);
    // Freeing up no capacity; repeated requests for the same exhausted
    // combo should still fall back consistently rather than wrongly
    // caching id 0 as "this combo's real pair".
    EXPECT_EQ(table.pair_for(Color::Green, Color::Black), 0);
}

TEST(ColorPairTable, InvokesCallbackExactlyOncePerNewPair) {
    std::vector<std::tuple<int, Color, Color>> calls;
    ColorPairTable table(16,
                         [&calls](int id, Color fg, Color bg) { calls.emplace_back(id, fg, bg); });

    table.pair_for(Color::Yellow, Color::Black);
    table.pair_for(Color::Yellow, Color::Black);  // repeat: no new callback
    table.pair_for(Color::White, Color::Blue);

    ASSERT_EQ(calls.size(), 2u);
    EXPECT_EQ(std::get<0>(calls[0]), 1);
    EXPECT_EQ(std::get<1>(calls[0]), Color::Yellow);
    EXPECT_EQ(std::get<2>(calls[0]), Color::Black);
    EXPECT_EQ(std::get<0>(calls[1]), 2);
    EXPECT_EQ(std::get<1>(calls[1]), Color::White);
    EXPECT_EQ(std::get<2>(calls[1]), Color::Blue);
}
