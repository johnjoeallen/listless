#include "line_edit.hpp"

#include <gtest/gtest.h>

using listless::line_edit_key;
using listless::LineEditStatus;

TEST(LineEdit, AppendsPrintableCharacters) {
    std::string text;
    EXPECT_EQ(line_edit_key(text, 'h'), LineEditStatus::Editing);
    EXPECT_EQ(line_edit_key(text, 'i'), LineEditStatus::Editing);
    EXPECT_EQ(text, "hi");
}

TEST(LineEdit, IgnoresNonPrintableControlKeys) {
    std::string text = "hi";
    EXPECT_EQ(line_edit_key(text, 1), LineEditStatus::Editing);
    EXPECT_EQ(text, "hi");
}

TEST(LineEdit, BackspaceRemovesLastCharacter) {
    std::string text = "hi";
    EXPECT_EQ(line_edit_key(text, 127), LineEditStatus::Editing);
    EXPECT_EQ(text, "h");
    EXPECT_EQ(line_edit_key(text, 8), LineEditStatus::Editing);
    EXPECT_EQ(text, "");
}

TEST(LineEdit, BackspaceOnEmptyTextIsANoOp) {
    std::string text;
    EXPECT_EQ(line_edit_key(text, 127), LineEditStatus::Editing);
    EXPECT_EQ(text, "");
}

TEST(LineEdit, EnterSubmitsWithoutModifyingText) {
    std::string text = "hi";
    EXPECT_EQ(line_edit_key(text, '\r'), LineEditStatus::Submitted);
    EXPECT_EQ(text, "hi");

    text = "hi";
    EXPECT_EQ(line_edit_key(text, '\n'), LineEditStatus::Submitted);
    EXPECT_EQ(text, "hi");
}

TEST(LineEdit, EscapeCancelsWithoutModifyingText) {
    std::string text = "hi";
    EXPECT_EQ(line_edit_key(text, listless::Key::Escape), LineEditStatus::Cancelled);
    EXPECT_EQ(text, "hi");
}
