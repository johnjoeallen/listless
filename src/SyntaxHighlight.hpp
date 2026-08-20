#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "Color.hpp"
#include "Style.hpp"

namespace listless {

// One coloured run of a highlighted line, in display order and covering
// the line end to end (adjacent spans, no gaps) -- a renderer just walks
// this list and paints `color` from `offset` for `length` bytes.
struct ColorSpan {
    std::size_t offset = 0;
    std::size_t length = 0;
    Color color = Color::LightGray;
    bool bold = false;
    bool underlined = false;
};

// Cross-line state carried from the end of one line into the start of
// the next -- matches the original's `LineStatus` (ostxt.hpp), split
// into independent fields since a line can genuinely be both bold and
// underlined at once (the original's single enum only encodes that
// combination when neither in_comment nor in_preprocessor also holds --
// see `Viewer::scanData`, osview.cpp:1652-1673). Default-constructed
// state is "start of file" (LS_NONE).
struct HighlightState {
    bool in_comment = false;
    bool in_preprocessor = false;
    bool bold = false;
    bool underlined = false;
    int block_text_base_indent = -1;

    friend bool operator==(const HighlightState&, const HighlightState&) = default;
};

// Tokenizes one line of text against `style`'s syntax rules (reserved
// words, comment/string/preprocessor delimiters, symbols, numeric
// prefixes) and the original's `BOLD_CODE`/`UNDERLINE_CODE` control-byte
// toggles, producing colour spans plus the state to carry into the next
// line. Ported from the per-character loop in `Viewer::displayData`
// (osview.cpp:832-1060) and the state-tracking pass in `Viewer::scanData`
// (osview.cpp:1518-1689) -- ordering and comment/string/preprocessor
// precedence match those exactly; see docs/09-syntax-highlighting.md for
// what's narrowed relative to the original. `BOLD_CODE`/`UNDERLINE_CODE`
// bytes are consumed (not emitted as a visible span) and only toggle
// `state.bold`/`state.underlined`, matching the original.
//
// If `style.syntax_highlight_enabled` resolves false (or unset), the
// entire line is returned as a single Default-coloured span with only
// the bold/underline toggle bytes processed -- matching the original's
// "no support for text with layout when syntax highlighting has been
// enabled" comment (osview.cpp:839-840), i.e. the two features are
// mutually exclusive per the original's own logic.
std::vector<ColorSpan> highlight_line(std::string_view text, const Style& style,
                                      HighlightState& state);

}  // namespace listless
