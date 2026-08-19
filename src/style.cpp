#include "style.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <sstream>

#include "text.hpp"

namespace listless {

namespace {

constexpr std::string_view kDefaultStyleName = "Default";

bool same_name(std::string_view a, std::string_view b) { return compare_ignore_case(a, b) == 0; }

// Colour names in exactly Color's declaration order (0-15), matching the
// original's colorTable[] -- also the order/spelling a saved config uses.
constexpr std::array<std::string_view, 16> kColorNames = {
    "Black",    "Blue",         "Green",    "Cyan",      "Red",        "Magenta",
    "Brown",    "LightGray",    "DarkGray", "LightBlue", "LightGreen", "LightCyan",
    "LightRed", "LightMagenta", "Yellow",   "White",
};

std::optional<Color> parse_color(std::string_view s) {
    for (std::size_t i = 0; i < kColorNames.size(); ++i) {
        if (same_name(s, kColorNames[i])) return static_cast<Color>(i);
    }
    return std::nullopt;
}

std::string_view color_name(Color c) { return kColorNames[static_cast<std::size_t>(c)]; }

std::optional<bool> parse_bool(std::string_view s) {
    if (same_name(s, "On") || same_name(s, "Yes")) return true;
    if (same_name(s, "Off") || same_name(s, "No")) return false;
    return std::nullopt;
}

std::optional<int> parse_int(std::string_view s) {
    int value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc{} || ptr != s.data() + s.size()) return std::nullopt;
    return value;
}

std::optional<char> parse_char(std::string_view s) {
    if (s.size() != 1) return std::nullopt;
    return s[0];
}

std::optional<StyleDisplayMode> parse_display_mode(std::string_view s) {
    if (same_name(s, "Text")) return StyleDisplayMode::Text;
    if (same_name(s, "Hex")) return StyleDisplayMode::Hex;
    return std::nullopt;
}

// One config key: applies a textual value to the matching Style field.
// Scalar fields overwrite on repeated application (matching Item<T>::
// SetItem being called again); list fields (Reserved/*Comment/
// NumberPrefix) append -- the original supports both by letting the same
// key repeat across "=> value" continuation lines.
struct FieldSpec {
    std::string_view key;
    std::function<bool(Style&, std::string_view)> apply;
};

bool apply_color(Item<Color>& item, std::string_view v) {
    auto c = parse_color(v);
    if (!c) return false;
    item.set(*c);
    return true;
}

bool apply_bool(Item<bool>& item, std::string_view v) {
    auto b = parse_bool(v);
    if (!b) return false;
    item.set(*b);
    return true;
}

bool apply_int(Item<int>& item, std::string_view v) {
    auto n = parse_int(v);
    if (!n) return false;
    item.set(*n);
    return true;
}

bool apply_char(Item<char>& item, std::string_view v) {
    auto c = parse_char(v);
    if (!c) return false;
    item.set(*c);
    return true;
}

bool apply_string(Item<std::string>& item, std::string_view v) {
    item.set(std::string(v));
    return true;
}

// Field table, in the same order as the original's styleSectionTable, and
// using its exact key spellings so a hand-edited config stays readable to
// anyone who knows the original format.
const std::vector<FieldSpec>& field_table() {
    static const std::vector<FieldSpec> table = {
        {"CommentColor",
         [](Style& s, std::string_view v) { return apply_color(s.comment_color, v); }},
        {"SymbolsColor",
         [](Style& s, std::string_view v) { return apply_color(s.symbols_color, v); }},
        {"StringColor",
         [](Style& s, std::string_view v) { return apply_color(s.string_color, v); }},
        {"ReservedColor",
         [](Style& s, std::string_view v) { return apply_color(s.reserved_color, v); }},
        {"PreProcessorColor",
         [](Style& s, std::string_view v) { return apply_color(s.preprocessor_color, v); }},
        {"NumberColor",
         [](Style& s, std::string_view v) { return apply_color(s.number_color, v); }},
        {"IdentColor", [](Style& s, std::string_view v) { return apply_color(s.ident_color, v); }},
        {"ForeGndColor", [](Style& s, std::string_view v) { return apply_color(s.fore_color, v); }},
        {"BackGndColor", [](Style& s, std::string_view v) { return apply_color(s.back_color, v); }},
        {"SelectedForeGndColor",
         [](Style& s, std::string_view v) { return apply_color(s.selected_fore_color, v); }},
        {"SelectedBackGndColor",
         [](Style& s, std::string_view v) { return apply_color(s.selected_back_color, v); }},
        {"BoldColor", [](Style& s, std::string_view v) { return apply_color(s.bold_color, v); }},
        {"UnderlineColor",
         [](Style& s, std::string_view v) { return apply_color(s.underline_color, v); }},
        {"BoldUnderlineColor",
         [](Style& s, std::string_view v) { return apply_color(s.bold_underline_color, v); }},
        {"ExpandTabs", [](Style& s, std::string_view v) { return apply_bool(s.expand_tabs, v); }},
        {"HighbitFilter",
         [](Style& s, std::string_view v) { return apply_bool(s.high_bit_filter, v); }},
        {"TextWithLayout",
         [](Style& s, std::string_view v) { return apply_bool(s.text_with_layout, v); }},
        {"TabWidth", [](Style& s, std::string_view v) { return apply_int(s.tab_width, v); }},
        {"DisplayMode",
         [](Style& s, std::string_view v) {
             auto m = parse_display_mode(v);
             if (!m) return false;
             s.display_mode.set(*m);
             return true;
         }},
        {"WordBreak", [](Style& s, std::string_view v) { return apply_bool(s.word_break, v); }},
        {"TopLineFormat",
         [](Style& s, std::string_view v) { return apply_int(s.top_line_format, v); }},
        {"Reserved",
         [](Style& s, std::string_view v) {
             s.add_reserved_word(std::string(v));
             return true;
         }},
        {"Strings",
         [](Style& s, std::string_view v) { return apply_string(s.string_delimiter, v); }},
        {"Escape", [](Style& s, std::string_view v) { return apply_char(s.escape, v); }},
        {"Symbols", [](Style& s, std::string_view v) { return apply_string(s.symbols, v); }},
        {"OpenComment",
         [](Style& s, std::string_view v) {
             s.add_open_comment(std::string(v));
             return true;
         }},
        {"CloseComment",
         [](Style& s, std::string_view v) {
             s.add_close_comment(std::string(v));
             return true;
         }},
        {"SingleLineComment",
         [](Style& s, std::string_view v) {
             s.add_eol_comment(std::string(v));
             return true;
         }},
        {"NumberPrefix",
         [](Style& s, std::string_view v) {
             s.add_numeric_prefix(std::string(v));
             return true;
         }},
        {"OpenPreProcessor",
         [](Style& s, std::string_view v) { return apply_string(s.open_preprocessor, v); }},
        {"ClosePreProcessor",
         [](Style& s, std::string_view v) { return apply_string(s.close_preprocessor, v); }},
        {"CommentColumn",
         [](Style& s, std::string_view v) { return apply_int(s.comment_column, v); }},
        {"LineContinuation",
         [](Style& s, std::string_view v) { return apply_char(s.line_continuation, v); }},
        {"ExternalFilter",
         [](Style& s, std::string_view v) { return apply_string(s.external_filter_cmd, v); }},
        {"CaseSensitive",
         [](Style& s, std::string_view v) { return apply_bool(s.case_sensitive, v); }},
        {"CaseConvert", [](Style& s, std::string_view v) { return apply_bool(s.case_convert, v); }},
        {"Editor", [](Style& s, std::string_view v) { return apply_string(s.editor, v); }},
    };
    return table;
}

const FieldSpec* find_field(std::string_view key) {
    for (const auto& field : field_table()) {
        if (same_name(field.key, key)) return &field;
    }
    return nullptr;
}

// Splits `text` into content lines with comments and blank lines removed
// -- matching getString()'s per-line preprocessing (trim, tabs -> spaces,
// drop lines that are blank or start with ';'). Config-level tokens never
// span what was originally a comment/blank line, matching the original.
std::vector<std::string> preprocess_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string raw;

