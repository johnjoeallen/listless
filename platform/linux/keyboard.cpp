#include "keyboard.hpp"

#include <ncurses.h>

namespace listless {

namespace {

KeyCode translate_curses_key(int raw) {
    switch (raw) {
        case KEY_UP:
            return Key::Up;
        case KEY_DOWN:
            return Key::Down;
        case KEY_LEFT:
            return Key::Left;
        case KEY_RIGHT:
            return Key::Right;
        case KEY_HOME:
            return Key::Home;
        case KEY_END:
            return Key::End;
        case KEY_PPAGE:
            return Key::PageUp;
        case KEY_NPAGE:
            return Key::PageDown;
        case KEY_IC:
            return Key::Insert;
        case KEY_DC:
            return Key::Delete;
        case KEY_BTAB:
            return Key::ShiftTab;
        case KEY_F(1):
            return Key::F1;
        case KEY_F(2):
            return Key::F2;
        case KEY_F(3):
            return Key::F3;
        case KEY_F(4):
            return Key::F4;
        case KEY_F(5):
            return Key::F5;
        case KEY_F(6):
            return Key::F6;
        case KEY_F(7):
            return Key::F7;
        case KEY_F(8):
            return Key::F8;
        case KEY_F(9):
            return Key::F9;
        case KEY_F(10):
            return Key::F10;
        case KEY_F(11):
            return Key::F11;
        case KEY_F(12):
            return Key::F12;
        case KEY_RESIZE:
            return Key::Resize;
        default:
            return Key::Unknown;
    }
}

}  // namespace

struct Keyboard::Impl {
    Impl() { keypad(stdscr, TRUE); }
};

Keyboard::Keyboard(Terminal&) : impl_(std::make_unique<Impl>()) {}

Keyboard::~Keyboard() = default;

bool Keyboard::key_available() {
    nodelay(stdscr, TRUE);
    int c = wgetch(stdscr);
    nodelay(stdscr, FALSE);

    if (c == ERR) {
        return false;
    }

    ungetch(c);
    return true;
}

KeyCode Keyboard::read_key() {
    int c = wgetch(stdscr);

    if (c == 27) {
        // ESC: peek for an immediately-following byte to distinguish a
        // standalone Escape keypress from Alt+<key> (terminals send
        // Alt as a bare ESC prefix on the wire -- there is no separate
        // signal the way the original's Win32 console API had).
        nodelay(stdscr, TRUE);
        int next = wgetch(stdscr);
        nodelay(stdscr, FALSE);

        if (next == ERR) {
            return Key::Escape;
        }

        if (next >= 0 && next < 256) {
            return alt_key(static_cast<char>(next));
        }

        return Key::Unknown;  // ESC followed by another special key: not modeled
    }

    if (c >= KEY_MIN && c <= KEY_MAX) {
        return translate_curses_key(c);
    }

    if (c == ERR) {
        return Key::Unknown;
    }

    return c;  // plain ASCII / control character, passed through as-is
}

}  // namespace listless
