#include "Viewer.hpp"

#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>

#include "Search.hpp"

namespace listless {

namespace {

constexpr int kLineBufMax = 1024;
constexpr int kHorizontalStep = 10;

// The last match strictly before `limit`, if any, on `text` -- used for
// "repeat search backward, continuing on the same line": HorspoolSearcher
// only finds the leftmost match, so this walks forward accumulating
// matches up to the limit and keeps the last one found.
std::optional<std::size_t> find_last_before(std::string_view text, const HorspoolSearcher& searcher,
                                            std::size_t limit) {
    std::optional<std::size_t> best;
    std::size_t start = 0;

    while (true) {
        auto match = searcher.find(text, start);
        if (!match || *match >= limit) {
            break;
        }
        best = match;
        start = *match + 1;
    }

    return best;
}

int clamp_top_for_full_page(int line_count, int visible_lines) {
    int max_top = line_count - visible_lines;
    return max_top < 0 ? 0 : max_top;
}

}  // namespace

Viewer::Viewer(const std::filesystem::path& path) : path_(path), display_name_(path.string()) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Viewer: cannot open " + path.string());
    }

    std::ostringstream buf;
    buf << in.rdbuf();
    if (in.bad()) {
        throw std::runtime_error("Viewer: error reading " + path.string());
    }

    load(buf.str());
}

Viewer::Viewer(std::string content, std::string display_name)
    : display_name_(std::move(display_name)) {
    load(std::move(content));
}

void Viewer::load(std::string content) {
    data_ = std::move(content);
    binary_ = data_.find('\0') != std::string::npos;

    raw_lines_.clear();
    std::size_t line_start = 0;

    for (std::size_t i = 0; i < data_.size(); ++i) {
        if (data_[i] == '\n') {
            std::size_t length = i - line_start;
            if (length > 0 && data_[line_start + length - 1] == '\r') {
                --length;
            }
            raw_lines_.push_back(LineSpan{line_start, length});
            line_start = i + 1;
        }
    }

    if (line_start < data_.size()) {
        raw_lines_.push_back(LineSpan{line_start, data_.size() - line_start});
    }

    lines_ = raw_lines_;
}

void Viewer::rebuild_lines(int width) {
    if (!word_wrap_ || width <= 0) {
        lines_ = raw_lines_;
        return;
    }

    lines_.clear();

    for (const LineSpan& raw : raw_lines_) {
        if (raw.length == 0) {
            lines_.push_back(raw);
            continue;
        }

        std::size_t pos = 0;
        auto limit = static_cast<std::size_t>(width);

        while (pos < raw.length) {
            std::size_t remaining = raw.length - pos;
            if (remaining <= limit) {
                lines_.push_back(LineSpan{raw.offset + pos, remaining});
                break;
            }

            std::size_t last_space = std::string::npos;
            for (std::size_t i = 0; i < limit; ++i) {
                if (data_[raw.offset + pos + i] == ' ') {
                    last_space = i;
                }
            }

            std::size_t take =
                (last_space != std::string::npos && last_space > 0) ? last_space + 1 : limit;
            lines_.push_back(LineSpan{raw.offset + pos, take});
            pos += take;
        }
    }
}

std::string_view Viewer::line_text(int line) const {
    const LineSpan& span = lines_.at(static_cast<std::size_t>(line));
    return std::string_view(data_).substr(span.offset, span.length);
}

void Viewer::set_word_wrap(bool enabled, int width) {
    word_wrap_ = enabled;
    rebuild_lines(width);

    int max_line = line_count() > 0 ? line_count() - 1 : 0;
    top_line_ = std::clamp(top_line_, 0, max_line);
    clear_selection();
}

bool Viewer::scroll_to_top() {
    if (top_line_ == 0) {
        return false;
    }
    top_line_ = 0;
    return true;
}

bool Viewer::scroll_to_bottom(int visible_lines) {
    if (visible_lines <= 0) {
        return false;
    }
    int target = clamp_top_for_full_page(line_count(), visible_lines);
    if (target == top_line_) {
        return false;
    }
    top_line_ = target;
    return true;
}

bool Viewer::scroll_page_down(int visible_lines) {
    if (visible_lines <= 0) {
        return false;
    }
    int max_top = clamp_top_for_full_page(line_count(), visible_lines);
    int target = std::min(top_line_ + visible_lines, max_top);
    if (target == top_line_) {
        return false;
    }
    top_line_ = target;
    return true;
}

bool Viewer::scroll_page_up(int visible_lines) {
    if (visible_lines <= 0 || top_line_ == 0) {
        return false;
    }
    top_line_ = std::max(0, top_line_ - visible_lines);
    return true;
}

