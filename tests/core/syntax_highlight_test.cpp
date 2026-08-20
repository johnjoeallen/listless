#include <gtest/gtest.h>

#include <algorithm>

#include "Style.hpp"
#include "SyntaxHighlight.hpp"

using listless::Color;
using listless::ColorSpan;
using listless::highlight_line;
using listless::HighlightState;
using listless::Style;

namespace {

// A C-like style: symbols, reserved words, `/* */` and `//` comments,
// `#`-preprocessor with `\`-continuation, double-quoted strings with
// `\`-escaping, and `0x`-prefixed hex numbers -- enough to exercise
// every branch of `highlight_line` without depending on a real config
// file (subsystem 08 covers loading; this exercises the tokenizer).
void init_c_style(Style& s) {
    s.syntax_highlight_enabled.set(true);
    s.symbols.set("(){};,");
    s.string_delimiter.set("\"'");
    s.escape.set('\\');
    s.add_open_comment("/*");
    s.add_close_comment("*/");
    s.add_eol_comment("//");
    s.add_numeric_prefix("0x");
    s.open_preprocessor.set("#");
    s.line_continuation.set('\\');
    s.case_sensitive.set(true);
    s.add_reserved_word("if");
    s.add_reserved_word("return");

    s.comment_color.set(Color::Green);
    s.preprocessor_color.set(Color::Magenta);
    s.string_color.set(Color::Red);
    s.symbols_color.set(Color::Cyan);
    s.number_color.set(Color::Yellow);
    s.reserved_color.set(Color::Blue);
    s.ident_color.set(Color::LightGray);
    s.fore_color.set(Color::White);
}

}  // namespace

TEST(HighlightLine, PlainIdentifierWhenNoRulesMatch) {
    Style s("C");
    init_c_style(s);
    HighlightState state;

    auto spans = highlight_line("xyz", s, state);

    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0].offset, 0u);
    EXPECT_EQ(spans[0].length, 3u);
    EXPECT_EQ(spans[0].color, Color::LightGray);
}

TEST(HighlightLine, BlockTextSuppressesContextualRulesUntilIndentationReturns) {
    Style s("Generic");
    s.syntax_highlight_enabled.set(true);
    s.before_delimiter.set(":");
    s.block_text_start.set("|");
    s.block_text_color.set(Color::Magenta);
    s.reserved_color.set(Color::Blue);
    s.fore_color.set(Color::White);
    HighlightState state;

    highlight_line("description: |", s, state);
    auto block = highlight_line("  http://example.test", s, state);
    ASSERT_EQ(block.size(), 1u);
    EXPECT_EQ(block[0].color, Color::Magenta);

    auto key = highlight_line("name: value", s, state);
    ASSERT_GE(key.size(), 1u);
    EXPECT_EQ(key[0].color, Color::Blue);
}

TEST(HighlightLine, BlockTextMarkerMustBeAScalarValue) {
    Style s("Generic");
    s.syntax_highlight_enabled.set(true);
    s.before_delimiter.set(":");
    s.before_delimiter_requires_space.set(true);
    s.block_text_start.set("|");
    s.fore_color.set(Color::White);
    s.reserved_color.set(Color::Blue);
    HighlightState state;

    highlight_line("name: value | metadata", s, state);
    auto next = highlight_line("next: value", s, state);
    EXPECT_NE(next.size(), 1u);
}

TEST(HighlightLine, BlockTextIndentIndicatorSetsMinimumContentIndentation) {
    Style s("Generic");
    s.syntax_highlight_enabled.set(true);
    s.before_delimiter.set(":");
    s.before_delimiter_requires_space.set(true);
    s.block_text_start.set("|");
    s.block_text_color.set(Color::Yellow);
    s.reserved_color.set(Color::Blue);

    HighlightState shallow_state;
    highlight_line("text: |2", s, shallow_state);
    auto shallow = highlight_line(" value: plain", s, shallow_state);
    EXPECT_NE(shallow[0].color, Color::Yellow);

    HighlightState content_state;
    highlight_line("text: |2", s, content_state);
    auto content = highlight_line("  value: plain", s, content_state);
    ASSERT_EQ(content.size(), 1u);
    EXPECT_EQ(content[0].color, Color::Yellow);
}

