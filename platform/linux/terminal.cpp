#include "terminal.hpp"

#include <ncurses.h>

#include <array>
#include <cstddef>
#include <cstdio>
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
    // Set only when `read_from_tty` is requested (the caller has consumed
    // stdin for piped content and it's no longer usable as a keyboard
    // source).
    FILE* tty_in = nullptr;
    FILE* tty_out = nullptr;

    explicit Impl(bool read_from_tty) {
        SCREEN* screen = nullptr;

        if (read_from_tty) {
            // ncurses reads keyboard input from whatever FILE* is passed
            // as input here; the caller has told us stdin holds piped
            // content rather than a keyboard, so read/write the
            // controlling terminal directly instead.
            tty_in = fopen("/dev/tty", "r");
            tty_out = fopen("/dev/tty", "w");
            if (tty_in == nullptr || tty_out == nullptr) {
                throw std::runtime_error("Terminal: stdin is unavailable for keyboard input and /dev/tty could not be opened");
            }
            screen = newterm(nullptr, tty_out, tty_in);
        } else {
            screen = newterm(nullptr, stdout, stdin);
        }

        if (screen == nullptr) {
            throw std::runtime_error("Terminal: failed to initialize (is $TERM set?)");
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

    ~Impl() {
        endwin();
        if (tty_in != nullptr) {
            fclose(tty_in);
        }
        if (tty_out != nullptr) {
            fclose(tty_out);
        }
    }

    int attr_for(Color fg, Color bg) {
        if (!colors_enabled || !pairs) {
            return A_NORMAL;
        }

        return COLOR_PAIR(pairs->pair_for(fg, bg));
    }
};

Terminal::Terminal(bool read_from_tty) : impl_(std::make_unique<Impl>(read_from_tty)) {}

Terminal::~Terminal() = default;

int Terminal::width() const { return getmaxx(stdscr); }

int Terminal::height() const { return getmaxy(stdscr); }

void Terminal::move_cursor(int x, int y) { wmove(stdscr, y, x); }

void Terminal::put_text(int x, int y, std::string_view text, Color fg, Color bg, bool bold,
                        bool underlined) {
    int attr = impl_->attr_for(fg, bg);
    if (bold) attr |= static_cast<int>(A_BOLD);
    if (underlined) attr |= static_cast<int>(A_UNDERLINE);

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
