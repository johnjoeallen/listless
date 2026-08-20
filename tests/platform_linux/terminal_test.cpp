#include "Terminal.hpp"

#include <gtest/gtest.h>
#include <ncurses.h>

#include <cstdlib>
#include <string>

using listless::Color;
using listless::Terminal;

namespace {

// mvinch() reads from stdscr's virtual screen buffer directly, so these
// tests can verify what Terminal actually wrote without needing a real
// display or calling refresh()/doupdate().
class TerminalTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Force a known, colour-capable terminal type regardless of the
        // ambient environment (CI runners often have $TERM unset).
        setenv("TERM", "xterm-256color", 1);
        terminal_ = std::make_unique<Terminal>();
    }

    void TearDown() override { terminal_.reset(); }

    std::unique_ptr<Terminal> terminal_;
};

}  // namespace

TEST_F(TerminalTest, WidthAndHeightAreSanePositiveValues) {
    EXPECT_GT(terminal_->width(), 0);
    EXPECT_GT(terminal_->height(), 0);
}

TEST_F(TerminalTest, PutTextWritesCharactersReadableFromTheScreenBuffer) {
    terminal_->put_text(2, 1, "hi", Color::White, Color::Black);

    EXPECT_EQ(static_cast<char>(mvinch(1, 2) & A_CHARTEXT), 'h');
    EXPECT_EQ(static_cast<char>(mvinch(1, 3) & A_CHARTEXT), 'i');
}

TEST_F(TerminalTest, ClearToEolBlanksFromCursorToEndOfLineOnly) {
    terminal_->put_text(0, 0, "abcdef", Color::White, Color::Black);
    terminal_->clear_to_eol(3, 0, Color::White, Color::Black);

    EXPECT_EQ(static_cast<char>(mvinch(0, 0) & A_CHARTEXT), 'a');
    EXPECT_EQ(static_cast<char>(mvinch(0, 2) & A_CHARTEXT), 'c');
    EXPECT_EQ(static_cast<char>(mvinch(0, 3) & A_CHARTEXT), ' ');
    EXPECT_EQ(static_cast<char>(mvinch(0, 4) & A_CHARTEXT), ' ');
}

TEST_F(TerminalTest, ClearBlanksTheWholeScreen) {
    terminal_->put_text(0, 0, "hello", Color::White, Color::Black);
    terminal_->clear();

    EXPECT_EQ(static_cast<char>(mvinch(0, 0) & A_CHARTEXT), ' ');
}

TEST_F(TerminalTest, MoveCursorPositionsTheRealCursor) {
    terminal_->move_cursor(5, 2);

    EXPECT_EQ(getcurx(stdscr), 5);
    EXPECT_EQ(getcury(stdscr), 2);
}

TEST_F(TerminalTest, ScrollRegionShiftsContentUp) {
    terminal_->put_text(0, 0, "line0", Color::White, Color::Black);
    terminal_->put_text(0, 1, "line1", Color::White, Color::Black);
    terminal_->put_text(0, 2, "line2", Color::White, Color::Black);

    terminal_->scroll_region(0, 3, 1);  // scroll rows [0,3) up by 1

    EXPECT_EQ(static_cast<char>(mvinch(0, 0) & A_CHARTEXT), 'l');  // was line1's 'l'
    EXPECT_EQ(static_cast<char>(mvinch(1, 0) & A_CHARTEXT), 'l');  // was line2's 'l'
}

TEST_F(TerminalTest, ConstructingASecondTerminalAfterTheFirstIsDestroyedWorks) {
    terminal_.reset();
    Terminal second;
    EXPECT_GT(second.width(), 0);
}
