#include "viewer_render.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

namespace listless {

namespace {

constexpr int kTabWidth = 8;  // placeholder until subsystem 08 wires in Style::iTabWidth
constexpr Color kNormalFg = Color::LightGray;
constexpr Color kNormalBg = Color::Black;
constexpr Color kSelectedFg = Color::Black;
constexpr Color kSelectedBg = Color::LightGray;

struct ExpandedLine {
    std::string text;
    std::size_t sel_start = std::string::npos;
    std::size_t sel_end = std::string::npos;
};

// Expands tabs to kTabWidth-aligned stops, tracking where the selected
// range (if any) lands in the expanded output -- selection_.pos/count
// are byte offsets into the raw line, which shift once tabs expand.
ExpandedLine expand_line(std::string_view raw, bool has_selection, std::size_t sel_pos,
                         std::size_t sel_len) {
    ExpandedLine result;
    result.text.reserve(raw.size());

    bool whole_line = has_selection && sel_len == kWholeLine;

    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (has_selection && !whole_line && i == sel_pos) {
            result.sel_start = result.text.size();
        }
        if (has_selection && !whole_line && sel_len != std::string::npos &&
            i == sel_pos + sel_len) {
            result.sel_end = result.text.size();
        }

        if (raw[i] == '\t') {
            std::size_t next_stop = ((result.text.size() / kTabWidth) + 1) * kTabWidth;
            result.text.append(next_stop - result.text.size(), ' ');
        } else {
            result.text.push_back(raw[i]);
        }
    }

    if (has_selection && !whole_line && result.sel_start != std::string::npos &&
        result.sel_end == std::string::npos) {
        result.sel_end = result.text.size();  // match runs to end of line
    }

    if (whole_line) {
        result.sel_start = 0;
        result.sel_end = result.text.size();
    }

    return result;
}

void draw_text_line(Terminal& terminal, int row, const Viewer& viewer, int line_index, int column,
                    int width) {
    bool has_selection = viewer.selection().line == line_index;
    ExpandedLine expanded = expand_line(viewer.line_text(line_index), has_selection,
                                        viewer.selection().pos, viewer.selection().count);

    std::string_view visible = expanded.text;
    if (static_cast<std::size_t>(column) < visible.size()) {
        visible = visible.substr(static_cast<std::size_t>(column));
    } else {
        visible = {};
    }
    if (visible.size() > static_cast<std::size_t>(width)) {
        visible = visible.substr(0, static_cast<std::size_t>(width));
    }

    terminal.clear_to_eol(0, row, kNormalFg, kNormalBg);

    auto clamp_to_visible = [&](std::size_t abs_col) -> std::size_t {
        if (abs_col < static_cast<std::size_t>(column)) {
            return 0;
        }
        std::size_t rel = abs_col - static_cast<std::size_t>(column);
        return std::min(rel, visible.size());
    };

    std::size_t sel_start = expanded.sel_start == std::string::npos
                                ? visible.size()
                                : clamp_to_visible(expanded.sel_start);
    std::size_t sel_end =
        expanded.sel_end == std::string::npos ? visible.size() : clamp_to_visible(expanded.sel_end);

    if (sel_start >= sel_end || !has_selection) {
        if (!visible.empty()) {
            terminal.put_text(0, row, visible, kNormalFg, kNormalBg);
        }
        return;
    }

    if (sel_start > 0) {
        terminal.put_text(0, row, visible.substr(0, sel_start), kNormalFg, kNormalBg);
    }
    terminal.put_text(static_cast<int>(sel_start), row,
                      visible.substr(sel_start, sel_end - sel_start), kSelectedFg, kSelectedBg);
    if (sel_end < visible.size()) {
        terminal.put_text(static_cast<int>(sel_end), row, visible.substr(sel_end), kNormalFg,
                          kNormalBg);
    }
}

void draw_status_line(Terminal& terminal, const Viewer& viewer, int width) {
    int total = viewer.line_count();
    int current = viewer.top_line() + 1;
    int percent = total > 0 ? std::min(100, (current * 100) / total) : 100;

    std::string status = viewer.display_name() + " | line " + std::to_string(current) + "/" +
                         std::to_string(total) + " (" + std::to_string(percent) + "%) | col " +
                         std::to_string(viewer.column() + 1);

    if (static_cast<int>(status.size()) > width) {
        status.resize(static_cast<std::size_t>(width));
    }

    terminal.clear_to_eol(0, 0, kSelectedFg, kSelectedBg);
    terminal.put_text(0, 0, status, kSelectedFg, kSelectedBg);
}

}  // namespace

void render_viewer(const Viewer& viewer, Terminal& terminal) {
    int width = terminal.width();
    int height = terminal.height();

    draw_status_line(terminal, viewer, width);

    for (int row = 1; row < height; ++row) {
        int line_index = viewer.top_line() + (row - 1);
        if (line_index < viewer.line_count()) {
            draw_text_line(terminal, row, viewer, line_index, viewer.column(), width);
        } else {
            terminal.clear_to_eol(0, row, kNormalFg, kNormalBg);
        }
    }
}

}  // namespace listless
