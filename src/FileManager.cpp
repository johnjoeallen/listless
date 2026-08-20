#include "FileManager.hpp"

#include <algorithm>
#include <cctype>

#include "Glob.hpp"
#include "Text.hpp"

namespace listless {

namespace {

bool starts_with_ci(std::string_view name, std::string_view prefix) {
    if (prefix.size() > name.size()) {
        return false;
    }
    return compare_ignore_case(name.substr(0, prefix.size()), prefix) == 0;
}

// The original special-cases ".." to have no extension (strrchr on ".."
// would otherwise find a spurious '.').
std::string extension_of(const std::string& name) {
    if (name == "..") {
        return "";
    }

    auto pos = name.rfind('.');
    if (pos == std::string::npos) {
        return "";
    }

    return name.substr(pos + 1);
}

int compare_by_key(const DirEntry& a, const DirEntry& b, SortKey key) {
    switch (key) {
        case SortKey::Name:
            return compare_ignore_case(a.name, b.name);

        case SortKey::Extension: {
            int cmp = compare_ignore_case(extension_of(a.name), extension_of(b.name));
            return cmp != 0 ? cmp : compare_ignore_case(a.name, b.name);
        }

        case SortKey::Date: {
            if (a.last_write_time != b.last_write_time) {
                return a.last_write_time < b.last_write_time ? -1 : 1;
            }
            return compare_ignore_case(a.name, b.name);
        }

        case SortKey::Size: {
            if (a.size != b.size) {
                return a.size < b.size ? -1 : 1;
            }
            return compare_ignore_case(a.name, b.name);
        }
    }

    return 0;
}

}  // namespace

Grid compute_grid(int screen_width, int screen_height, int requested_columns,
                  int min_column_width) {
    // The original's `do { iColumnWidth = di.screenwidth / --iColumns; }
    // while (iColumnWidth < 40)` decrements before the first division, so
    // `requested_columns` itself is never tried -- see
    // docs/06-file-list-ui.md. The floor at 1 column (rather than letting
    // `columns` reach 0) is a defensive addition; the original has no
    // such guard.
    int columns = requested_columns;
    int column_width = screen_width;

    do {
        --columns;
        if (columns < 1) {
            columns = 1;
            break;
        }
        column_width = screen_width / columns;
    } while (column_width < min_column_width);

    int lines_per_column = screen_height - 3;
    if (lines_per_column < 1) {
        lines_per_column = 1;
    }

    return Grid{columns, lines_per_column};
}

FileManager::FileManager(std::filesystem::path dir) : dir_(std::move(dir)) { refresh(); }

bool FileManager::change_directory(const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec) || ec) {
        return false;
    }

    directory_history_.push_back(dir_);
    dir_ = dir;
    selected_ = 0;
    view_column_ = 0;
    clear_type_ahead();
    refresh();

    return true;
}

bool FileManager::enter_selected(const Grid& grid) {
    if (entries_.empty() || !selected().is_directory) {
        return false;
    }

    const DirEntry& d = selected();
    std::filesystem::path previous = dir_;
    bool going_up = (d.name == "..");

    directory_history_.push_back(dir_);
    dir_ = d.path;
    selected_ = 0;
    view_column_ = 0;
    clear_type_ahead();
    refresh();

    if (going_up) {
        std::string child_name = previous.filename().string();

        for (std::size_t i = 0; i < entries_.size(); ++i) {
            if (entries_[i].is_directory && entries_[i].name == child_name) {
                select(i, grid);
                break;
            }
        }
    }

    return true;
}

void FileManager::set_file_spec(std::string_view pattern) {
    file_spec_ = std::string(pattern);
    selected_ = 0;
    view_column_ = 0;
    clear_type_ahead();
    refresh();
}

void FileManager::refresh() {
    entries_.clear();

    if (dir_.parent_path() != dir_) {
        entries_.push_back(DirEntry{"..", dir_.parent_path(), 0, {}, true, false});
    }

    std::error_code iter_ec;
    auto it = std::filesystem::directory_iterator(
        dir_, std::filesystem::directory_options::skip_permission_denied, iter_ec);

    if (!iter_ec) {
        for (const auto& dir_entry : it) {
            std::error_code status_ec;
            std::filesystem::file_status status = dir_entry.status(status_ec);
            if (status_ec) {
                continue;
            }

            bool is_dir = std::filesystem::is_directory(status);

            // The original's fill() lists every directory unfiltered and
            // only applies the file spec to non-directory entries.
            if (!is_dir && !glob_match(file_spec_, dir_entry.path().filename().string())) {
                continue;
            }

            std::uintmax_t size = 0;
            if (!is_dir) {
                std::error_code size_ec;
                size = dir_entry.file_size(size_ec);
                if (size_ec) {
                    continue;
                }
            }

            std::error_code time_ec;
            std::filesystem::file_time_type mtime = dir_entry.last_write_time(time_ec);
            if (time_ec) {
                continue;
            }

            bool read_only = (status.permissions() & std::filesystem::perms::owner_write) ==
                             std::filesystem::perms::none;

            entries_.push_back(DirEntry{
                dir_entry.path().filename().string(),
                dir_entry.path(),
                size,
                mtime,
                is_dir,
                read_only,
            });
        }
    }

    sort_entries();
    clamp_selection();
}

