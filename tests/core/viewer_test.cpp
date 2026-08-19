#include "viewer.hpp"

#include <gtest/gtest.h>

#include <fstream>

namespace fs = std::filesystem;
using listless::BookmarkAction;
using listless::kWholeLine;
using listless::Viewer;

namespace {

Viewer make_viewer(std::string content) { return Viewer(std::move(content), "test"); }

}  // namespace

// --- construction / line model -------------------------------------------

TEST(Viewer, ConstructsFromInMemoryContentWithDisplayName) {
    Viewer v = make_viewer("a\nb\nc\n");
    EXPECT_EQ(v.display_name(), "test");
    EXPECT_TRUE(v.path().empty());
    EXPECT_EQ(v.line_count(), 3);
}

TEST(Viewer, SplitsOnLfAndCrlf) {
    Viewer v = make_viewer("one\r\ntwo\nthree");
    ASSERT_EQ(v.line_count(), 3);
    EXPECT_EQ(v.line_text(0), "one");
    EXPECT_EQ(v.line_text(1), "two");
    EXPECT_EQ(v.line_text(2), "three");  // no trailing newline still counted
}

TEST(Viewer, TrailingNewlineDoesNotProduceExtraEmptyLine) {
    Viewer v = make_viewer("a\nb\n");
    EXPECT_EQ(v.line_count(), 2);
}

TEST(Viewer, EmptyContentHasZeroLines) {
    Viewer v = make_viewer("");
    EXPECT_EQ(v.line_count(), 0);
}

TEST(Viewer, DetectsBinaryContent) {
    Viewer text = make_viewer("hello\nworld\n");
    Viewer binary = make_viewer(std::string("hello\0world", 11));
    EXPECT_FALSE(text.is_binary());
    EXPECT_TRUE(binary.is_binary());
}

TEST(Viewer, ConstructsFromRealFile) {
    fs::path path = fs::temp_directory_path() / "listless_viewer_test_file.txt";
    {
        std::ofstream out(path);
        out << "line1\nline2\n";
    }

    Viewer v(path);
    EXPECT_EQ(v.path(), path);
    EXPECT_EQ(v.line_count(), 2);
    EXPECT_EQ(v.line_text(0), "line1");

    fs::remove(path);
}

TEST(Viewer, ThrowsOnMissingFile) {
    EXPECT_THROW(Viewer(fs::path("/nonexistent/definitely/not/here.txt")), std::runtime_error);
}

// --- word wrap -------------------------------------------------------------

TEST(Viewer, WordWrapSplitsAtLastSpaceBeforeWidth) {
    Viewer v = make_viewer("the quick brown fox jumps");
    v.set_word_wrap(true, 10);
    ASSERT_GT(v.line_count(), 1);
    // "the quick " is 10 chars including trailing space consumed by the break
    EXPECT_EQ(v.line_text(0), "the quick ");
}

TEST(Viewer, WordWrapHardBreaksWithNoSpace) {
    Viewer v = make_viewer("abcdefghijklmnop");
    v.set_word_wrap(true, 5);
    ASSERT_GT(v.line_count(), 1);
    EXPECT_EQ(v.line_text(0), "abcde");
}

TEST(Viewer, DisablingWordWrapRestoresOriginalLines) {
    Viewer v = make_viewer("the quick brown fox\nsecond line");
    v.set_word_wrap(true, 10);
    ASSERT_NE(v.line_count(), 2);
    v.set_word_wrap(false, 10);
    ASSERT_EQ(v.line_count(), 2);
    EXPECT_EQ(v.line_text(0), "the quick brown fox");
    EXPECT_EQ(v.line_text(1), "second line");
}

// --- scrolling ---------------------------------------------------------

TEST(Viewer, ScrollLineDownAndUp) {
    Viewer v = make_viewer("1\n2\n3\n4\n5\n");  // 5 lines
    EXPECT_TRUE(v.scroll_line_down(3));
    EXPECT_EQ(v.top_line(), 1);
    EXPECT_TRUE(v.scroll_line_up());
    EXPECT_EQ(v.top_line(), 0);
    EXPECT_FALSE(v.scroll_line_up());  // already at top
}

TEST(Viewer, ScrollLineDownClampedAtLastFullPage) {
    Viewer v = make_viewer("1\n2\n3\n4\n5\n");  // 5 lines
    for (int i = 0; i < 10; ++i) {
        v.scroll_line_down(3);
    }
    EXPECT_EQ(v.top_line(), 2);  // last position where 3 lines still fill the page
    EXPECT_FALSE(v.scroll_line_down(3));
}

