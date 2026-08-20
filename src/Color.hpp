#pragma once

#include <cstdint>

namespace listless {

// The original OnScreen/2's 16-value PC BIOS/CGA text-attribute palette,
// preserved with the same numeric values (0-15) so a later Style config
// (subsystem 08) can store/round-trip these exactly as the original did.
enum class Color : std::uint8_t {
    Black = 0,
    Blue,
    Green,
    Cyan,
    Red,
    Magenta,
    Brown,
    LightGray,
    DarkGray,
    LightBlue,
    LightGreen,
    LightCyan,
    LightRed,
    LightMagenta,
    Yellow,
    White,
};

// A colour expressed as a standard ANSI base colour (0-7: black, red,
// green, yellow, blue, magenta, cyan, white) plus a "high intensity"
// flag -- the model most terminal colour APIs (including ncurses' base
// 8/16-colour palette) use.
struct AnsiColor {
    int base = 0;
    bool bright = false;
};

// Converts a Color to its ANSI base colour + bright flag. The DOS
// palette's colour order differs from ANSI's (e.g. DOS puts Blue at
// index 1 and Red at 4; ANSI has Red at 1 and Blue at 4), so this is a
// real lookup, not an identity mapping.
AnsiColor to_ansi(Color c);

}  // namespace listless
