#include "FileManagerRender.hpp"

#include <algorithm>
#include <string>

namespace listless {

namespace {

// Mirrors viewer_render.cpp's palette -- kept as a separate, duplicated
// pair of constants rather than a shared header, since that's the only
// thing the two renderers would share.
constexpr Color kNormalFg = Color::LightGray;
constexpr Color kNormalBg = Color::Black;
constexpr Color kSelectedFg = Color::Black;
constexpr Color kSelectedBg = Color::LightGray;

std::string format_cell(const DirEntry& entry, int column_width) {
    std::string text = entry.name;
    if (entry.is_directory) {
        text += '/';
    }
    if (static_cast<int>(text.size()) > column_width) {
        text.resize(static_cast<std::size_t>(column_width));
    } else {
        text.append(static_cast<std::size_t>(column_width) - text.size(), ' ');
    }
    return text;
}

void draw_grid_row(Terminal& terminal, int row, const FileManager& fm, const Grid& grid, int r,
                   int column_width) {
    terminal.clear_to_eol(0, row, kNormalFg, kNormalBg);

    for (int c = 0; c < grid.columns; ++c) {
        std::size_t index = static_cast<std::size_t>(fm.view_column() + c) *
                                static_cast<std::size_t>(grid.lines_per_column) +
                            static_cast<std::size_t>(r);
        if (index >= fm.size()) {
            continue;
        }

        bool selected = index == fm.selected_index();
        std::string cell = format_cell(fm.entry(index), column_width);
        terminal.put_text(c * column_width, row, cell, selected ? kSelectedFg : kNormalFg,
                          selected ? kSelectedBg : kNormalBg);
    }
}

void draw_status_line(Terminal& terminal, const FileManager& fm, int width) {
    std::string status = fm.current_directory().string() + " | " + fm.file_spec() + " | " +
                         std::to_string(fm.size()) + " entries";

    if (static_cast<int>(status.size()) > width) {
        status.resize(static_cast<std::size_t>(width));
    }

    terminal.clear_to_eol(0, 0, kSelectedFg, kSelectedBg);
    terminal.put_text(0, 0, status, kSelectedFg, kSelectedBg);
}

}  // namespace

void render_file_manager(const FileManager& fm, const Grid& grid, Terminal& terminal) {
    int width = terminal.width();
    int height = terminal.height();
    int column_width = grid.columns > 0 ? width / grid.columns : width;

    draw_status_line(terminal, fm, width);

    for (int row = 1; row < height; ++row) {
        int r = row - 1;
        if (r < grid.lines_per_column) {
            draw_grid_row(terminal, row, fm, grid, r, column_width);
        } else {
            terminal.clear_to_eol(0, row, kNormalFg, kNormalBg);
        }
    }
}

}  // namespace listless
