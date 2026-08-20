#include "ViewerRender.hpp"

#include <gtest/gtest.h>
#include <ncurses.h>

#include <cstdlib>
#include <memory>
#include <string>

#include "Style.hpp"

using listless::Color;
using listless::HighlightCache;
using listless::render_viewer;
using listless::Style;
using listless::Terminal;
using listless::Viewer;

namespace {

class ViewerRenderTest : public ::testing::Test {
  protected:
    void SetUp() override {
        setenv("TERM", "xterm-256color", 1);
        terminal_ = std::make_unique<Terminal>();
    }

    void TearDown() override { terminal_.reset(); }

    std::string row_text(int row, int width) {
        std::string s;
        for (int x = 0; x < width; ++x) {
            s.push_back(static_cast<char>(mvinch(row, x) & A_CHARTEXT));
        }
        return s;
    }

    std::unique_ptr<Terminal> terminal_;
};

// A minimal style: reserved word "return" in Blue, everything else the
// default LightGray -- enough to check that render_viewer actually paints
// distinct colours per span without pulling in a whole language grammar.
// Style is move/copy-disabled (see style.hpp), so callers construct their
// own Style and pass it in rather than getting one back by value.
void init_reserved_word_style(Style& s) {
    s.syntax_highlight_enabled.set(true);
    s.add_reserved_word("return");
    s.reserved_color.set(Color::Blue);
    s.ident_color.set(Color::LightGray);
}

}  // namespace

TEST_F(ViewerRenderTest, DrawsVisibleLinesStartingBelowTheStatusLine) {
    Viewer v("alpha\nbeta\ngamma\n", "test.txt");
    render_viewer(v, *terminal_);

    std::string row1 = row_text(1, 5);
    std::string row2 = row_text(2, 4);
    EXPECT_EQ(row1, "alpha");
    EXPECT_EQ(row2, "beta");
}

TEST_F(ViewerRenderTest, StatusLineShowsDisplayNameAndPosition) {
    Viewer v("one\ntwo\nthree\n", "my-file.txt");
    render_viewer(v, *terminal_);

    std::string row0 = row_text(0, 40);
    EXPECT_NE(row0.find("my-file.txt"), std::string::npos);
    EXPECT_NE(row0.find("1/3"), std::string::npos);
}

TEST_F(ViewerRenderTest, ScrollingChangesWhichLinesAreDrawn) {
    Viewer v("a\nb\nc\nd\ne\n", "t");
    v.scroll_line_down(
        2);  // visible_lines is just a parameter here, independent of the real terminal
    render_viewer(v, *terminal_);

    EXPECT_EQ(row_text(1, 1), "b");
}

TEST_F(ViewerRenderTest, PastEndOfFileRowsAreBlank) {
    Viewer v("only-line\n", "t");
    render_viewer(v, *terminal_);

    std::string row2 = row_text(2, 5);
    EXPECT_EQ(row2, "     ");
}

TEST_F(ViewerRenderTest, SelectedMatchIsHighlightedWithDifferentAttributes) {
    Viewer v("hello world\n", "t");
    v.search_forward("world", true);
    render_viewer(v, *terminal_);

    chtype before = mvinch(1, 0);    // 'h', not selected
    chtype selected = mvinch(1, 6);  // 'w' of "world", selected
    EXPECT_NE((before & A_ATTRIBUTES), (selected & A_ATTRIBUTES));
}

TEST_F(ViewerRenderTest, GotoLineHighlightsWholeLine) {
    Viewer v("aaa\nbbb\nccc\n", "t");
    v.goto_line(1);
    render_viewer(v, *terminal_);

    // goto_line() moves top_line_ to the target, so it's drawn first.
    std::string top_row = row_text(1, 3);
    EXPECT_EQ(top_row, "bbb");

    chtype first = mvinch(1, 0);
    chtype last = mvinch(1, 2);
    EXPECT_EQ((first & A_ATTRIBUTES),
              (last & A_ATTRIBUTES));  // whole line, same highlight throughout
}

TEST_F(ViewerRenderTest, HexModeDrawsOffsetHexBytesAndAsciiGutter) {
    Viewer v("ABCDEFGHIJKLMNOP", "t");
    v.switch_to_hex_mode();
    render_viewer(v, *terminal_);

    std::string row1 = row_text(1, 80);
    EXPECT_NE(row1.find("00000000"), std::string::npos);
    EXPECT_NE(row1.find("41 42 43 44"), std::string::npos);  // 'A' 'B' 'C' 'D' in hex
    EXPECT_NE(row1.find("|ABCDEFGHIJKLMNOP|"), std::string::npos);
}

TEST_F(ViewerRenderTest, HexModeNonPrintableBytesShowAsDotInGutter) {
    std::string data;
    data.push_back('\x01');
    data.append(15, 'z');
    Viewer v(data, "t");
    v.switch_to_hex_mode();
    render_viewer(v, *terminal_);

    std::string row1 = row_text(1, 80);
    EXPECT_NE(row1.find("|.zzzzzzzzzzzzzzz|"), std::string::npos);
}

