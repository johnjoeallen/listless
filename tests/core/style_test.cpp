#include "style.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

using listless::Color;
using listless::cycle_color;
using listless::default_config_path;
using listless::load_config;
using listless::save_config;
using listless::Style;
using listless::StyleDisplayMode;
using listless::StyleSet;

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

TEST(LoadConfig, MissingFileReturnsFalseWithoutModifyingStyles) {
    StyleSet styles;
    EXPECT_FALSE(load_config(styles, "/nonexistent/path/style.conf"));
    EXPECT_EQ(styles.styles().size(), 1u);
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