TEST(Viewer, ScrollPageDownAndUp) {
    Viewer v = make_viewer("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n");  // 10 lines
    EXPECT_TRUE(v.scroll_page_down(4));
    EXPECT_EQ(v.top_line(), 4);
    EXPECT_TRUE(v.scroll_page_up(4));
    EXPECT_EQ(v.top_line(), 0);
    EXPECT_FALSE(v.scroll_page_up(4));  // already at top
}

TEST(Viewer, ScrollToTopAndBottom) {
    Viewer v = make_viewer("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n");
    EXPECT_TRUE(v.scroll_to_bottom(4));
    EXPECT_EQ(v.top_line(), 6);
    EXPECT_FALSE(v.scroll_to_bottom(4));  // already there
    EXPECT_TRUE(v.scroll_to_top());
    EXPECT_EQ(v.top_line(), 0);
}

TEST(Viewer, HorizontalScrollStepsByTenAndClampsAtZero) {
    Viewer v = make_viewer("just one line\n");
    EXPECT_TRUE(v.scroll_right(80));
    EXPECT_EQ(v.column(), 10);
    EXPECT_TRUE(v.scroll_left());
    EXPECT_EQ(v.column(), 0);
    EXPECT_FALSE(v.scroll_left());
}

TEST(Viewer, HorizontalScrollCapsAtLineBufMax) {
    Viewer v = make_viewer("x\n");
    for (int i = 0; i < 200; ++i) {
        v.scroll_right(80);
    }
    EXPECT_LE(v.column(), 1024 - (80 + 10));
}

TEST(Viewer, ResetHorizontalScroll) {
    Viewer v = make_viewer("x\n");
    v.scroll_right(80);
    EXPECT_TRUE(v.reset_horizontal_scroll());
    EXPECT_EQ(v.column(), 0);
    EXPECT_FALSE(v.reset_horizontal_scroll());
}

// --- search ------------------------------------------------------------

TEST(Viewer, SearchForwardFindsMatchOnLaterLine) {
    Viewer v = make_viewer("alpha\nbeta\ngamma\n");
    EXPECT_TRUE(v.search_forward("gamma", true));
    EXPECT_EQ(v.selection().line, 2);
    EXPECT_EQ(v.selection().pos, 0u);
    EXPECT_EQ(v.selection().count, 5u);
}

TEST(Viewer, SearchForwardNoMatchClearsSelection) {
    Viewer v = make_viewer("alpha\nbeta\n");
    EXPECT_FALSE(v.search_forward("zzz", true));
    EXPECT_EQ(v.selection().line, -1);
}

TEST(Viewer, SearchIsCaseSensitiveByDefault) {
    Viewer v = make_viewer("Alpha\n");
    EXPECT_FALSE(v.search_forward("alpha", true));
    EXPECT_TRUE(v.search_forward("alpha", false));
}

TEST(Viewer, SearchBackwardScansFromTopLineDownward) {
    Viewer v = make_viewer("foo\nbar\nfoo\n");
    v.goto_line(2);
    EXPECT_TRUE(v.search_backward("foo", true));
    EXPECT_EQ(v.selection().line, 2);  // top_line's own line matches first
}

TEST(Viewer, RepeatSearchForwardContinuesOnSameLinePastCurrentMatch) {
    Viewer v = make_viewer("aa aa aa\n");
    ASSERT_TRUE(v.search_forward("aa", true));
    EXPECT_EQ(v.selection().pos, 0u);
    ASSERT_TRUE(v.repeat_search(true));
    EXPECT_EQ(v.selection().line, 0);
    EXPECT_EQ(v.selection().pos, 3u);
    ASSERT_TRUE(v.repeat_search(true));
    EXPECT_EQ(v.selection().pos, 6u);
}

TEST(Viewer, RepeatSearchForwardMovesToNextLineWhenNoMoreOnCurrentLine) {
    Viewer v = make_viewer("aa\nbb\naa\n");
    ASSERT_TRUE(v.search_forward("aa", true));
    EXPECT_EQ(v.selection().line, 0);
    ASSERT_TRUE(v.repeat_search(true));
    EXPECT_EQ(v.selection().line, 2);
}