void FileManager::set_sort(SortKey key, bool ascending) {
    sort_key_ = key;
    sort_ascending_ = ascending;
    sort_entries();
}

void FileManager::sort_entries() {
    std::sort(entries_.begin(), entries_.end(),
              [key = sort_key_, asc = sort_ascending_](const DirEntry& a, const DirEntry& b) {
                  if (a.is_directory != b.is_directory) {
                      return a.is_directory;
                  }
                  if (a.is_directory) {
                      // Directories always sort by name, ascending,
                      // regardless of sort_ascending() -- matching every
                      // original comparator.
                      return compare_ignore_case(a.name, b.name) < 0;
                  }

                  int cmp = compare_by_key(a, b, key);
                  if (!asc) {
                      cmp = -cmp;
                  }
                  return cmp < 0;
              });
}

void FileManager::clamp_selection() {
    if (entries_.empty()) {
        selected_ = 0;
        return;
    }

    if (selected_ >= entries_.size()) {
        selected_ = entries_.size() - 1;
    }
}

void FileManager::adjust_view_column(const Grid& grid) {
    if (grid.lines_per_column <= 0 || grid.columns <= 0) {
        return;
    }

    int which_column = static_cast<int>(selected_) / grid.lines_per_column;

    if (which_column < view_column_) {
        view_column_ = which_column;
    } else if (which_column >= view_column_ + grid.columns) {
        view_column_ = which_column - (grid.columns - 1);
        if (view_column_ < 0) {
            view_column_ = 0;
        }
    }
}

void FileManager::select(std::size_t index, const Grid& grid) {
    if (entries_.empty()) {
        selected_ = 0;
        return;
    }

    selected_ = index >= entries_.size() ? entries_.size() - 1 : index;
    adjust_view_column(grid);
}

bool FileManager::move_up(const Grid& grid) {
    if (entries_.empty() || selected_ == 0 || grid.lines_per_column <= 0) {
        return false;
    }

    selected_ -= 1;

    if (static_cast<int>(selected_) < view_column_ * grid.lines_per_column) {
        --view_column_;
        if (view_column_ < 0) {
            view_column_ = 0;
        }
    }

    return true;
}

bool FileManager::move_down(const Grid& grid) {
    if (entries_.empty() || selected_ >= entries_.size() - 1 || grid.lines_per_column <= 0) {
        return false;
    }

    selected_ += 1;

    if (static_cast<int>(selected_) >= (view_column_ + grid.columns) * grid.lines_per_column) {
        ++view_column_;
    }

    return true;
}

bool FileManager::move_left(const Grid& grid) {
    if (grid.lines_per_column <= 0) {
        return false;
    }
    if (static_cast<int>(selected_) - grid.lines_per_column < 0) {
        return false;
    }

    selected_ -= static_cast<std::size_t>(grid.lines_per_column);

    if (static_cast<int>(selected_) < view_column_ * grid.lines_per_column) {
        --view_column_;
        if (view_column_ < 0) {
            view_column_ = 0;
        }
    }

    return true;
}

bool FileManager::move_right(const Grid& grid) {
    if (grid.lines_per_column <= 0 || entries_.empty()) {
        return false;
    }

    int n = static_cast<int>(entries_.size());
    int cur = static_cast<int>(selected_);

    if (((cur / grid.lines_per_column) + 1) * grid.lines_per_column >= n) {
        return false;
    }

    cur += grid.lines_per_column;
    if (cur > n - 1) {
        cur = n - 1;
    }

    selected_ = static_cast<std::size_t>(cur);

    if (cur >= (view_column_ + grid.columns) * grid.lines_per_column) {
        ++view_column_;
    }

    return true;
}

bool FileManager::move_home(const Grid& grid) {
    if (entries_.empty() || selected_ == 0 || grid.lines_per_column <= 0) {
        return false;
    }

    int top_of_visible = view_column_ * grid.lines_per_column;

    if (static_cast<int>(selected_) == top_of_visible) {
        selected_ = 0;
        view_column_ = 0;
    } else {
        selected_ = static_cast<std::size_t>(top_of_visible);
    }

    return true;
}

