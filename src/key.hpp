#pragma once

namespace listless {

// The original OnScreen/2's internal keycode model (see
// docs/05-keyboard-input.md), preserved so ported key-dispatch switch
// statements can reuse the original's literal values directly: a plain
// ASCII/control character is its own value (0-255); an "extended" key
// (arrows, function keys, editing keys, Alt+letter/digit) is
// 0xFF00 + <PC/AT Set-1 scan code>, matching the original's getKey()
// (osmisc.cpp) convention exactly.
using KeyCode = int;

namespace Key {

constexpr KeyCode ExtendedBase = 0xFF00;
constexpr KeyCode Escape = 27;

constexpr KeyCode Up = 0xFF48;
constexpr KeyCode Down = 0xFF50;
constexpr KeyCode Left = 0xFF4B;
constexpr KeyCode Right = 0xFF4D;
constexpr KeyCode Home = 0xFF47;
constexpr KeyCode End = 0xFF4F;
constexpr KeyCode PageUp = 0xFF49;
constexpr KeyCode PageDown = 0xFF51;
constexpr KeyCode Insert = 0xFF52;
constexpr KeyCode Delete = 0xFF53;
constexpr KeyCode ShiftTab = 0xFF0F;

constexpr KeyCode F1 = 0xFF3B;
constexpr KeyCode F2 = 0xFF3C;
constexpr KeyCode F3 = 0xFF3D;
constexpr KeyCode F4 = 0xFF3E;
constexpr KeyCode F5 = 0xFF3F;
constexpr KeyCode F6 = 0xFF40;
constexpr KeyCode F7 = 0xFF41;
constexpr KeyCode F8 = 0xFF42;
constexpr KeyCode F9 = 0xFF43;
constexpr KeyCode F10 = 0xFF44;
constexpr KeyCode F11 = 0xFF85;
constexpr KeyCode F12 = 0xFF86;

// Not part of the original's key space (no BIOS scan code for it) --
// surfaced through the same read_key() stream as a pseudo-key, matching
// how most ncurses TUIs report a terminal resize.
constexpr KeyCode Resize = 0xFFF0;

// An extended key that isn't one of the above (or an unmodeled Alt/Esc
// sequence) -- surfaced rather than silently dropped.
constexpr KeyCode Unknown = 0xFFFF;

}  // namespace Key

// The Alt+<c> keycode, matching the original's VKALT_* table
// (ostxt.hpp): valid for 'A'-'Z'/'a'-'z' and '1'-'9'. Returns
// Key::Unknown for any other character.
KeyCode alt_key(char c);

}  // namespace listless