TEST(HighlightLine, PositionalPrefixRulesHighlightStructuralTokens) {
    Style s("Generic");
    s.syntax_highlight_enabled.set(true);
    s.line_start_prefix.set("-");
    s.line_start_prefix_requires_space.set(true);
    s.prefix_token.set("&!");
    s.reserved_color.set(Color::Blue);
    HighlightState state;

    auto sequence = highlight_line("  - item", s, state);
    ASSERT_GE(sequence.size(), 1u);
    EXPECT_TRUE(std::any_of(sequence.begin(), sequence.end(),
                            [](const ColorSpan& span) { return span.color == Color::Blue; }));

    auto anchor = highlight_line("value: &shared", s, state);
    ASSERT_GE(anchor.size(), 2u);
    EXPECT_EQ(anchor.back().color, Color::Blue);
}

TEST(HighlightLine, BeforeDelimiterCanRequireWhitespaceAfterTheDelimiter) {
    Style s("Generic");
    s.syntax_highlight_enabled.set(true);
    s.before_delimiter.set(":");
    s.before_delimiter_requires_space.set(true);
    s.reserved_color.set(Color::Blue);
    HighlightState state;

    auto scalar = highlight_line("- write:pets", s, state);
    EXPECT_TRUE(std::none_of(scalar.begin(), scalar.end(),
                             [](const ColorSpan& span) { return span.color == Color::Blue; }));
}

TEST(HighlightLine, SequenceDataAndSequenceMappingsUseDifferentRules) {
    Style s("Generic");
    s.syntax_highlight_enabled.set(true);
    s.before_delimiter.set(":");
    s.before_delimiter_requires_space.set(true);
    s.before_delimiter_color.set(Color::Blue);
    s.line_start_prefix.set("-");
    s.line_start_prefix_requires_space.set(true);
    s.line_start_data_color.set(Color::Green);
    HighlightState state;

    auto scalar = highlight_line("- write:pets", s, state);
    ASSERT_GE(scalar.size(), 2u);
    EXPECT_EQ(scalar.back().color, Color::Green);

    auto mapping = highlight_line("- write: pets", s, state);
    EXPECT_TRUE(std::any_of(mapping.begin(), mapping.end(),
                            [](const ColorSpan& span) { return span.color == Color::Blue; }));
}

TEST(HighlightLine, SequenceDataCanStartWithAPrefixToken) {
    Style s("Generic");
    s.syntax_highlight_enabled.set(true);
    s.line_start_prefix.set("-");
    s.line_start_prefix_requires_space.set(true);
    s.line_start_data_color.set(Color::Green);
    s.prefix_token.set("&");
    s.prefix_token_color.set(Color::Blue);
    HighlightState state;

    auto spans = highlight_line("- &shared value", s, state);
    EXPECT_TRUE(std::any_of(spans.begin(), spans.end(),
                            [](const ColorSpan& span) { return span.color == Color::Blue; }));
    EXPECT_EQ(spans.back().color, Color::Green);
}

TEST(HighlightLine, PrefixTokensSupportStructuredTextAnnotations) {
    Style s("Generic");
    s.syntax_highlight_enabled.set(true);
    s.prefix_token.set("&*!%");
    s.prefix_token_color.set(Color::Blue);
    HighlightState state;

    for (std::string_view text : {"&defaults", "*defaults", "!custom", "%YAML 1.2"}) {
        auto spans = highlight_line(text, s, state);
        ASSERT_FALSE(spans.empty());
        EXPECT_EQ(spans.front().color, Color::Blue) << text;
    }
}

TEST(HighlightLine, BeforeDelimiterIgnoresQuotedScalarContent) {
    Style s("Generic");
    s.syntax_highlight_enabled.set(true);
    s.before_delimiter.set(":");
    s.before_delimiter_requires_space.set(true);
    s.line_start_prefix.set("-");
    s.string_delimiter.set("\"");
    s.string_color.set(Color::Green);
    s.reserved_color.set(Color::Blue);
    HighlightState state;

    auto spans = highlight_line("- \"write: pets\"", s, state);
    EXPECT_TRUE(std::none_of(spans.begin(), spans.end(), [](const ColorSpan& span) {
        return span.color == Color::Blue && span.length > 1;
    }));
}

TEST(HighlightLine, BeforeDelimiterHighlightsQuotedKeys) {
    Style s("Generic");
    s.syntax_highlight_enabled.set(true);
    s.before_delimiter.set(":");
    s.string_delimiter.set("\"");
    s.before_delimiter_color.set(Color::Blue);
    s.string_color.set(Color::Green);
    HighlightState state;

    auto spans = highlight_line("\"name\": \"value\"", s, state);
    ASSERT_GE(spans.size(), 2u);
    EXPECT_EQ(spans.front().color, Color::Blue);
    EXPECT_EQ(spans.back().color, Color::Green);
}