    while (std::getline(stream, raw)) {
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();
        for (char& c : raw) {
            if (c == '\t') c = ' ';
        }

        std::string_view trimmed = trim(raw);
        if (trimmed.empty() || trimmed.front() == ';') continue;

        lines.emplace_back(trimmed);
    }

    return lines;
}

// A token stream over preprocessed config lines. `next_token()` never
// crosses a line boundary implicitly for a value -- callers that need
// "rest of the current line" (config values) call `rest_of_line()`
// immediately after consuming "=>", matching getSymbol(f, /*restOfLine=*/1).
class ConfigLexer {
  public:
    explicit ConfigLexer(std::vector<std::string> lines) : lines_(std::move(lines)) {}

    // Next symbol: an identifier/extension (alpha or '.'-led), one of
    // "()*{}", or "=>". Empty string_view at end of input.
    std::string_view next_token() {
        skip_to_content();
        if (line_ >= lines_.size()) return {};

        const std::string& line = lines_[line_];
        std::size_t start = pos_;

        if (std::isalpha(static_cast<unsigned char>(line[pos_])) || line[pos_] == '.') {
            ++pos_;
            while (pos_ < line.size() && line[pos_] != ' ' && line[pos_] != '=' &&
                   line[pos_] != '<' && line[pos_] != '>' && line[pos_] != '(' &&
                   line[pos_] != ')' && line[pos_] != '{' && line[pos_] != '}' &&
                   line[pos_] != '.') {
                ++pos_;
            }
            return std::string_view(line).substr(start, pos_ - start);
        }

        if (line[pos_] == '(' || line[pos_] == ')' || line[pos_] == '*' || line[pos_] == '{' ||
            line[pos_] == '}') {
            ++pos_;
            return std::string_view(line).substr(start, 1);
        }

        if (line[pos_] == '=' && pos_ + 1 < line.size() && line[pos_ + 1] == '>') {
            pos_ += 2;
            return std::string_view(line).substr(start, 2);
        }

        // Unrecognised character: skip it so a stray symbol can't loop
        // the parser forever, and try again.
        ++pos_;
        return next_token();
    }