bool Viewer::scroll_line_down(int visible_lines) {
    // Clamped the same way as page-down/End, so the viewport never
    // scrolls past the point where the last page stops filling the
    // screen -- a deliberate simplification/consistency choice; the
    // original's own clamp for single-line-down is described only
    // loosely ("if not at last line") in the sources examined.
    if (visible_lines <= 0) {
        return false;
    }
    int max_top = clamp_top_for_full_page(line_count(), visible_lines);
    if (top_line_ >= max_top) {
        return false;
    }
    ++top_line_;
    return true;
}

bool Viewer::scroll_line_up() {
    if (top_line_ <= 0) {
        return false;
    }
    --top_line_;
    return true;
}

bool Viewer::scroll_right(int visible_width) {
    int cap = kLineBufMax - (visible_width + kHorizontalStep);
    if (cap < 0) {
        cap = 0;
    }
    int target = std::min(column_ + kHorizontalStep, cap);
    if (target == column_) {
        return false;
    }
    column_ = target;
    return true;
}

bool Viewer::scroll_left() {
    if (column_ <= 0) {
        return false;
    }
    column_ = std::max(0, column_ - kHorizontalStep);
    return true;
}

bool Viewer::reset_horizontal_scroll() {
    if (column_ == 0) {
        return false;
    }
    column_ = 0;
    return true;
}

void Viewer::set_match(int line, std::size_t pos, std::size_t count) {
    selection_ = Selection{line, pos, count};
}

void Viewer::clear_selection() { selection_ = Selection{}; }

bool Viewer::search_forward(std::string_view pattern, bool case_sensitive) {
    last_pattern_ = pattern;
    last_case_sensitive_ = case_sensitive;
    has_last_search_ = true;

    HorspoolSearcher searcher(last_pattern_, last_case_sensitive_);

    for (int line = top_line_; line < line_count(); ++line) {
        if (auto match = searcher.find(line_text(line), 0)) {
            set_match(line, *match, last_pattern_.size());
            return true;
        }
    }

    clear_selection();
    return false;
}

bool Viewer::search_backward(std::string_view pattern, bool case_sensitive) {
    last_pattern_ = pattern;
    last_case_sensitive_ = case_sensitive;
    has_last_search_ = true;

    HorspoolSearcher searcher(last_pattern_, last_case_sensitive_);

    for (int line = top_line_; line >= 0; --line) {
        if (auto match = searcher.find(line_text(line), 0)) {
            set_match(line, *match, last_pattern_.size());
            return true;
        }
    }

    clear_selection();
    return false;
}

bool Viewer::repeat_search(bool forward) {
    if (!has_last_search_) {
        return false;
    }

    HorspoolSearcher searcher(last_pattern_, last_case_sensitive_);

    if (selection_.line != -1 && selection_.count != kWholeLine) {
        std::string_view text = line_text(selection_.line);

        if (forward) {
            if (auto match = searcher.find(text, selection_.pos + 1)) {
                set_match(selection_.line, *match, last_pattern_.size());
                return true;
            }
        } else {
            if (auto match = find_last_before(text, searcher, selection_.pos)) {
                set_match(selection_.line, *match, last_pattern_.size());
                return true;
            }
        }
    }

    int start = selection_.line != -1 ? selection_.line : top_line_;

    if (forward) {
        for (int line = start + 1; line < line_count(); ++line) {
            if (auto match = searcher.find(line_text(line), 0)) {
                set_match(line, *match, last_pattern_.size());
                return true;
            }
        }
    } else {
        for (int line = start - 1; line >= 0; --line) {
            if (auto match = searcher.find(line_text(line), 0)) {
                set_match(line, *match, last_pattern_.size());
                return true;
            }
        }
    }

    clear_selection();
    return false;
}

bool Viewer::ensure_selection_visible(int visible_lines, int visible_width) {
    if (selection_.line == -1) {
        return false;
    }

    bool moved = false;

    if (visible_lines > 0 &&
        (selection_.line < top_line_ || selection_.line >= top_line_ + visible_lines)) {
        top_line_ = selection_.line;
        moved = true;
    }

    if (visible_width > 0 && selection_.count != kWholeLine) {
        auto match_end = selection_.pos + selection_.count;
        if (selection_.pos < static_cast<std::size_t>(column_) ||
            match_end >= static_cast<std::size_t>(column_ + visible_width)) {
            int cap = kLineBufMax - (visible_width + kHorizontalStep);
            if (cap < 0) {
                cap = 0;
            }
            column_ = std::clamp(static_cast<int>(selection_.pos), 0, cap);
            moved = true;
        }
    }

    return moved;
}

