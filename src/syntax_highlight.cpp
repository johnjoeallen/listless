#include "syntax_highlight.hpp"

#include <cctype>
#include <cstddef>

namespace listless {

namespace {

bool char_in_set(const Item<std::string>& item, char c) {
    const std::string* set = item.get();
    return set != nullptr && set->find(c) != std::string::npos;
}

// Returns the length of the delimiter from `candidates` that matches at
// the start of `text`, or 0 if none does. Matches `IsBeginComment`/
// `IsEndComment`/`IsEolComment`'s "first matching entry in the set"
// semantics (osview.cpp:42-90); comment delimiters are always matched
// case-sensitively in the original (unlike `IsEolComment`, which
// respects `iCaseSensitive` -- narrowed here to always case-sensitive,
// noted in docs/09-syntax-highlighting.md).
std::size_t match_any(std::string_view text, const Item<std::vector<std::string>>& item) {
    const std::vector<std::string>* candidates = item.get();
    if (candidates == nullptr) return 0;
    for (const std::string& candidate : *candidates) {
        if (!candidate.empty() && text.substr(0, candidate.size()) == candidate) {
            return candidate.size();
        }
    }
    return 0;
}

std::size_t match_prefix(std::string_view text, const Item<std::string>& item) {
    const std::string* prefix = item.get();
    if (prefix == nullptr || prefix->empty()) return 0;
    return text.substr(0, prefix->size()) == *prefix ? prefix->size() : 0;
}

// Word-boundary-aware reserved-word match at the start of `text`: the
// candidate must match case-(in)sensitively per `case_sensitive`, and
// the following character (if any) must not continue an identifier.
// Matches `keywordCmp`'s semantics (osview.cpp:147-176), as a linear
// scan since `Style::reserved` isn't kept sorted (unlike the original's
// binary-searched `iReserved` -- reserved words are few enough per style
// that this isn't a performance concern).
const std::string* match_reserved(std::string_view text, const Style& style) {
    bool case_sensitive = style.case_sensitive.get() != nullptr && *style.case_sensitive.get();

    for (const ReservedWord& word : style.reserved) {
        const std::string& kw = word.keyword;
        if (text.size() < kw.size()) continue;

        bool matches = true;
        for (std::size_t i = 0; i < kw.size() && matches; ++i) {
            char a = text[i];
            char b = kw[i];
            if (!case_sensitive) {
                a = static_cast<char>(std::toupper(static_cast<unsigned char>(a)));
                b = static_cast<char>(std::toupper(static_cast<unsigned char>(b)));
            }
            matches = (a == b);
        }
        if (!matches) continue;

        if (text.size() > kw.size()) {
            char next = text[kw.size()];
            if (std::isalnum(static_cast<unsigned char>(next)) || next == '_') continue;
        }
        return &kw;
    }
    return nullptr;
}

bool is_identifier_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

// Highlights one line while syntax highlighting is enabled: the
// per-character precedence chain from `Viewer::displayData`
// (osview.cpp:836-1021), unified with the cross-line comment/
// preprocessor state machine from `Viewer::scanData` (osview.cpp:1518-
// 1689) into a single pass -- the original runs these as two separate
// passes over the same rules because `scanData` only needs the state
// carried *out* of each line, while `displayData` re-derives the state
// carried *in*. A single function can produce spans and the exit state
// together instead.
std::vector<ColorSpan> highlight_syntax(std::string_view text, const Style& style,
                                        HighlightState& state) {
    std::vector<ColorSpan> spans;
    std::size_t n = text.size();

    Color comment_color = style.comment_color.get() ? *style.comment_color.get() : Color::LightGray;
    Color preprocessor_color =
        style.preprocessor_color.get() ? *style.preprocessor_color.get() : Color::LightGray;
    Color string_color = style.string_color.get() ? *style.string_color.get() : Color::LightGray;
    Color symbols_color = style.symbols_color.get() ? *style.symbols_color.get() : Color::LightGray;
    Color number_color = style.number_color.get() ? *style.number_color.get() : Color::LightGray;
    Color reserved_color =
        style.reserved_color.get() ? *style.reserved_color.get() : Color::LightGray;
    Color ident_color = style.ident_color.get() ? *style.ident_color.get() : Color::LightGray;
    Color default_color = style.fore_color.get() ? *style.fore_color.get() : Color::LightGray;

    char escape = style.escape.get() ? *style.escape.get() : '\0';

    enum class Mode { Text, Comment, Preprocessor };
    Mode mode = state.in_comment ? Mode::Comment
                                 : (state.in_preprocessor ? Mode::Preprocessor : Mode::Text);
    bool seen_non_space = false;

    auto push = [&](std::size_t start, std::size_t len, Color color) {
        if (len == 0) return;
        spans.push_back(ColorSpan{start, len, color, /*bold=*/false, /*underlined=*/false});
    };

    std::size_t i = 0;
    while (i < n) {
        std::string_view rest = text.substr(i);
        char c = text[i];

        if (mode == Mode::Comment) {
            if (std::size_t len = match_any(rest, style.close_comment)) {
                push(i, len, comment_color);
                i += len;
                mode = Mode::Text;
            } else {
                push(i, 1, comment_color);
                ++i;
            }
            continue;
        }

        if (mode == Mode::Preprocessor) {
            if (match_any(rest, style.eol_comment) || match_any(rest, style.open_comment)) {
                mode = Mode::Text;  // re-examine as a comment start below
                continue;
            }
            if (std::size_t len = match_prefix(rest, style.close_preprocessor)) {
                push(i, len, preprocessor_color);
                i += len;
                mode = Mode::Text;
            } else {
                push(i, 1, preprocessor_color);
                ++i;
            }
            if (!std::isspace(static_cast<unsigned char>(c))) seen_non_space = true;
            continue;
        }

        // Mode::Text
        if (match_any(rest, style.eol_comment)) {
            push(i, n - i, comment_color);
            i = n;
        } else if (match_any(rest, style.open_comment)) {
            mode = Mode::Comment;  // re-process this position in Comment mode
        } else if (match_prefix(rest, style.open_preprocessor) && !seen_non_space) {
            mode = Mode::Preprocessor;  // re-process this position in Preprocessor mode
        } else if (char_in_set(style.string_delimiter, c)) {
            char opener = c;
            push(i, 1, string_color);
            ++i;
            while (i < n && text[i] != opener) {
                if (escape != '\0' && text[i] == escape && i + 1 < n) {
                    push(i, 2, string_color);
                    i += 2;
                    continue;
                }
                push(i, 1, string_color);
                ++i;
            }
            if (i < n) {
                push(i, 1, string_color);
                ++i;
            }
            seen_non_space = true;
        } else if (char_in_set(style.symbols, c)) {
            push(i, 1, symbols_color);
            ++i;
            seen_non_space = true;
        } else if (std::size_t len = match_any(rest, style.numeric_prefix)) {
            std::size_t start = i;
            i += len;
            while (i < n && std::isxdigit(static_cast<unsigned char>(text[i]))) ++i;
            push(start, i - start, number_color);
            seen_non_space = true;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            std::size_t start = i;
            while (i < n && std::isdigit(static_cast<unsigned char>(text[i]))) ++i;
            push(start, i - start, number_color);
            seen_non_space = true;
        } else if (const std::string* kw = match_reserved(rest, style)) {
            push(i, kw->size(), reserved_color);
            i += kw->size();
            seen_non_space = true;
        } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::size_t start = i;
            while (i < n && is_identifier_char(text[i])) ++i;
            push(start, i - start, ident_color);
            seen_non_space = true;
        } else {
            push(i, 1, default_color);
            ++i;
            if (!std::isspace(static_cast<unsigned char>(c))) seen_non_space = true;
        }
    }