bool FileManager::move_end(const Grid& grid) {
    if (entries_.empty() || grid.lines_per_column <= 0) {
        return false;
    }

    int n = static_cast<int>(entries_.size());
    int last_index = n - 1;
    int bottom_of_visible = (view_column_ + grid.columns) * grid.lines_per_column - 1;
    int cur = static_cast<int>(selected_);

    if (cur == bottom_of_visible && cur != last_index) {
        selected_ = static_cast<std::size_t>(last_index);
        view_column_ = (last_index / grid.lines_per_column) - (grid.columns - 1);
        if (view_column_ < 0) {
            view_column_ = 0;
        }
        return true;
    }

    if (cur != last_index) {
        int candidate = bottom_of_visible;

        if (candidate > last_index) {
            selected_ = static_cast<std::size_t>(last_index);
            // Divides by `n`, not `last_index` -- a literal port of the
            // original's fileman.cpp:1797 (see docs/06-file-list-ui.md).
            view_column_ = (n / grid.lines_per_column) - (grid.columns - 1);
            if (view_column_ < 0) {
                view_column_ = 0;
            }
        } else {
            selected_ = static_cast<std::size_t>(candidate);
        }

        return true;
    }

    return false;
}

bool FileManager::move_page_up(const Grid& grid) {
    if (entries_.empty() || selected_ == 0 || grid.lines_per_column <= 0) {
        return false;
    }

    int cur = static_cast<int>(selected_) - grid.lines_per_column;
    if (cur < 0) {
        cur = 0;
    }

    selected_ = static_cast<std::size_t>(cur);
    --view_column_;
    if (view_column_ < 0) {
        view_column_ = 0;
    }

    return true;
}

bool FileManager::move_page_down(const Grid& grid) {
    if (entries_.empty() || grid.lines_per_column <= 0) {
        return false;
    }

    int n = static_cast<int>(entries_.size());
    int last_index = n - 1;

    if (static_cast<int>(selected_) >= last_index) {
        return false;
    }

    int cur = static_cast<int>(selected_) + grid.lines_per_column;
    int new_column = view_column_ + 1;

    if (((cur / grid.lines_per_column) + 1) * grid.lines_per_column >= n) {
        --new_column;
    }

    if (cur > last_index) {
        cur = last_index;
    }

    selected_ = static_cast<std::size_t>(cur);
    view_column_ = new_column;

    return true;
}

bool FileManager::match_select(std::string_view text, bool forward, bool directories,
                               const Grid& grid) {
    if (entries_.empty()) {
        return false;
    }

    if (directories && !selected().is_directory) {
        select(0, grid);
    }

    if (text.empty()) {
        select(0, grid);
        return true;
    }

    std::size_t n = entries_.size();

    if (forward) {
        for (std::size_t i = selected_ + 1; i < n; ++i) {
            if (entries_[i].is_directory == directories && starts_with_ci(entries_[i].name, text)) {
                select(i, grid);
                return true;
            }
        }
        for (std::size_t i = selected_ + 1; i > 0;) {
            --i;
            if (entries_[i].is_directory == directories && starts_with_ci(entries_[i].name, text)) {
                select(i, grid);
                return true;
            }
        }
    } else {
        for (std::size_t i = selected_; i > 0;) {
            --i;
            if (entries_[i].is_directory == directories && starts_with_ci(entries_[i].name, text)) {
                select(i, grid);
                return true;
            }
        }
    }

    return false;
}

bool FileManager::type_ahead_append(char c, bool directories, const Grid& grid, bool forward) {
    std::string& text = directories ? dir_text_ : file_text_;
    std::string candidate = text + static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (match_select(candidate, forward, directories, grid)) {
        text = candidate;
        return true;
    }

    return false;
}

bool FileManager::type_ahead_backspace(bool directories, const Grid& grid) {
    std::string& text = directories ? dir_text_ : file_text_;
    if (text.empty()) {
        return false;
    }

    // First tries a backward re-search with the text unchanged (find an
    // earlier entry that still matches); only shrinks the filter if that
    // fails -- matching fileman.cpp:2349-2378 exactly.
    if (!match_select(text, /*forward=*/false, directories, grid)) {
        text.pop_back();
        match_select(text, /*forward=*/false, directories, grid);
    }

    return true;
}

void FileManager::clear_type_ahead() {
    file_text_.clear();
    dir_text_.clear();
}

}  // namespace listless