    // Remaining text of the *current* line (no line advance), trimmed.
    // Empty if the current line was already fully consumed.
    std::string_view rest_of_line() {
        if (line_ >= lines_.size()) return {};

        const std::string& line = lines_[line_];
        std::string_view rest = trim(std::string_view(line).substr(pos_));
        pos_ = line.size();
        return rest;
    }

    bool at_end() {
        skip_to_content();
        return line_ >= lines_.size();
    }

  private:
    void skip_to_content() {
        while (line_ < lines_.size()) {
            while (pos_ < lines_[line_].size() && lines_[line_][pos_] == ' ') ++pos_;
            if (pos_ < lines_[line_].size()) return;
            ++line_;
            pos_ = 0;
        }
    }

    std::vector<std::string> lines_;
    std::size_t line_ = 0;
    std::size_t pos_ = 0;
};

}  // namespace

void Style::add_reserved_word(std::string keyword, bool inherited) {
    for (auto& word : reserved) {
        if (same_name(word.keyword, keyword)) {
            word.keyword = std::move(keyword);
            return;
        }
    }
    reserved.push_back(ReservedWord{inherited, std::move(keyword)});
}

void Style::add_base_style(Style& base) {
    base_style_names_.push_back(base.name());

    fore_color.add_base_item(&base.fore_color);
    back_color.add_base_item(&base.back_color);
    selected_fore_color.add_base_item(&base.selected_fore_color);
    selected_back_color.add_base_item(&base.selected_back_color);
    bold_color.add_base_item(&base.bold_color);
    underline_color.add_base_item(&base.underline_color);
    bold_underline_color.add_base_item(&base.bold_underline_color);
    expand_tabs.add_base_item(&base.expand_tabs);
    high_bit_filter.add_base_item(&base.high_bit_filter);
    text_with_layout.add_base_item(&base.text_with_layout);
    tab_width.add_base_item(&base.tab_width);
    display_mode.add_base_item(&base.display_mode);
    external_filter_cmd.add_base_item(&base.external_filter_cmd);
    filter_enabled.add_base_item(&base.filter_enabled);
    editor.add_base_item(&base.editor);
    top_line_format.add_base_item(&base.top_line_format);
    word_break.add_base_item(&base.word_break);
    syntax_highlight_enabled.add_base_item(&base.syntax_highlight_enabled);
    symbols_color.add_base_item(&base.symbols_color);
    comment_color.add_base_item(&base.comment_color);
    string_color.add_base_item(&base.string_color);
    reserved_color.add_base_item(&base.reserved_color);
    preprocessor_color.add_base_item(&base.preprocessor_color);
    number_color.add_base_item(&base.number_color);
    ident_color.add_base_item(&base.ident_color);
    symbols.add_base_item(&base.symbols);
    string_delimiter.add_base_item(&base.string_delimiter);
    escape.add_base_item(&base.escape);
    numeric_prefix.add_base_item(&base.numeric_prefix);
    case_sensitive.add_base_item(&base.case_sensitive);
    case_convert.add_base_item(&base.case_convert);
    open_preprocessor.add_base_item(&base.open_preprocessor);
    close_preprocessor.add_base_item(&base.close_preprocessor);
    comment_column.add_base_item(&base.comment_column);
    line_continuation.add_base_item(&base.line_continuation);

    // Extensions deliberately don't inherit, matching the original.
    open_comment.add_base_item(&base.open_comment);
    close_comment.add_base_item(&base.close_comment);
    eol_comment.add_base_item(&base.eol_comment);

    for (const ReservedWord& word : base.reserved) {
        add_reserved_word(word.keyword, /*inherited=*/true);
    }
}

