#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Color.hpp"

namespace listless {

// A value that may be set directly on a Style, or left unset so it
// resolves lazily through the style's base styles instead. Mirrors the
// original's `Item<T>` (os.hpp) and its `GetItem()` fallback chain, minus
// the manual memory management `T*`/`new`/`delete` required in C++98.
//
// `add_base_item()` prepends (matching `Item<T>::AddBaseItem`, which
// inserts each new base *before* the current head of `iBaseItems` --
// so among several base styles, the most recently added one is
// consulted first when resolving an unset field).
template <typename T>
class Item {
  public:
    void set(T value) { value_ = std::move(value); }

    // Own value only, ignoring base items -- matches `GetUnresolved()`.
    // Used by config-saving code, which only ever writes non-inherited
    // values (inherited ones are implied by the `Style`'s base list).
    const T* get_unresolved() const { return value_ ? &*value_ : nullptr; }

    // Own value if set; otherwise the first base item (in
    // most-recently-added order) that resolves to a value, recursively.
    // Matches `Item<T>::GetItem()`.
    const T* get() const {
        if (value_) return &*value_;
        for (const Item<T>* base : base_items_) {
            if (const T* found = base->get()) return found;
        }
        return nullptr;
    }

    // Creates the own value (default-constructed) if unset, and returns
    // a mutable reference to it -- for list-valued fields whose Add*()
    // methods append to the style's own (non-inherited) list.
    T& mutable_own() {
        if (!value_) value_ = T{};
        return *value_;
    }

    void add_base_item(Item<T>* item) { base_items_.insert(base_items_.begin(), item); }

  private:
    std::optional<T> value_;
    std::vector<Item<T>*> base_items_;
};

enum class StyleDisplayMode { Text, Hex };

// A syntax-highlighting reserved word. `inherited` distinguishes words
// copied in from a base style (so `save_config()` can skip re-writing
// them -- matching the original's `iInherited` flag on `ReservedWord`).
struct ReservedWord {
    bool inherited = false;
    std::string keyword;
};

// One named style: display colours, tab/wrap settings, and syntax-
// highlighting rules, with prototype-style inheritance from zero or more
// base styles. See docs/08-style-config.md for what this narrows or
// defers relative to the original's `Style` (os.hpp/osstyle.cpp).
//
// `Style` is not copyable or movable: `Item<T>::add_base_item()` stores
// raw pointers into the base style's `Item<T>` members, and `AddBaseStyle`
// stores raw pointers into *this* style's own members too (via the
// `Item<T>&` overload) -- both would dangle if a `Style` moved.
// `StyleSet` stores styles behind `std::unique_ptr` for exactly this
// reason.
class Style {
  public:
    explicit Style(std::string name) : name_(std::move(name)) {}

    Style(const Style&) = delete;
    Style& operator=(const Style&) = delete;
    Style(Style&&) = delete;
    Style& operator=(Style&&) = delete;

    const std::string& name() const { return name_; }
    const std::vector<std::string>& base_style_names() const { return base_style_names_; }

    void add_extension(std::string ext) { extensions.mutable_own().push_back(std::move(ext)); }
    void add_open_comment(std::string s) { open_comment.mutable_own().push_back(std::move(s)); }
    void add_close_comment(std::string s) { close_comment.mutable_own().push_back(std::move(s)); }
    void add_eol_comment(std::string s) { eol_comment.mutable_own().push_back(std::move(s)); }
    void add_numeric_prefix(std::string s) { numeric_prefix.mutable_own().push_back(std::move(s)); }

    // Adds or updates `keyword` (case-insensitive) in `reserved`; does
    // not add a duplicate. Matches `Style::AddReservedWord`.
    void add_reserved_word(std::string keyword, bool inherited = false);

    // Links every field to fall back to `base`'s, appends `base`'s name
    // to base_style_names(), and copies `base`'s reserved words in
    // (marked inherited). Matches `Style::AddBaseStyle`; note
    // `extensions` deliberately does not inherit, matching the original.
    void add_base_style(Style& base);

    Item<std::vector<std::string>> extensions;
    Item<Color> fore_color;
    Item<Color> back_color;
    Item<Color> selected_fore_color;
    Item<Color> selected_back_color;
    Item<Color> bold_color;
    Item<Color> underline_color;
    Item<Color> bold_underline_color;
    Item<bool> expand_tabs;
    Item<bool> high_bit_filter;
    Item<bool> text_with_layout;
    Item<int> tab_width;
    Item<StyleDisplayMode> display_mode;
    Item<std::string> external_filter_cmd;
    Item<bool> filter_enabled;
    Item<std::string> editor;
    Item<int> top_line_format;
    Item<bool> word_break;

