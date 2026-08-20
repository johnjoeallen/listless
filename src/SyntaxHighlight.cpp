#include "SyntaxHighlight.hpp"

#include <cctype>
#include <cstddef>

namespace listless {

namespace {

// Item<T>::get() returns a pointer that may be null; resolving it as
// `item.get() ? *item.get() : fallback` (or `item.get() != nullptr &&
// *item.get()`) calls get() twice, which the optimizer at -O2 can't
// always prove returns the same pointer both times -- GCC then flags
// the second dereference as a potential null dereference
// (-Wnull-dereference), a false positive that only Debug builds happen
// to dodge (issue #43). Calling get() once sidesteps it.
template <typename T>
T get_or(const Item<T>& item, T fallback) {
    const T* value = item.get();
    return value ? *value : fallback;
}

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
    bool case_sensitive = get_or(style.case_sensitive, false);

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

    Color comment_color = get_or(style.comment_color, Color::LightGray);
    Color preprocessor_color = get_or(style.preprocessor_color, Color::LightGray);
    Color string_color = get_or(style.string_color, Color::LightGray);
    Color block_text_color = get_or(style.block_text_color, string_color);
    Color symbols_color = get_or(style.symbols_color, Color::LightGray);
    Color number_color = get_or(style.number_color, Color::LightGray);
    Color reserved_color = get_or(style.reserved_color, Color::LightGray);
    Color ident_color = get_or(style.ident_color, Color::LightGray);
    Color default_color = get_or(style.fore_color, Color::LightGray);

    char escape = get_or(style.escape, '\0');
    const std::string* block_text_start = style.block_text_start.get();
    const std::string* line_start_prefix = style.line_start_prefix.get();
    const std::string* prefix_token = style.prefix_token.get();
    std::size_t first_content = text.find_first_not_of(" \t");
    int indent = first_content == std::string_view::npos ? 0 : static_cast<int>(first_content);
    if (state.block_text_base_indent >= 0) {
        if (first_content == std::string_view::npos || indent > state.block_text_base_indent) {
            state.in_comment = false;
            state.in_preprocessor = false;
            return {{0, text.size(), block_text_color}};
        }
        state.block_text_base_indent = -1;
    }
    const std::string* before_delimiter = style.before_delimiter.get();
    bool before_delimiter_requires_space = get_or(style.before_delimiter_requires_space, false);
    std::size_t contextual_start = n;
    std::size_t contextual_end = n;
    if (before_delimiter != nullptr && !before_delimiter->empty()) {
        std::size_t delimiter = text.find(*before_delimiter);
        contextual_start = text.find_first_not_of(" \t");
        bool delimiter_is_mapping =
            delimiter == std::string_view::npos || !before_delimiter_requires_space ||
            delimiter + before_delimiter->size() == n ||
            std::isspace(static_cast<unsigned char>(text[delimiter + before_delimiter->size()]));
        if (delimiter_is_mapping && delimiter != std::string_view::npos &&
            contextual_start != std::string_view::npos && contextual_start < delimiter) {
            contextual_end = delimiter;
            while (contextual_end > contextual_start &&
                   std::isspace(static_cast<unsigned char>(text[contextual_end - 1]))) {
                --contextual_end;
            }
        } else {
            contextual_start = n;
        }
    }

    bool starts_block_text = false;
    if (block_text_start != nullptr && !block_text_start->empty()) {
        for (char marker : *block_text_start) {
            if (text.find(marker) != std::string_view::npos) {
                starts_block_text = true;
                break;
            }
        }
    }

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
        if (i == contextual_start) {
            push(i, contextual_end - i, reserved_color);
            i = contextual_end;
            seen_non_space = true;
        } else if (i == first_content && line_start_prefix != nullptr &&
                   line_start_prefix->find(c) != std::string::npos) {
            push(i, 1, reserved_color);
            ++i;
            seen_non_space = true;
        } else if (prefix_token != nullptr && prefix_token->find(c) != std::string::npos) {
            std::size_t start = i++;
            while (i < n && (is_identifier_char(text[i]) || text[i] == '-' || text[i] == ':')) ++i;
            push(start, i - start, reserved_color);
            seen_non_space = true;
        } else if (match_any(rest, style.eol_comment)) {
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
        char continuation = get_or(style.line_continuation, '\0');
        state.in_preprocessor = continuation != '\0' && n > 0 && text.back() == continuation;
        state.in_comment = false;
    } else {
        state.in_comment = false;
        state.in_preprocessor = false;
    }
    state.bold = false;
    state.underlined = false;
    if (starts_block_text) state.block_text_base_indent = indent;

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

    Color default_color = get_or(style.fore_color, Color::LightGray);
    Color bold_color = get_or(style.bold_color, default_color);
    Color underline_color = get_or(style.underline_color, default_color);
    Color bold_underline_color = get_or(style.bold_underline_color, default_color);

    bool with_layout = get_or(style.text_with_layout, false);

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
    bool syntax_on =
        get_or(style.syntax_highlight_enabled, false) &&
        (!style.reserved.empty() || style.before_delimiter.get() != nullptr ||
         style.block_text_start.get() != nullptr || style.line_start_prefix.get() != nullptr ||
         style.prefix_token.get() != nullptr);

    std::vector<ColorSpan> spans =
        syntax_on ? highlight_syntax(text, style, state) : highlight_layout(text, style, state);
    return merge_adjacent(std::move(spans));
}

}  // namespace listless