Color cycle_color(Color c) {
    auto next = static_cast<unsigned>(c) + 1;
    if (next > static_cast<unsigned>(Color::White)) next = static_cast<unsigned>(Color::Black);
    return static_cast<Color>(next);
}

StyleSet::StyleSet() { styles_.push_back(std::make_unique<Style>(std::string(kDefaultStyleName))); }

Style* StyleSet::find(std::string_view name) {
    for (auto& style : styles_) {
        if (same_name(style->name(), name)) return style.get();
    }
    return nullptr;
}

Style& StyleSet::get_or_create(std::string_view name) {
    if (Style* existing = find(name)) return *existing;
    styles_.push_back(std::make_unique<Style>(std::string(name)));
    return *styles_.back();
}

Style* StyleSet::style_for_extension(std::string_view ext) {
    for (auto& style : styles_) {
        const auto* exts = style->extensions.get();
        if (!exts) continue;
        if (std::any_of(exts->begin(), exts->end(),
                        [&](const std::string& e) { return same_name(e, ext); })) {
            return style.get();
        }
    }
    return nullptr;
}

std::filesystem::path default_config_path() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
        return std::filesystem::path(xdg) / "listless" / "style.conf";
    }
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home ? home : "") / ".config" / "listless" / "style.conf";
}

bool load_config(StyleSet& styles, const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    std::ostringstream buffer;
    buffer << file.rdbuf();
    ConfigLexer lexer(preprocess_lines(buffer.str()));

    while (!lexer.at_end()) {
        std::string_view keyword = lexer.next_token();
        if (!same_name(keyword, "Style")) continue;

        std::string_view style_name = lexer.next_token();
        if (style_name.empty()) break;

        Style& style = same_name(style_name, kDefaultStyleName) ? styles.default_style()
                                                                : styles.get_or_create(style_name);

        if (lexer.next_token() != "(") continue;  // malformed; skip this Style block's header

        std::string_view ext;
        do {
            ext = lexer.next_token();
            if (ext.empty()) break;
            if (ext.front() == '.' || ext.front() == '*') style.add_extension(std::string(ext));
        } while (ext != ")");

        std::string_view token = lexer.next_token();
        while (!token.empty() && token != "{") {
            if (Style* base = styles.find(token)) style.add_base_style(*base);
            token = lexer.next_token();
        }

        std::string_view current_key;
        for (token = lexer.next_token(); !token.empty() && token != "}";
             token = lexer.next_token()) {
            if (token == "=>") {
                std::string_view value = lexer.rest_of_line();
                if (const FieldSpec* field = find_field(current_key)) field->apply(style, value);
            } else {
                current_key = token;
            }
        }
    }

    return true;
}