TEST(HighlightLine, ReservedWordRequiresWordBoundary) {
    Style s("C");
    init_c_style(s);
    HighlightState state;

    auto spans = highlight_line("iffy", s, state);
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0].color, Color::LightGray);  // identifier, not "if"

    HighlightState state2;
    auto spans2 = highlight_line("if(x)", s, state2);
    ASSERT_EQ(spans2.size(), 4u);
    EXPECT_EQ(spans2[0].length, 2u);
    EXPECT_EQ(spans2[0].color, Color::Blue);       // "if"
    EXPECT_EQ(spans2[1].color, Color::Cyan);       // "("
    EXPECT_EQ(spans2[2].color, Color::LightGray);  // "x"
    EXPECT_EQ(spans2[3].color, Color::Cyan);       // ")"
}

TEST(HighlightLine, ReservedWordCaseInsensitive) {
    Style s("C");
    init_c_style(s);
    s.case_sensitive.set(false);
    HighlightState state;

    auto spans = highlight_line("RETURN;", s, state);
    ASSERT_EQ(spans.size(), 2u);
    EXPECT_EQ(spans[0].color, Color::Blue);
}

TEST(HighlightLine, HexNumber) {
    Style s("C");
    init_c_style(s);
    HighlightState state;

    auto spans = highlight_line("0xFF", s, state);
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0].length, 4u);
    EXPECT_EQ(spans[0].color, Color::Yellow);
}

TEST(HighlightLine, DecimalNumber) {
    Style s("C");
    init_c_style(s);
    HighlightState state;

    auto spans = highlight_line("123", s, state);
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0].color, Color::Yellow);
}

TEST(HighlightLine, StringWithEscape) {
    Style s("C");
    init_c_style(s);
    HighlightState state;

    auto spans = highlight_line(R"("a\"b")", s, state);
    // Every span should be string-coloured, spanning the whole line.
    std::size_t total = 0;
    for (const auto& span : spans) {
        EXPECT_EQ(span.color, Color::Red);
        total += span.length;
    }
    EXPECT_EQ(total, 6u);
    EXPECT_FALSE(state.in_comment);
}

TEST(HighlightLine, EolCommentConsumesRestOfLine) {
    Style s("C");
    init_c_style(s);
    HighlightState state;

    auto spans = highlight_line("x = 1 // trailing", s, state);
    ASSERT_FALSE(spans.empty());
    EXPECT_EQ(spans.back().color, Color::Green);
    EXPECT_EQ(spans.back().offset + spans.back().length, 17u);
    EXPECT_FALSE(state.in_comment);  // EOL comments don't persist
}

TEST(HighlightLine, MultiLineCommentPersistsAcrossLines) {
    Style s("C");
    init_c_style(s);
    HighlightState state;

    auto first = highlight_line("int x; /* start of a", s, state);
    EXPECT_TRUE(state.in_comment);
    ASSERT_FALSE(first.empty());
    EXPECT_EQ(first.back().color, Color::Green);

    auto second = highlight_line("comment that continues", s, state);
    EXPECT_TRUE(state.in_comment);
    for (const auto& span : second) {
        EXPECT_EQ(span.color, Color::Green);
    }

    auto third = highlight_line("end */ int y;", s, state);
    EXPECT_FALSE(state.in_comment);
    ASSERT_FALSE(third.empty());
    EXPECT_EQ(third.front().color, Color::Green);
    // "int" after the closing "*/" is a plain identifier again.
    bool saw_ident = false;
    for (const auto& span : third) {
        if (span.color == Color::LightGray) saw_ident = true;
    }
    EXPECT_TRUE(saw_ident);
}

TEST(HighlightLine, PreprocessorRequiresLineStart) {
    Style s("C");
    init_c_style(s);
    HighlightState state;

    auto spans = highlight_line("#define X 1", s, state);
    ASSERT_FALSE(spans.empty());
    EXPECT_EQ(spans.front().color, Color::Magenta);
    EXPECT_FALSE(state.in_preprocessor);  // no trailing continuation
}

TEST(HighlightLine, PreprocessorNotRecognizedMidLine) {
    Style s("C");
    init_c_style(s);
    HighlightState state;

    // '#' after non-space content should not start a preprocessor run.
    auto spans = highlight_line("x #define", s, state);
    bool saw_preprocessor = false;
    for (const auto& span : spans) {
        if (span.color == Color::Magenta) saw_preprocessor = true;
    }
    EXPECT_FALSE(saw_preprocessor);
}