TEST(Viewer, RepeatSearchBackwardFindsEarlierOccurrenceOnSameLine) {
    Viewer v = make_viewer("aa aa aa\n");
    v.goto_line(0);
    // Position selection at the last occurrence directly via a forward
    // search then repeats forward to reach it, then repeat backward.
    ASSERT_TRUE(v.search_forward("aa", true));
    ASSERT_TRUE(v.repeat_search(true));
    ASSERT_TRUE(v.repeat_search(true));
    EXPECT_EQ(v.selection().pos, 6u);
    ASSERT_TRUE(v.repeat_search(false));
    EXPECT_EQ(v.selection().pos, 3u);
}

TEST(Viewer, RepeatSearchWithoutPriorSearchReturnsFalse) {
    Viewer v = make_viewer("aa\n");
    EXPECT_FALSE(v.repeat_search(true));
}

TEST(Viewer, ClearSelection) {
    Viewer v = make_viewer("aa\n");
    v.search_forward("aa", true);
    ASSERT_NE(v.selection().line, -1);
    v.clear_selection();
    EXPECT_EQ(v.selection().line, -1);
}

// --- goto line -----------------------------------------------------------

TEST(Viewer, GotoLineSetsTopLineAndWholeLineSelection) {
    Viewer v = make_viewer("1\n2\n3\n4\n5\n");
    v.goto_line(3);
    EXPECT_EQ(v.top_line(), 3);
    EXPECT_EQ(v.selection().line, 3);
    EXPECT_EQ(v.selection().count, kWholeLine);
}

TEST(Viewer, GotoLineClampsToValidRange) {
    Viewer v = make_viewer("1\n2\n3\n");
    v.goto_line(100);
    EXPECT_EQ(v.top_line(), 2);
    v.goto_line(-5);
    EXPECT_EQ(v.top_line(), 0);
}

// --- viewport adjustment for selection ------------------------------------

TEST(Viewer, EnsureSelectionVisibleScrollsToShowOffscreenMatch) {
    Viewer v = make_viewer("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n");
    v.search_forward("9", true);
    ASSERT_EQ(v.selection().line, 8);
    EXPECT_TRUE(v.ensure_selection_visible(4, 80));
    EXPECT_EQ(v.top_line(), 8);
}

TEST(Viewer, EnsureSelectionVisibleNoopWhenAlreadyVisible) {
    Viewer v = make_viewer("1\n2\n3\n4\n5\n");
    v.search_forward("1", true);
    EXPECT_FALSE(v.ensure_selection_visible(4, 80));
}

TEST(Viewer, EnsureSelectionVisibleNoopWithoutSelection) {
    Viewer v = make_viewer("1\n2\n3\n");
    EXPECT_FALSE(v.ensure_selection_visible(4, 80));
}

// --- bookmarks -----------------------------------------------------------

TEST(Viewer, SetBookmarkTogglesSetClearedReset) {
    Viewer v = make_viewer("1\n2\n3\n4\n5\n");
    EXPECT_EQ(v.bookmark(0).line, -1);

    v.goto_line(2);
    EXPECT_EQ(v.set_bookmark(0), BookmarkAction::Set);
    EXPECT_EQ(v.bookmark(0).line, 2);

    EXPECT_EQ(v.set_bookmark(0), BookmarkAction::Cleared);
    EXPECT_EQ(v.bookmark(0).line, -1);

    v.goto_line(2);
    v.set_bookmark(0);
    v.goto_line(4);
    EXPECT_EQ(v.set_bookmark(0), BookmarkAction::Reset);
    EXPECT_EQ(v.bookmark(0).line, 4);
}

TEST(Viewer, JumpToBookmarkRestoresPosition) {
    Viewer v = make_viewer("1\n2\n3\n4\n5\n");
    v.goto_line(3);
    v.scroll_right(80);
    int saved_column = v.column();
    v.set_bookmark(5);

    v.scroll_to_top();
    v.reset_horizontal_scroll();

    EXPECT_TRUE(v.jump_to_bookmark(5));
    EXPECT_EQ(v.top_line(), 3);
    EXPECT_EQ(v.column(), saved_column);
}

TEST(Viewer, JumpToUnsetBookmarkReturnsFalse) {
    Viewer v = make_viewer("1\n2\n3\n");
    EXPECT_FALSE(v.jump_to_bookmark(7));
}

TEST(Viewer, BookmarksAreIndependentPerSlot) {
    Viewer v = make_viewer("1\n2\n3\n4\n5\n");
    v.goto_line(1);
    v.set_bookmark(0);
    v.goto_line(4);
    v.set_bookmark(1);

    EXPECT_EQ(v.bookmark(0).line, 1);
    EXPECT_EQ(v.bookmark(1).line, 4);
}
