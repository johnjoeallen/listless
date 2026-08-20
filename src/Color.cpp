#include "Color.hpp"

#include <array>
#include <cstddef>

namespace listless {

AnsiColor to_ansi(Color c) {
    // ANSI base colour (0-7) for each of the 8 hues in DOS palette order
    // (Black, Blue, Green, Cyan, Red, Magenta, Brown, LightGray). The
    // high 8 DOS colours (DarkGray..White) are the same 8 hues again,
    // just "bright".
    static constexpr std::array<int, 8> kAnsiBaseForHue = {
        0,  // Black      -> ANSI black
        4,  // Blue       -> ANSI blue
        2,  // Green      -> ANSI green
        6,  // Cyan       -> ANSI cyan
        1,  // Red        -> ANSI red
        5,  // Magenta    -> ANSI magenta
        3,  // Brown      -> ANSI yellow (brown is dark yellow)
        7,  // LightGray  -> ANSI white
    };

    auto raw = static_cast<unsigned>(c);
    std::size_t hue = raw % 8;
    bool bright = raw >= 8;

    return AnsiColor{kAnsiBaseForHue[hue], bright};
}

}  // namespace listless