TEST(HighlightLine, PreprocessorContinuationPersistsAcrossLines) {
    Style s("C");
    init_c_style(s);
    HighlightState state;

    auto first = highlight_line("#define LONG(a, b) \\", s, state);
    EXPECT_TRUE(state.in_preprocessor);
    ASSERT_FALSE(first.empty());
    EXPECT_EQ(first.front().color, Color::Magenta);

    auto second = highlight_line("    (a) + (b)", s, state);
    EXPECT_FALSE(state.in_preprocessor);
    ASSERT_FALSE(second.empty());
    EXPECT_EQ(second.front().color, Color::Magenta);
}

TEST(HighlightLine, SyntaxHighlightingDisabledYieldsSingleDefaultSpan) {
    Style s("C");
    init_c_style(s);
    s.syntax_highlight_enabled.set(false);
    HighlightState state;

    auto spans = highlight_line("if (x) return; // not a keyword here", s, state);
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0].color, Color::White);  // fore_color, not reserved_color
}

TEST(HighlightLine, NoReservedWordsFallsBackToLayoutMode) {
    Style s("Plain");
    s.syntax_highlight_enabled.set(true);  // enabled, but no reserved words
    s.fore_color.set(Color::White);
    HighlightState state;

    auto spans = highlight_line("anything at all", s, state);
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0].color, Color::White);
}

TEST(HighlightLine, BoldToggleByteTracksAcrossLines) {
    Style s("Layout");
    s.text_with_layout.set(true);
    s.fore_color.set(Color::White);
    s.bold_color.set(Color::LightRed);
    HighlightState state;

    std::string bold_code(1, static_cast<char>(('B' - 'A') + 1));
    auto spans = highlight_line(bold_code + "bold text", s, state);

    EXPECT_TRUE(state.bold);
    ASSERT_EQ(spans.size(), 1u);  // the toggle byte itself is consumed
    EXPECT_EQ(spans[0].offset, 1u);
    EXPECT_EQ(spans[0].length, 9u);
    EXPECT_EQ(spans[0].color, Color::LightRed);
    EXPECT_TRUE(spans[0].bold);

    auto next = highlight_line("still bold", s, state);
    EXPECT_TRUE(state.bold);
    ASSERT_FALSE(next.empty());
    EXPECT_EQ(next.front().color, Color::LightRed);
}

TEST(HighlightLine, UnderlineToggleByte) {
    Style s("Layout");
    s.text_with_layout.set(true);
    s.fore_color.set(Color::White);
    s.underline_color.set(Color::LightCyan);
    HighlightState state;

    std::string underline_code(1, static_cast<char>(('S' - 'A') + 1));
    auto spans = highlight_line("plain " + underline_code + "under", s, state);

    ASSERT_FALSE(spans.empty());
    EXPECT_TRUE(state.underlined);
    EXPECT_EQ(spans.back().color, Color::LightCyan);
    EXPECT_TRUE(spans.back().underlined);
}

TEST(HighlightLine, ToggleBytesIgnoredWithoutTextWithLayout) {
    Style s("Plain");
    s.fore_color.set(Color::White);
    HighlightState state;

    std::string bold_code(1, static_cast<char>(('B' - 'A') + 1));
    auto spans = highlight_line(bold_code + "text", s, state);

    EXPECT_FALSE(state.bold);
    ASSERT_EQ(spans.size(), 1u);  // toggle byte not consumed, displayed as-is; merges with rest
    EXPECT_EQ(spans[0].length, 5u);
    EXPECT_EQ(spans[0].color, Color::White);
}

TEST(HighlightLine, EmptyLineProducesNoSpans) {
    Style s("C");
    init_c_style(s);
    HighlightState state;

    auto spans = highlight_line("", s, state);
    EXPECT_TRUE(spans.empty());
    EXPECT_FALSE(state.in_comment);
    EXPECT_FALSE(state.in_preprocessor);
}

TEST(HighlightLine, StyleForExtensionSelectsHighlightRules) {
    listless::StyleSet styles;
    Style& c_style = styles.get_or_create("C");
    c_style.add_extension(".c");
    c_style.syntax_highlight_enabled.set(true);
    c_style.add_reserved_word("if");
    c_style.reserved_color.set(Color::Blue);
    c_style.fore_color.set(Color::White);

    listless::Style* found = styles.style_for_extension(".c");
    ASSERT_NE(found, nullptr);

    HighlightState state;
    auto spans = highlight_line("if", *found, state);
    ASSERT_EQ(spans.size(), 1u);
    EXPECT_EQ(spans[0].color, Color::Blue);
}