void save_config(const StyleSet& styles, const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return;

    file << "; Listless style/config file -- see docs/08-style-config.md\n\n";

    for (const auto& style : styles.styles()) {
        file << "Style " << style->name() << " (";

        if (same_name(style->name(), kDefaultStyleName)) {
            file << "*";
        } else if (const auto* exts = style->extensions.get_unresolved()) {
            for (std::size_t i = 0; i < exts->size(); ++i) {
                file << (*exts)[i] << (i + 1 < exts->size() ? " " : "");
            }
        }
        file << ")";

        const auto& bases = style->base_style_names();
        if (!bases.empty()) {
            file << " ";
            for (std::size_t i = 0; i < bases.size(); ++i) {
                file << bases[i] << (i + 1 < bases.size() ? " " : "");
            }
        }
        file << "\n{\n";

        auto write_color = [&](std::string_view key, const Item<Color>& item) {
            if (const Color* c = item.get_unresolved())
                file << "\t" << key << " => " << color_name(*c) << "\n";
        };
        auto write_bool = [&](std::string_view key, const Item<bool>& item) {
            if (const bool* b = item.get_unresolved())
                file << "\t" << key << " => " << (*b ? "On" : "Off") << "\n";
        };
        auto write_int = [&](std::string_view key, const Item<int>& item) {
            if (const int* n = item.get_unresolved()) file << "\t" << key << " => " << *n << "\n";
        };
        auto write_char = [&](std::string_view key, const Item<char>& item) {
            if (const char* c = item.get_unresolved(); c && *c != '\0')
                file << "\t" << key << " => " << *c << "\n";
        };
        auto write_string = [&](std::string_view key, const Item<std::string>& item) {
            if (const std::string* s = item.get_unresolved())
                file << "\t" << key << " => " << *s << "\n";
        };
        auto write_list = [&](std::string_view key, const Item<std::vector<std::string>>& item) {
            if (const auto* list = item.get_unresolved()) {
                for (const std::string& value : *list)
                    file << "\t" << key << " => " << value << "\n";
            }
        };

        write_color("ForeGndColor", style->fore_color);
        write_color("BackGndColor", style->back_color);
        write_color("SelectedForeGndColor", style->selected_fore_color);
        write_color("SelectedBackGndColor", style->selected_back_color);
        write_color("BoldColor", style->bold_color);
        write_color("UnderlineColor", style->underline_color);
        write_color("BoldUnderlineColor", style->bold_underline_color);
        write_bool("ExpandTabs", style->expand_tabs);
        write_bool("HighbitFilter", style->high_bit_filter);
        write_bool("TextWithLayout", style->text_with_layout);
        write_int("TabWidth", style->tab_width);
        if (const StyleDisplayMode* m = style->display_mode.get_unresolved()) {
            file << "\tDisplayMode => " << (*m == StyleDisplayMode::Text ? "Text" : "Hex") << "\n";
        }
        write_string("ExternalFilter", style->external_filter_cmd);
        write_string("Editor", style->editor);
        write_int("TopLineFormat", style->top_line_format);
        write_bool("WordBreak", style->word_break);

        for (const ReservedWord& word : style->reserved) {
            if (!word.inherited) file << "\tReserved => " << word.keyword << "\n";
        }

        write_color("SymbolsColor", style->symbols_color);
        write_color("CommentColor", style->comment_color);
        write_color("StringColor", style->string_color);
        write_color("ReservedColor", style->reserved_color);
        write_color("PreProcessorColor", style->preprocessor_color);
        write_color("NumberColor", style->number_color);
        write_color("IdentColor", style->ident_color);
        write_string("Symbols", style->symbols);
        write_list("OpenComment", style->open_comment);
        write_list("CloseComment", style->close_comment);
        write_list("SingleLineComment", style->eol_comment);
        write_string("Strings", style->string_delimiter);
        write_char("Escape", style->escape);
        write_list("NumberPrefix", style->numeric_prefix);
        write_bool("CaseSensitive", style->case_sensitive);
        write_bool("CaseConvert", style->case_convert);
        write_string("OpenPreProcessor", style->open_preprocessor);
        write_string("ClosePreProcessor", style->close_preprocessor);
        write_int("CommentColumn", style->comment_column);
        write_char("LineContinuation", style->line_continuation);

        file << "}\n\n";
    }
}

}  // namespace listless
