#include "terminal.hpp"

#include <ncurses.h>

#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>

#include "color_pair_table.hpp"

namespace listless {

namespace {

short curses_color_number(Color c) {
    static constexpr std::array<short, 8> kAnsiToCurses = {
        COLOR_BLACK, COLOR_RED,     COLOR_GREEN, COLOR_YELLOW,
        COLOR_BLUE,  COLOR_MAGENTA, COLOR_CYAN,  COLOR_WHITE,
    };

    AnsiColor ansi = to_ansi(c);
    short base = kAnsiToCurses[static_cast<std::size_t>(ansi.base)];

    if (COLORS >= 16 && ansi.bright) {
        return static_cast<short>(base + 8);
    }

    return base;
}

}  // namespace

struct Terminal::Impl {
    bool colors_enabled = false;
    std::optional<ColorPairTable> pairs;

    Impl() {
        if (initscr() == nullptr) {
            throw std::runtime_error("Terminal: initscr() failed (is $TERM set?)");
        }

        cbreak();
        noecho();
        curs_set(1);

        colors_enabled = has_colors();
        if (colors_enabled) {
            start_color();
            pairs.emplace(COLOR_PAIRS, [](int id, Color fg, Color bg) {
                init_pair(static_cast<short>(id), curses_color_number(fg), curses_color_number(bg));
            });
        }
    }

    ~Impl() { endwin(); }

    int attr_for(Color fg, Color bg) {
        if (!colors_enabled || !pairs) {
            return A_NORMAL;
        }

        return COLOR_PAIR(pairs->pair_for(fg, bg));
    }
};

Terminal::Terminal() : impl_(std::make_unique<Impl>()) {}

Terminal::~Terminal() = default;

int Terminal::width() const { return getmaxx(stdscr); }

int Terminal::height() const { return getmaxy(stdscr); }

void Terminal::move_cursor(int x, int y) { wmove(stdscr, y, x); }

void Terminal::put_text(int x, int y, std::string_view text, Color fg, Color bg) {
    int attr = impl_->attr_for(fg, bg);

    wattron(stdscr, attr);
    mvwaddnstr(stdscr, y, x, text.data(), static_cast<int>(text.size()));
    wattroff(stdscr, attr);
}

void Terminal::clear_to_eol(int x, int y, Color fg, Color bg) {
    int attr = impl_->attr_for(fg, bg);

    wmove(stdscr, y, x);
    wattron(stdscr, attr);
    wclrtoeol(stdscr);
    wattroff(stdscr, attr);
}

void Terminal::clear() { werase(stdscr); }

void Terminal::scroll_region(int top, int bottom, int lines) {
    wsetscrreg(stdscr, top, bottom - 1);
    wscrl(stdscr, lines);
    wsetscrreg(stdscr, 0, height() - 1);
}

void Terminal::refresh() { wrefresh(stdscr); }

}  // namespace listless
