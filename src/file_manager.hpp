#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "directory.hpp"

namespace listless {

enum class SortKey { Name, Extension, Date, Size };

// Column-major grid geometry: `columns` logical columns of
// `lines_per_column` rows each. Entry index `i` sits at row
// `i % lines_per_column`, column `i / lines_per_column`.
struct Grid {
    int columns = 1;
    int lines_per_column = 1;
};

// The original's screenwidth/screenheight layout math (fileman.cpp's
// FileManager constructor): searches downward from `requested_columns -
// 1` (matching the original's pre-decrement-in-the-same-expression
// idiom -- the requested count itself is never tried) for the widest
// column count with each column at least `min_column_width` wide, and
// sets `lines_per_column` to `screen_height - 3` (reserving 3 status
// rows). The default `requested_columns = 3` matches the constructor;
// pass 2/3/4 to implement the original's Alt+1/Alt+2/Alt+3 override
// (see docs/06-file-list-ui.md).
Grid compute_grid(int screen_width, int screen_height, int requested_columns = 3,
                  int min_column_width = 40);

// Pure logic and state for one FileManager directory listing: listing,
// sorting, column-major grid navigation, and type-ahead multi-select. No
// terminal/rendering dependency -- see docs/06-file-list-ui.md for what's
// ported, narrowed, or deferred relative to the original's FileManager
// (fileman.cpp).
class FileManager {
  public:
    explicit FileManager(std::filesystem::path dir = std::filesystem::current_path());

    const std::filesystem::path& current_directory() const { return dir_; }
    const std::string& file_spec() const { return file_spec_; }

    // Re-lists `dir` if it names a directory, resetting selection and
    // type-ahead state, and appending the *previous* directory to
    // directory_history(). Returns false (no change) if `dir` doesn't
    // name a directory.
    bool change_directory(const std::filesystem::path& dir);

    // Sets the glob pattern filtering files (not directories -- matching
    // the original, directories are always listed unfiltered), re-lists,
    // and resets selection and type-ahead state.
    void set_file_spec(std::string_view pattern);

    // Re-lists the current directory with the current file_spec() and
    // sort, clamping the selection to the new size. Directories are
    // always listed (unfiltered by file_spec()); files are filtered by
    // it. A synthetic ".." entry is included unless dir() is the
    // filesystem root.
    void refresh();

    void set_sort(SortKey key, bool ascending = true);
    SortKey sort_key() const { return sort_key_; }
    bool sort_ascending() const { return sort_ascending_; }

    const std::vector<std::filesystem::path>& directory_history() const {
        return directory_history_;
    }

    std::size_t size() const { return entries_.size(); }
    const DirEntry& entry(std::size_t index) const { return entries_.at(index); }

    std::size_t selected_index() const { return selected_; }
    const DirEntry& selected() const { return entries_.at(selected_); }

    // Selects `index` directly, clamping to a valid range, and adjusts
    // the visible column so it stays in view under `grid`.
    void select(std::size_t index, const Grid& grid);

    // If selected() is a directory, navigates into it: appends the
    // current directory to directory_history(), re-lists, and resets
    // selection/type-ahead state. If the entry is "..", additionally
    // re-selects (in the freshly-listed parent) the directory just left,
    // matching the original's "remember where I came from" behaviour
    // (fileman.cpp:1958-1983) -- change_directory() (a typed path, not a
    // list entry) does not do this. Returns false, unchanged, if
    // selected() isn't a directory.
    bool enter_selected(const Grid& grid);

    // Left-most visible column (there can be more logical columns than
    // fit on screen at once).
    int view_column() const { return view_column_; }

    // All move_*() return true if the selection (or just the visible
    // column) changed.
    bool move_up(const Grid& grid);
    bool move_down(const Grid& grid);
    bool move_left(const Grid& grid);
    bool move_right(const Grid& grid);
    bool move_home(const Grid& grid);
    bool move_end(const Grid& grid);
    bool move_page_up(const Grid& grid);
    bool move_page_down(const Grid& grid);

    // Incremental type-ahead select (MatchSelect): appends `c` to the
    // accumulated per-kind (file/directory) search text and selects the
    // first matching entry of that kind whose name starts with the
    // accumulated text (case-insensitive). Returns false, leaving the
    // accumulated text unchanged, if no match is found. Forward search
    // wraps around; backward does not (see docs/06-file-list-ui.md).
    bool type_ahead_append(char c, bool directories, const Grid& grid, bool forward = true);

    // Removes the last character from the accumulated per-kind search
    // text and re-searches backward from the current selection. Returns
    // false if the text was already empty.
    bool type_ahead_backspace(bool directories, const Grid& grid);

    // Clears both accumulated search strings.
    void clear_type_ahead();

    const std::string& type_ahead_text(bool directories) const {
        return directories ? dir_text_ : file_text_;
    }

  private:
    bool match_select(std::string_view text, bool forward, bool directories, const Grid& grid);
    void sort_entries();
    void clamp_selection();
    void adjust_view_column(const Grid& grid);

    std::filesystem::path dir_;
    std::string file_spec_ = "*";
    std::vector<DirEntry> entries_;
    std::vector<std::filesystem::path> directory_history_;

    SortKey sort_key_ = SortKey::Name;
    bool sort_ascending_ = true;

    std::size_t selected_ = 0;
    int view_column_ = 0;

    std::string file_text_;
    std::string dir_text_;
};

}  // namespace listless