    if (mode == Mode::Comment) {
        state.in_comment = true;
        state.in_preprocessor = false;
    } else if (mode == Mode::Preprocessor) {
        char continuation = style.line_continuation.get() ? *style.line_continuation.get() : '\0';
        state.in_preprocessor = continuation != '\0' && n > 0 && text.back() == continuation;
        state.in_comment = false;
    } else {
        state.in_comment = false;
        state.in_preprocessor = false;
    }
    state.bold = false;
    state.underlined = false;

    return spans;
}

// Highlights one line while syntax highlighting is disabled: only the
// `BOLD_CODE`/`UNDERLINE_CODE` control-byte toggles from
// `Viewer::displayData` (osview.cpp:1043-1060), gated on
// `text_with_layout` (`WithLayout`, osview.cpp:29) exactly as the
// original gates the whole branch. Bytes matching either code are
// consumed (not emitted) and only flip `state.bold`/`state.underlined`.
std::vector<ColorSpan> highlight_layout(std::string_view text, const Style& style,
                                        HighlightState& state) {
    constexpr char kBoldCode = ('B' - 'A') + 1;
    constexpr char kUnderlineCode = ('S' - 'A') + 1;

    Color default_color = style.fore_color.get() ? *style.fore_color.get() : Color::LightGray;
    Color bold_color = style.bold_color.get() ? *style.bold_color.get() : default_color;
    Color underline_color =
        style.underline_color.get() ? *style.underline_color.get() : default_color;
    Color bold_underline_color =
        style.bold_underline_color.get() ? *style.bold_underline_color.get() : default_color;

    bool with_layout = style.text_with_layout.get() != nullptr && *style.text_with_layout.get();

    std::vector<ColorSpan> spans;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i] & 0x7F;

        if (with_layout && c == kBoldCode) {
            state.bold = !state.bold;
            continue;
        }
        if (with_layout && c == kUnderlineCode) {
            state.underlined = !state.underlined;
            continue;
        }

        Color color = default_color;
        if (with_layout) {
            if (state.bold && state.underlined) {
                color = bold_underline_color;
            } else if (state.bold) {
                color = bold_color;
            } else if (state.underlined) {
                color = underline_color;
            }
        }
        spans.push_back(ColorSpan{i, 1, color, state.bold, state.underlined});
    }

    state.in_comment = false;
    state.in_preprocessor = false;
    return spans;
}

// Coalesces adjacent, contiguous spans sharing the same colour/bold/
// underline into one -- both `highlight_syntax` and `highlight_layout`
// build up spans one token (or one byte) at a time, which can leave
// runs of identical attributes split across several spans (e.g.
// `highlight_layout` emits one span per byte).
std::vector<ColorSpan> merge_adjacent(std::vector<ColorSpan> spans) {
    std::vector<ColorSpan> merged;
    for (const ColorSpan& span : spans) {
        if (!merged.empty()) {
            ColorSpan& last = merged.back();
            if (last.offset + last.length == span.offset && last.color == span.color &&
                last.bold == span.bold && last.underlined == span.underlined) {
                last.length += span.length;
                continue;
            }
        }
        merged.push_back(span);
    }
    return merged;
}

}  // namespace

std::vector<ColorSpan> highlight_line(std::string_view text, const Style& style,
                                      HighlightState& state) {
    bool syntax_on = style.syntax_highlight_enabled.get() != nullptr &&
                     *style.syntax_highlight_enabled.get() && !style.reserved.empty();

    std::vector<ColorSpan> spans =
        syntax_on ? highlight_syntax(text, style, state) : highlight_layout(text, style, state);
    return merge_adjacent(std::move(spans));
}

}  // namespace listless
