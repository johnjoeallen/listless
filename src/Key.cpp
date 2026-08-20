#include "Key.hpp"

#include <array>
#include <cctype>
#include <cstddef>

namespace listless {

KeyCode alt_key(char c) {
    // Scan codes for Alt+A..Alt+Z, matching the original's VKALT_A..
    // VKALT_Z (ostxt.hpp).
    static constexpr std::array<int, 26> kAltLetter = {
        0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
        0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C,
    };

    // Scan codes for Alt+0..Alt+9, indexed by digit value. There's no
    // named VKALT_0 in ostxt.hpp, but scan code 0x81 (129 decimal) is
    // used for it at the one real call site found (Viewer's bookmark
    // slot 0, subsystem 07) -- it matches kbdtab's '0' row's alt column
    // (EXT(129)) exactly, so it belongs in the same table as 1-9.
    static constexpr std::array<int, 10> kAltDigit = {
        0x81, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80,
    };

    if (std::isalpha(static_cast<unsigned char>(c))) {
        std::size_t index =
            static_cast<std::size_t>(std::toupper(static_cast<unsigned char>(c)) - 'A');
        return Key::ExtendedBase + kAltLetter[index];
    }

    if (c >= '0' && c <= '9') {
        std::size_t index = static_cast<std::size_t>(c - '0');
        return Key::ExtendedBase + kAltDigit[index];
    }

    return Key::Unknown;
}

}  // namespace listless