void Viewer::goto_line(int line) {
    int max_line = line_count() > 0 ? line_count() - 1 : 0;
    line = std::clamp(line, 0, max_line);
    top_line_ = line;
    selection_ = Selection{line, 0, kWholeLine};
}

BookmarkAction Viewer::set_bookmark(int slot) {
    Bookmark& bm = bookmarks_[static_cast<std::size_t>(slot)];

    if (bm.line == -1) {
        bm = Bookmark{top_line_, column_};
        return BookmarkAction::Set;
    }

    if (bm.line == top_line_ && bm.column == column_) {
        bm = Bookmark{-1, 0};
        return BookmarkAction::Cleared;
    }

    bm = Bookmark{top_line_, column_};
    return BookmarkAction::Reset;
}

bool Viewer::jump_to_bookmark(int slot) {
    const Bookmark& bm = bookmarks_[static_cast<std::size_t>(slot)];
    if (bm.line == -1) {
        return false;
    }

    top_line_ = bm.line;
    column_ = bm.column;
    return true;
}

// --- Hex mode ---------------------------------------------------------

void Viewer::switch_to_hex_mode() {
    std::size_t offset = top_line_ < line_count()
                             ? lines_[static_cast<std::size_t>(top_line_)].offset
                             : data_.size();

    hex_top_line_ = static_cast<int>(offset / kBytesPerHexLine);
    if (offset % kBytesPerHexLine != 0) {
        ++hex_top_line_;
    }

    display_mode_ = DisplayMode::Hex;
}

void Viewer::switch_to_text_mode() {
    std::size_t offset = static_cast<std::size_t>(hex_top_line_) * kBytesPerHexLine;

    // Faithful to the original's switchToTextMode(): finds the first line
    // starting past `offset` and backs up one. If no such line exists
    // (the hex viewport is at/past the last line's start), top_line_
    // falls back to 0, matching the original's uninitialized-loop
    // behaviour in that case rather than clamping to the last line.
    int target = 0;
    for (int i = 0; i < line_count(); ++i) {
        if (lines_[static_cast<std::size_t>(i)].offset > offset) {
            target = i > 0 ? i - 1 : 0;
            break;
        }
    }

    top_line_ = target;
    display_mode_ = DisplayMode::Text;
}

int Viewer::hex_line_count() const {
    return static_cast<int>((data_.size() + kBytesPerHexLine - 1) / kBytesPerHexLine);
}

bool Viewer::hex_scroll_to_top() {
    if (hex_top_line_ == 0) {
        return false;
    }
    hex_top_line_ = 0;
    return true;
}

bool Viewer::hex_scroll_to_bottom(int visible_lines) {
    if (visible_lines <= 0) {
        return false;
    }
    int target = clamp_top_for_full_page(hex_line_count(), visible_lines);
    if (target == hex_top_line_) {
        return false;
    }
    hex_top_line_ = target;
    return true;
}

bool Viewer::hex_scroll_page_down(int visible_lines) {
    if (visible_lines <= 0) {
        return false;
    }
    int max_top = clamp_top_for_full_page(hex_line_count(), visible_lines);
    int target = std::min(hex_top_line_ + visible_lines, max_top);
    if (target == hex_top_line_) {
        return false;
    }
    hex_top_line_ = target;
    return true;
}

bool Viewer::hex_scroll_page_up(int visible_lines) {
    if (visible_lines <= 0 || hex_top_line_ == 0) {
        return false;
    }
    hex_top_line_ = std::max(0, hex_top_line_ - visible_lines);
    return true;
}

bool Viewer::hex_scroll_line_down(int visible_lines) {
    if (visible_lines <= 0) {
        return false;
    }
    int max_top = clamp_top_for_full_page(hex_line_count(), visible_lines);
    if (hex_top_line_ >= max_top) {
        return false;
    }
    ++hex_top_line_;
    return true;
}

bool Viewer::hex_scroll_line_up() {
    if (hex_top_line_ <= 0) {
        return false;
    }
    --hex_top_line_;
    return true;
}

void Viewer::hex_goto_offset(std::size_t offset) {
    if (data_.empty()) {
        hex_top_line_ = 0;
        return;
    }

    offset = std::min(offset, data_.size() - 1);
    offset -= offset % kBytesPerHexLine;
    hex_top_line_ = static_cast<int>(offset / kBytesPerHexLine);
}

std::string_view Viewer::hex_line_bytes(int line) const {
    std::size_t offset = static_cast<std::size_t>(line) * kBytesPerHexLine;
    std::size_t len = std::min(static_cast<std::size_t>(kBytesPerHexLine), data_.size() - offset);
    return std::string_view(data_).substr(offset, len);
}

}  // namespace listless
