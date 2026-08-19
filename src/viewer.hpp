#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace listless {

// A span of bytes within Viewer's internal buffer -- one line.
struct LineSpan {
    std::size_t offset = 0;
    std::size_t length = 0;
};

// A saved (line, column) position. line == -1 means unset.
struct Bookmark {
    int line = -1;
    int column = 0;
};

// Sentinel for Selection::count meaning "highlight the whole line"
// (used by goto_line()), matching the original's iSelectedCount == -1.
inline constexpr std::size_t kWholeLine = static_cast<std::size_t>(-1);

// The current search/goto-line highlight. line == -1 means no selection.
struct Selection {
    int line = -1;
    std::size_t pos = 0;
    std::size_t count = 0;
};

enum class BookmarkAction { Set, Cleared, Reset };

// Pure logic and state for viewing one loaded text file: line model,
// scrolling, word wrap, search, bookmarks. No terminal/rendering
// dependency -- see viewer_render.hpp for that. See
// docs/07-file-viewer-core.md for what's ported, narrowed, or deferred
// relative to the original's Viewer (osview.cpp).
class Viewer {
  public:
    // Loads and indexes the file at `path`. Throws std::runtime_error if
    // it can't be opened or read.
    explicit Viewer(const std::filesystem::path& path);

    // Constructs from already-read content (e.g. piped stdin) with a
    // display name for the status line; `path()` is empty.
    Viewer(std::string content, std::string display_name);

    const std::filesystem::path& path() const { return path_; }
    const std::string& display_name() const { return display_name_; }
    bool is_binary() const { return binary_; }

    int line_count() const { return static_cast<int>(lines_.size()); }
    std::string_view line_text(int line) const;

    bool word_wrap() const { return word_wrap_; }
    // Enabling re-splits the line index at word boundaries to fit
    // `width`; disabling restores the original (unwrapped) line spans.
    void set_word_wrap(bool enabled, int width);

    int top_line() const { return top_line_; }
    int column() const { return column_; }

    // All scroll_*() return true if the position actually changed.
    bool scroll_to_top();
    bool scroll_to_bottom(int visible_lines);
    bool scroll_page_down(int visible_lines);
    bool scroll_page_up(int visible_lines);
    bool scroll_line_down(int visible_lines);
    bool scroll_line_up();
    bool scroll_right(int visible_width);
    bool scroll_left();
    bool reset_horizontal_scroll();

    // Starts a new search from top_line(), scanning forward (to the
    // last line) or backward (to line 0). Returns true on a match.
    bool search_forward(std::string_view pattern, bool case_sensitive);
    bool search_backward(std::string_view pattern, bool case_sensitive);

    // Repeats the last search (either direction), first trying to
    // continue past the current match on the same line before scanning
    // subsequent/preceding lines. Returns false if there's no previous
    // search or no match is found.
    bool repeat_search(bool forward);

    void clear_selection();
    const Selection& selection() const { return selection_; }

    // Repositions the viewport (top_line()/column()) so the current
    // selection is visible, if one exists. Returns true if the
    // viewport moved. Separate from search_*() so "was a match found"
    // and "how do we scroll to show it" stay independently testable.
    bool ensure_selection_visible(int visible_lines, int visible_width);

    // Repositions the viewport so `line` is at the top, with the whole
    // line highlighted. Clamped to a valid line index.
    void goto_line(int line);

    // slot is 0-9. Toggles: unset -> Set at the current position;
    // set at the current position -> Cleared; set elsewhere -> Reset to
    // the current position.
    BookmarkAction set_bookmark(int slot);

    // Jumps the viewport to the bookmark, if set. Returns false if the
    // slot is unset.
    bool jump_to_bookmark(int slot);

    const Bookmark& bookmark(int slot) const { return bookmarks_[static_cast<std::size_t>(slot)]; }

  private:
    void load(std::string content);
    void rebuild_lines(int width);
    void set_match(int line, std::size_t pos, std::size_t count);

    std::filesystem::path path_;
    std::string display_name_;
    std::string data_;
    std::vector<LineSpan> raw_lines_;
    std::vector<LineSpan> lines_;
    bool binary_ = false;
    bool word_wrap_ = false;

    int top_line_ = 0;
    int column_ = 0;

    Selection selection_;
    std::string last_pattern_;
    bool last_case_sensitive_ = true;
    bool has_last_search_ = false;

    std::array<Bookmark, 10> bookmarks_;
};

}  // namespace listless