TEST_F(ViewerRenderTest, HexModeFinalPartialRowPadsMissingBytes) {
    Viewer v("ABC", "t");
    v.switch_to_hex_mode();
    render_viewer(v, *terminal_);

    std::string row1 = row_text(1, 80);
    EXPECT_NE(row1.find("41 42 43"), std::string::npos);
    EXPECT_NE(row1.find("|ABC             |"), std::string::npos);
}

TEST_F(ViewerRenderTest, HexModeScrollingChangesWhichRowIsDrawn) {
    Viewer v(std::string(160, 'x'), "t");  // 10 hex rows
    v.switch_to_hex_mode();
    v.hex_scroll_line_down(4);  // room to scroll: 10 rows > 4 visible
    render_viewer(v, *terminal_);

    std::string row1 = row_text(1, 80);
    EXPECT_NE(row1.find("00000010"), std::string::npos);  // row 1 now shows offset 0x10
}

TEST_F(ViewerRenderTest, TextModeUnaffectedByHexModeExistence) {
    Viewer v("plain text\n", "t");
    render_viewer(v, *terminal_);

    EXPECT_EQ(row_text(1, 10), "plain text");
}

TEST_F(ViewerRenderTest, SyntaxHighlightingPaintsDistinctSpansInDifferentColours) {
    Style style("Test");
    init_reserved_word_style(style);
    HighlightCache cache;

    Viewer v("return x\n", "t.test");
    render_viewer(v, *terminal_, style, cache);

    EXPECT_EQ(row_text(1, 8), "return x");
    chtype reserved = mvinch(1, 0);  // 'r' of "return", reserved_color (Blue)
    chtype ident = mvinch(1, 7);     // 'x', ident_color (LightGray)
    EXPECT_NE((reserved & A_ATTRIBUTES), (ident & A_ATTRIBUTES));
}

TEST_F(ViewerRenderTest, HighlightStateCarriesAcrossLinesViaCache) {
    // Neither line closes the comment it's in -- HighlightCache must
    // carry "in_comment" from line 0 into line 1 so its text is coloured
    // as a comment too, matching what a from-scratch scan would produce.
    Style style("Test");
    style.syntax_highlight_enabled.set(true);
    style.add_open_comment("/*");
    style.add_close_comment("*/");
    style.comment_color.set(Color::Green);
    style.ident_color.set(Color::LightGray);
    // highlight_line() only turns on syntax mode (vs. the plain layout
    // fallback) once at least one reserved word exists -- irrelevant to
    // this test's content, just needed to flip that switch.
    style.add_reserved_word("unrelated_keyword");

    HighlightCache cache_via_scroll;
    Viewer scrolled("/* start\nstill comment\n", "t.test");
    scrolled.scroll_line_down(1);
    render_viewer(scrolled, *terminal_, style, cache_via_scroll);
    chtype in_comment = mvinch(1, 0);  // 's' of "still", carried-over state

    HighlightCache cache_no_comment;
    Viewer no_comment("xx start\nstill comment\n", "t.test");
    no_comment.scroll_line_down(1);
    render_viewer(no_comment, *terminal_, style, cache_no_comment);
    chtype not_in_comment = mvinch(1, 0);  // same text, but never entered a comment

    EXPECT_NE((in_comment & A_ATTRIBUTES), (not_in_comment & A_ATTRIBUTES));
}

TEST_F(ViewerRenderTest, SelectionOverridesSyntaxColourOnlyWithinTheMatch) {
    Style style("Test");
    init_reserved_word_style(style);

    HighlightCache unselected_cache;
    Viewer unselected("return x\n", "t.test");
    render_viewer(unselected, *terminal_, style, unselected_cache);
    chtype reserved_unselected = mvinch(1, 0);  // 'r' of "return", syntax-coloured only

    HighlightCache cache;
    Viewer v("return x\n", "t.test");
    v.search_forward("x", true);
    render_viewer(v, *terminal_, style, cache);

    chtype reserved = mvinch(1, 0);  // 'r' of "return", still syntax-coloured
    chtype selected = mvinch(1, 7);  // 'x', now selection-highlighted

    EXPECT_NE((reserved & A_ATTRIBUTES), (selected & A_ATTRIBUTES));
    EXPECT_EQ((reserved & A_ATTRIBUTES), (reserved_unselected & A_ATTRIBUTES));
}

TEST_F(ViewerRenderTest, UnstyledOverloadStillDrawsLinesUncoloured) {
    Viewer v("plain\n", "t");
    render_viewer(v, *terminal_);  // 2-arg overload: no Style available

    EXPECT_EQ(row_text(1, 5), "plain");
}