    std::vector<ReservedWord> reserved;
    Item<bool> syntax_highlight_enabled;
    Item<Color> symbols_color;
    Item<Color> comment_color;
    Item<Color> string_color;
    Item<Color> reserved_color;
    Item<Color> preprocessor_color;
    Item<Color> number_color;
    Item<Color> ident_color;
    Item<std::string> symbols;
    Item<std::vector<std::string>> open_comment;
    Item<std::vector<std::string>> close_comment;
    Item<std::vector<std::string>> eol_comment;
    Item<std::string> string_delimiter;
    Item<char> escape;
    Item<std::vector<std::string>> numeric_prefix;
    Item<std::string> before_delimiter;
    Item<bool> case_sensitive;
    Item<bool> case_convert;
    Item<std::string> open_preprocessor;
    Item<std::string> close_preprocessor;
    Item<int> comment_column;
    Item<char> line_continuation;

  private:
    std::string name_;
    std::vector<std::string> base_style_names_;
};

// Returns the next colour in the 16-value palette after `c`, wrapping
// White -> Black. The underlying primitive for the original's F2-F7
// live colour-cycling keys; the keybinding wiring itself is an
// App/main-loop concern (issue #24), not this subsystem's.
Color cycle_color(Color c);

// The collection of named styles a config file loads into, always
// containing at least a "Default" style (present from construction,
// matching the original's always-present `gDefaultStyle`). Styles are
// held behind `std::unique_ptr` so `Item<T>::add_base_item()`'s raw
// pointers into other styles' members stay valid as the set grows.
class StyleSet {
  public:
    StyleSet();

    Style& default_style() { return *styles_.front(); }
    const Style& default_style() const { return *styles_.front(); }

    // Case-insensitive lookup by name; nullptr if not found.
    Style* find(std::string_view name);

    // Case-insensitive lookup, creating (and registering) a new style
    // if none exists yet.
    Style& get_or_create(std::string_view name);

    // First style (in registration order) whose resolved extensions
    // include `ext` (a leading-dot extension, e.g. ".cpp", or "*").
    // nullptr if none matches.
    Style* style_for_extension(std::string_view ext);

    const std::vector<std::unique_ptr<Style>>& styles() const { return styles_; }

  private:
    std::vector<std::unique_ptr<Style>> styles_;
};

// $XDG_CONFIG_HOME/listless/style.conf, or ~/.config/listless/style.conf
// if XDG_CONFIG_HOME is unset -- a deliberate reinterpretation of the
// original's `os.set` (a fixed path next to the DOS/OS2 executable),
// which has no Linux equivalent. Superseded by default_styles_dir()
// (a directory of *.conf files) as the preferred personal config
// location, but still loaded for backward compatibility -- see
// docs/08-style-config.md.
std::filesystem::path default_config_path();

// $XDG_CONFIG_HOME/listless/styles/syntax/, or
// ~/.config/listless/styles/syntax/ if
// XDG_CONFIG_HOME is unset -- the personal counterpart to
// system_styles_dir(), loaded after it so a user's own styles take
// precedence over (or extend, via BaseStyle) the system defaults.
std::filesystem::path default_styles_dir();

// The read-only, package-installed styles directory -- baked in at
// build time from CMAKE_INSTALL_PREFIX (see CMakeLists.txt's
// LISTLESS_SYSTEM_STYLES_DIR compile definition), defaulting to
// /usr/local/share/listless/styles if the definition is absent (e.g.
// a build system that doesn't set it). Loaded first, so personal
// styles (default_styles_dir(), default_config_path()) can override or
// extend anything shipped here by reusing a style's name.
std::filesystem::path system_styles_dir();

// Parses `path` into `styles`, adding/updating styles by name (matching
// existing ones case-insensitively, same as `loadConfig`). Returns false
// without modifying `styles` if the file doesn't exist (a fresh install
// has no config yet -- not an error). Malformed content is skipped line
// by line rather than aborting the whole load, a deliberate deviation
// from the original's `exit(3)`-on-error behaviour (see
// docs/08-style-config.md).
bool load_config(StyleSet& styles, const std::filesystem::path& path);

// Loads every *.conf file directly inside `dir` (not recursive) into
// `styles`, in filename order. Unlike calling load_config() once per
// file in a loop, style names are pre-registered across every file in
// `dir` *before* any file's fields/BaseStyle links are parsed, so a
// style in one file can serve as a BaseStyle for a style in another
// file regardless of which file loads first (see
// docs/08-style-config.md's "Cross-file inheritance" section). Returns
// false without modifying `styles` if `dir` doesn't exist -- not an
// error, matching load_config()'s missing-file handling.
bool load_config_dir(StyleSet& styles, const std::filesystem::path& dir);

// Writes every style's own (non-inherited) fields to `path`, in the same
// `Style Name (ext...) BaseStyle...  { Key => Value }` shape
// `load_config()` reads. Matches `saveConfig`.
void save_config(const StyleSet& styles, const std::filesystem::path& path);

}  // namespace listless
