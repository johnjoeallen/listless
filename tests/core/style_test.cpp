#include "Style.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

using listless::Color;
using listless::cycle_color;
using listless::default_config_path;
using listless::default_styles_dir;
using listless::load_config;
using listless::load_config_dir;
using listless::save_config;
using listless::Style;
using listless::StyleDisplayMode;
using listless::StyleSet;
using listless::system_styles_dir;

namespace {

// A scratch file path under the test binary's temp directory, removed on
// destruction.
class TempFile {
  public:
    TempFile() : path_(std::filesystem::temp_directory_path() / "listless_style_test.conf") {}
    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
};

// A scratch directory under the test binary's temp directory (for
// load_config_dir()), recursively removed on destruction.
class TempDir {
  public:
    TempDir()
        : path_(std::filesystem::temp_directory_path() /
                ("listless_style_test_dir_" +
                 std::to_string(reinterpret_cast<std::uintptr_t>(this)))) {
        std::filesystem::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    void write(std::string_view filename, std::string_view content) const {
        std::ofstream out(path_ / filename);
        out << content;
    }

    const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
};

}  // namespace

TEST(Item, UnsetResolvesToNullptr) {
    listless::Item<int> item;
    EXPECT_EQ(item.get(), nullptr);
    EXPECT_EQ(item.get_unresolved(), nullptr);
}

TEST(Item, OwnValueTakesPriorityOverBase) {
    listless::Item<int> base;
    base.set(1);
    listless::Item<int> derived;
    derived.add_base_item(&base);
    derived.set(2);

    ASSERT_NE(derived.get(), nullptr);
    EXPECT_EQ(*derived.get(), 2);
}

TEST(Item, FallsBackToBaseWhenUnset) {
    listless::Item<int> base;
    base.set(42);
    listless::Item<int> derived;
    derived.add_base_item(&base);

    ASSERT_NE(derived.get(), nullptr);
    EXPECT_EQ(*derived.get(), 42);
    EXPECT_EQ(derived.get_unresolved(), nullptr);
}

TEST(Item, ResolvesThroughMultipleLevels) {
    listless::Item<int> grandparent;
    grandparent.set(1);
    listless::Item<int> parent;
    parent.add_base_item(&grandparent);
    listless::Item<int> child;
    child.add_base_item(&parent);

    ASSERT_NE(child.get(), nullptr);
    EXPECT_EQ(*child.get(), 1);
}

TEST(Item, MostRecentlyAddedBaseWinsTies) {
    listless::Item<int> base_a;
    base_a.set(1);
    listless::Item<int> base_b;
    base_b.set(2);

    listless::Item<int> derived;
    derived.add_base_item(&base_a);
    derived.add_base_item(&base_b);  // added after base_a -> consulted first

    ASSERT_NE(derived.get(), nullptr);
    EXPECT_EQ(*derived.get(), 2);
}

TEST(Style, AddBaseStyleLinksScalarFields) {
    Style base("Base");
    base.fore_color.set(Color::Green);
    base.tab_width.set(4);

    Style derived("Derived");
    derived.add_base_style(base);

    ASSERT_NE(derived.fore_color.get(), nullptr);
    EXPECT_EQ(*derived.fore_color.get(), Color::Green);
    ASSERT_NE(derived.tab_width.get(), nullptr);
    EXPECT_EQ(*derived.tab_width.get(), 4);
}

TEST(Style, OwnFieldOverridesBase) {
    Style base("Base");
    base.fore_color.set(Color::Green);

    Style derived("Derived");
    derived.add_base_style(base);
    derived.fore_color.set(Color::Red);

    ASSERT_NE(derived.fore_color.get(), nullptr);
    EXPECT_EQ(*derived.fore_color.get(), Color::Red);
}

TEST(Style, ContextualRulesAndColorsInherit) {
    Style base("Base");
    base.before_delimiter.set(":");
    base.before_delimiter_color.set(Color::Blue);
    base.block_text_start.set("|>");

    Style derived("Derived");
    derived.add_base_style(base);

    ASSERT_NE(derived.before_delimiter.get(), nullptr);
    EXPECT_EQ(*derived.before_delimiter.get(), ":");
    ASSERT_NE(derived.before_delimiter_color.get(), nullptr);
    EXPECT_EQ(*derived.before_delimiter_color.get(), Color::Blue);
    ASSERT_NE(derived.block_text_start.get(), nullptr);
    EXPECT_EQ(*derived.block_text_start.get(), "|>");
}

TEST(Style, ExtensionsDoNotInherit) {
    Style base("Base");
    base.add_extension(".base");

    Style derived("Derived");
    derived.add_base_style(base);

    EXPECT_EQ(derived.extensions.get(), nullptr);
}

TEST(Style, ReservedWordsCopyInAsInherited) {
    Style base("Base");
    base.add_reserved_word("if");
    base.add_reserved_word("else");

    Style derived("Derived");
    derived.add_reserved_word("class");
    derived.add_base_style(base);

    ASSERT_EQ(derived.reserved.size(), 3u);
    EXPECT_FALSE(derived.reserved[0].inherited);
    EXPECT_EQ(derived.reserved[0].keyword, "class");
    EXPECT_TRUE(derived.reserved[1].inherited);
    EXPECT_TRUE(derived.reserved[2].inherited);
}

TEST(Style, AddReservedWordDeduplicatesCaseInsensitively) {
    Style style("S");
    style.add_reserved_word("If");
    style.add_reserved_word("if");

    EXPECT_EQ(style.reserved.size(), 1u);
}

TEST(Style, BaseStyleNamesRecordOrder) {
    Style a("A");
    Style b("B");
    Style derived("Derived");
    derived.add_base_style(a);
    derived.add_base_style(b);

    ASSERT_EQ(derived.base_style_names().size(), 2u);
    EXPECT_EQ(derived.base_style_names()[0], "A");
    EXPECT_EQ(derived.base_style_names()[1], "B");
}

TEST(CycleColor, WrapsAtWhiteBackToBlack) { EXPECT_EQ(cycle_color(Color::White), Color::Black); }

TEST(CycleColor, StepsThroughPalette) {
    EXPECT_EQ(cycle_color(Color::Black), Color::Blue);
    EXPECT_EQ(cycle_color(Color::Blue), Color::Green);
}

TEST(StyleSet, AlwaysHasADefaultStyle) {
    StyleSet styles;
    EXPECT_EQ(styles.default_style().name(), "Default");
    EXPECT_EQ(styles.styles().size(), 1u);
}

TEST(StyleSet, FindIsCaseInsensitiveAndMissingReturnsNull) {
    StyleSet styles;
    EXPECT_EQ(styles.find("default"), &styles.default_style());
    EXPECT_EQ(styles.find("Nonexistent"), nullptr);
}

TEST(StyleSet, GetOrCreateReusesExistingStyle) {
    StyleSet styles;
    Style& first = styles.get_or_create("Cpp");
    Style& second = styles.get_or_create("cpp");

    EXPECT_EQ(&first, &second);
    EXPECT_EQ(styles.styles().size(), 2u);
}

TEST(StyleSet, StyleForExtensionFindsMatch) {
    StyleSet styles;
    Style& cpp = styles.get_or_create("Cpp");
    cpp.add_extension(".cpp");
    cpp.add_extension(".hpp");

    EXPECT_EQ(styles.style_for_extension(".cpp"), &cpp);
    EXPECT_EQ(styles.style_for_extension(".py"), nullptr);
}

TEST(DefaultConfigPath, RespectsXdgConfigHome) {
#ifdef _WIN32
    GTEST_SKIP();
#else
    setenv("XDG_CONFIG_HOME", "/tmp/listless-xdg-test", 1);
    EXPECT_EQ(default_config_path(),
              std::filesystem::path("/tmp/listless-xdg-test/listless/style.conf"));
    unsetenv("XDG_CONFIG_HOME");
#endif
}

TEST(DefaultStylesDir, RespectsXdgConfigHome) {
#ifdef _WIN32
    GTEST_SKIP();
#else
    setenv("XDG_CONFIG_HOME", "/tmp/listless-xdg-test", 1);
    EXPECT_EQ(default_styles_dir(),
              std::filesystem::path("/tmp/listless-xdg-test/listless/styles/syntax"));
    unsetenv("XDG_CONFIG_HOME");
#endif
}

TEST(SystemStylesDir, ReturnsANonEmptyAbsolutePath) {
    // The exact value is a build-time compile definition (see
    // CMakeLists.txt); just check it resolves to something sane rather
    // than pinning the path, which would break across install prefixes.
    EXPECT_TRUE(system_styles_dir().is_absolute());
}

TEST(LoadConfig, MissingFileReturnsFalseWithoutModifyingStyles) {
    StyleSet styles;
    EXPECT_FALSE(load_config(styles, "/nonexistent/path/style.conf"));
    EXPECT_EQ(styles.styles().size(), 1u);
}

TEST(LoadConfig, CaseSensitiveAcceptsOnOffNotTrueFalse) {
    // Regression test: parse_bool() (src/Style.cpp) only recognizes
    // On/Yes/Off/No, matching every other Item<bool> field -- "TRUE" was
    // mistakenly used in this repo's shipped style/syntax/*.conf files
    // and silently ignored (malformed values are skipped, not fatal),
    // leaving CaseSensitive permanently unset there.
    TempFile file;
    {
        std::ofstream out(file.path());
        out << "Style Cpp (.cpp)\n"
               "{\n"
               "\tCaseSensitive => TRUE\n"
               "}\n"
               "Style Shell (.sh)\n"
               "{\n"
               "\tCaseSensitive => On\n"
               "}\n";
    }

    StyleSet styles;
    ASSERT_TRUE(load_config(styles, file.path()));

    Style* cpp = styles.find("Cpp");
    ASSERT_NE(cpp, nullptr);
    EXPECT_EQ(cpp->case_sensitive.get(), nullptr);  // "TRUE" is not a recognized value

    Style* shell = styles.find("Shell");
    ASSERT_NE(shell, nullptr);
    ASSERT_NE(shell->case_sensitive.get(), nullptr);
    EXPECT_TRUE(*shell->case_sensitive.get());
}

TEST(LoadConfig, ParsesScalarFieldsAndExtensions) {
    TempFile file;
    {
        std::ofstream out(file.path());
        out << "Style Cpp (.cpp .hpp)\n"
               "{\n"
               "\tForeGndColor => White\n"
               "\tBackGndColor => Black\n"
               "\tTabWidth => 4\n"
               "\tExpandTabs => On\n"
               "\tEditor => vim\n"
               "\tBeforeDelimiter => :\n"
               "\tBeforeDelimiterRequiresSpace => On\n"
               "\tBlockTextStart => |>\n"
               "\tBlockTextColor => Yellow\n"
               "}\n";
    }

    StyleSet styles;
    ASSERT_TRUE(load_config(styles, file.path()));

    Style* cpp = styles.find("Cpp");
    ASSERT_NE(cpp, nullptr);
    ASSERT_NE(cpp->fore_color.get(), nullptr);
    EXPECT_EQ(*cpp->fore_color.get(), Color::White);
    ASSERT_NE(cpp->back_color.get(), nullptr);
    EXPECT_EQ(*cpp->back_color.get(), Color::Black);
    ASSERT_NE(cpp->tab_width.get(), nullptr);
    EXPECT_EQ(*cpp->tab_width.get(), 4);
    ASSERT_NE(cpp->expand_tabs.get(), nullptr);
    EXPECT_TRUE(*cpp->expand_tabs.get());
    ASSERT_NE(cpp->editor.get(), nullptr);
    EXPECT_EQ(*cpp->editor.get(), "vim");
    ASSERT_NE(cpp->before_delimiter.get(), nullptr);
    EXPECT_EQ(*cpp->before_delimiter.get(), ":");
    ASSERT_NE(cpp->before_delimiter_requires_space.get(), nullptr);
    EXPECT_TRUE(*cpp->before_delimiter_requires_space.get());
    ASSERT_NE(cpp->block_text_start.get(), nullptr);
    EXPECT_EQ(*cpp->block_text_start.get(), "|>");
    ASSERT_NE(cpp->block_text_color.get(), nullptr);
    EXPECT_EQ(*cpp->block_text_color.get(), Color::Yellow);

    ASSERT_NE(cpp->extensions.get(), nullptr);
    EXPECT_EQ(cpp->extensions.get()->size(), 2u);
    EXPECT_EQ((*cpp->extensions.get())[0], ".cpp");
    EXPECT_EQ((*cpp->extensions.get())[1], ".hpp");
}

TEST(LoadConfig, ParsesListFieldsAcrossContinuationLines) {
    TempFile file;
    {
        std::ofstream out(file.path());
        out << "Style Cpp (.cpp)\n"
               "{\n"
               "\tReserved => if\n"
               "\t\t\t=> else\n"
               "\t\t\t=> while\n"
               "}\n";
    }

    StyleSet styles;
    ASSERT_TRUE(load_config(styles, file.path()));

    Style* cpp = styles.find("Cpp");
    ASSERT_NE(cpp, nullptr);
    ASSERT_EQ(cpp->reserved.size(), 3u);
    EXPECT_EQ(cpp->reserved[0].keyword, "if");
    EXPECT_EQ(cpp->reserved[1].keyword, "else");
    EXPECT_EQ(cpp->reserved[2].keyword, "while");
}

TEST(LoadConfig, EnablesSyntaxHighlightingImplicitlyWhenReservedWordsExist) {
    // Matches the original's loadConfig (osstyle.cpp:1309-1313): there's
    // no explicit config key for this field, it's switched on as a side
    // effect of the style ending up with reserved words.
    TempFile file;
    {
        std::ofstream out(file.path());
        out << "Style Cpp (.cpp)\n"
               "{\n"
               "\tReserved => if\n"
               "}\n"
               "Style Plain (.txt)\n"
               "{\n"
               "}\n";
    }

    StyleSet styles;
    ASSERT_TRUE(load_config(styles, file.path()));

    Style* cpp = styles.find("Cpp");
    ASSERT_NE(cpp, nullptr);
    ASSERT_NE(cpp->syntax_highlight_enabled.get(), nullptr);
    EXPECT_TRUE(*cpp->syntax_highlight_enabled.get());

    Style* plain = styles.find("Plain");
    ASSERT_NE(plain, nullptr);
    EXPECT_EQ(plain->syntax_highlight_enabled.get(), nullptr);
}

TEST(LoadConfig, ResolvesBaseStylesByName) {
    TempFile file;
    {
        std::ofstream out(file.path());
        out << "Style Base (*)\n"
               "{\n"
               "\tForeGndColor => Green\n"
               "}\n"
               "\n"
               "Style Cpp (.cpp) Base\n"
               "{\n"
               "}\n";
    }

    StyleSet styles;
    ASSERT_TRUE(load_config(styles, file.path()));

    Style* cpp = styles.find("Cpp");
    ASSERT_NE(cpp, nullptr);
    ASSERT_EQ(cpp->base_style_names().size(), 1u);
    EXPECT_EQ(cpp->base_style_names()[0], "Base");
    ASSERT_NE(cpp->fore_color.get(), nullptr);
    EXPECT_EQ(*cpp->fore_color.get(), Color::Green);
}

TEST(LoadConfig, DefaultStyleNameReusesTheBuiltInDefaultStyle) {
    TempFile file;
    {
        std::ofstream out(file.path());
        out << "Style Default (*)\n"
               "{\n"
               "\tForeGndColor => LightGray\n"
               "}\n";
    }

    StyleSet styles;
    ASSERT_TRUE(load_config(styles, file.path()));

    EXPECT_EQ(styles.styles().size(), 1u);
    ASSERT_NE(styles.default_style().fore_color.get(), nullptr);
    EXPECT_EQ(*styles.default_style().fore_color.get(), Color::LightGray);
}

TEST(LoadConfig, IgnoresCommentAndBlankLines) {
    TempFile file;
    {
        std::ofstream out(file.path());
        out << "; a leading comment\n"
               "\n"
               "Style Cpp (.cpp)\n"
               "{\n"
               "\t; a comment inside the block\n"
               "\tForeGndColor => White\n"
               "}\n";
    }

    StyleSet styles;
    ASSERT_TRUE(load_config(styles, file.path()));

    Style* cpp = styles.find("Cpp");
    ASSERT_NE(cpp, nullptr);
    ASSERT_NE(cpp->fore_color.get(), nullptr);
    EXPECT_EQ(*cpp->fore_color.get(), Color::White);
}

TEST(LoadConfig, InvalidValueIsSkippedNotFatal) {
    TempFile file;
    {
        std::ofstream out(file.path());
        out << "Style Cpp (.cpp)\n"
               "{\n"
               "\tForeGndColor => NotAColor\n"
               "\tTabWidth => 4\n"
               "}\n";
    }

    StyleSet styles;
    ASSERT_TRUE(load_config(styles, file.path()));

    Style* cpp = styles.find("Cpp");
    ASSERT_NE(cpp, nullptr);
    EXPECT_EQ(cpp->fore_color.get(), nullptr);
    ASSERT_NE(cpp->tab_width.get(), nullptr);
    EXPECT_EQ(*cpp->tab_width.get(), 4);
}

TEST(LoadConfigDir, MissingDirectoryReturnsFalseWithoutModifyingStyles) {
    StyleSet styles;
    EXPECT_FALSE(load_config_dir(styles, "/nonexistent/listless/styles"));
    EXPECT_EQ(styles.styles().size(), 1u);
}

TEST(LoadConfigDir, LoadsEveryConfFileAndIgnoresOthers) {
    TempDir dir;
    dir.write("cpp.conf", "Style Cpp (.cpp)\n{\n\tReserved => if\n}\n");
    dir.write("python.conf", "Style Python (.py)\n{\n\tReserved => def\n}\n");
    dir.write("notes.txt", "Style Ignored (.ignored)\n{\n}\n");

    StyleSet styles;
    ASSERT_TRUE(load_config_dir(styles, dir.path()));

    EXPECT_NE(styles.find("Cpp"), nullptr);
    EXPECT_NE(styles.find("Python"), nullptr);
    EXPECT_EQ(styles.find("Ignored"), nullptr);
}

TEST(LoadConfigDir, BaseStyleInAnotherFileResolvesRegardlessOfLoadOrder) {
    // "zcommon.conf" sorts *after* "cpp.conf" -- if load_config_dir()
    // only did a single pass in filename order, Cpp's "Common" BaseStyle
    // token wouldn't exist yet when cpp.conf is parsed, and the link
    // would silently fail to resolve.
    TempDir dir;
    dir.write("cpp.conf", "Style Cpp (.cpp) Common\n{\n\tReserved => if\n}\n");
    dir.write("zcommon.conf", "Style Common ()\n{\n\tForeGndColor => White\n}\n");

    StyleSet styles;
    ASSERT_TRUE(load_config_dir(styles, dir.path()));

    Style* cpp = styles.find("Cpp");
    ASSERT_NE(cpp, nullptr);
    ASSERT_EQ(cpp->base_style_names().size(), 1u);
    EXPECT_EQ(cpp->base_style_names()[0], "Common");
    ASSERT_NE(cpp->fore_color.get(), nullptr);
    EXPECT_EQ(*cpp->fore_color.get(), Color::White);
}

TEST(LoadConfigDir, AWordThatLooksLikeAStyleHeaderInsideAValueIsNotMisread) {
    // Pass 1's pre-registration scan must consume field values via
    // rest_of_line() exactly like the real parser, or a value containing
    // the literal word "Style" would be misread as a new style header.
    TempDir dir;
    dir.write("cpp.conf", "Style Cpp (.cpp)\n{\n\tEditor => vim -c Style now\n}\n");

    StyleSet styles;
    ASSERT_TRUE(load_config_dir(styles, dir.path()));

    EXPECT_NE(styles.find("Cpp"), nullptr);
    EXPECT_EQ(styles.find("now"), nullptr);
    EXPECT_EQ(styles.styles().size(), 2u);  // Default + Cpp, nothing spurious
}

TEST(SaveConfig, RoundTripsScalarFieldsAndExtensions) {
    TempFile file;

    {
        StyleSet styles;
        Style& cpp = styles.get_or_create("Cpp");
        cpp.add_extension(".cpp");
        cpp.fore_color.set(Color::White);
        cpp.tab_width.set(4);
        cpp.expand_tabs.set(true);
        cpp.add_reserved_word("if");
        save_config(styles, file.path());
    }

    StyleSet reloaded;
    ASSERT_TRUE(load_config(reloaded, file.path()));

    Style* cpp = reloaded.find("Cpp");
    ASSERT_NE(cpp, nullptr);
    ASSERT_NE(cpp->extensions.get(), nullptr);
    EXPECT_EQ((*cpp->extensions.get())[0], ".cpp");
    ASSERT_NE(cpp->fore_color.get(), nullptr);
    EXPECT_EQ(*cpp->fore_color.get(), Color::White);
    ASSERT_NE(cpp->tab_width.get(), nullptr);
    EXPECT_EQ(*cpp->tab_width.get(), 4);
    ASSERT_NE(cpp->expand_tabs.get(), nullptr);
    EXPECT_TRUE(*cpp->expand_tabs.get());
    ASSERT_EQ(cpp->reserved.size(), 1u);
    EXPECT_EQ(cpp->reserved[0].keyword, "if");
}

TEST(SaveConfig, OnlyWritesOwnNotInheritedReservedWords) {
    TempFile file;

    {
        StyleSet styles;
        Style& base = styles.get_or_create("Base");
        base.add_reserved_word("if");

        Style& derived = styles.get_or_create("Derived");
        derived.add_base_style(base);
        derived.add_reserved_word("class");

        save_config(styles, file.path());
    }

    std::ifstream in(file.path());
    std::ostringstream contents;
    contents << in.rdbuf();

    // "class" (own) is written for Derived; "if" (inherited) is not --
    // it's implied by Derived's base-style list instead.
    std::string text = contents.str();
    auto derived_block = text.find("Style Derived");
    ASSERT_NE(derived_block, std::string::npos);
    std::string derived_section = text.substr(derived_block);
    EXPECT_NE(derived_section.find("Reserved => class"), std::string::npos);
    EXPECT_EQ(derived_section.find("Reserved => if"), std::string::npos);
}

TEST(SaveConfig, DefaultStyleExtensionMarkerIsAsterisk) {
    TempFile file;
    StyleSet styles;
    save_config(styles, file.path());

    std::ifstream in(file.path());
    std::ostringstream contents;
    contents << in.rdbuf();

    EXPECT_NE(contents.str().find("Style Default (*)"), std::string::npos);
}

TEST(SaveConfig, RoundTripsBaseStyleRelationship) {
    TempFile file;

    {
        StyleSet styles;
        Style& base = styles.get_or_create("Base");
        base.fore_color.set(Color::Green);
        Style& derived = styles.get_or_create("Derived");
        derived.add_base_style(base);
        save_config(styles, file.path());
    }

    StyleSet reloaded;
    ASSERT_TRUE(load_config(reloaded, file.path()));

    Style* derived = reloaded.find("Derived");
    ASSERT_NE(derived, nullptr);
    ASSERT_EQ(derived->base_style_names().size(), 1u);
    EXPECT_EQ(derived->base_style_names()[0], "Base");
    ASSERT_NE(derived->fore_color.get(), nullptr);
    EXPECT_EQ(*derived->fore_color.get(), Color::Green);
}
