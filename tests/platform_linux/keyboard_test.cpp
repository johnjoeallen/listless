#include "keyboard.hpp"

#include <gtest/gtest.h>
#include <ncurses.h>

#include <cstdlib>
#include <memory>

using listless::Keyboard;
using listless::Terminal;
namespace Key = listless::Key;

namespace {

// These inject real input via ncurses' own ungetch(), then exercise the
// real Keyboard::read_key()/key_available() -- no mocking.
class KeyboardTest : public ::testing::Test {
  protected:
    void SetUp() override {
        setenv("TERM", "xterm-256color", 1);
        terminal_ = std::make_unique<Terminal>();
        keyboard_ = std::make_unique<Keyboard>(*terminal_);
    }

    void TearDown() override {
        keyboard_.reset();
        terminal_.reset();
    }

    std::unique_ptr<Terminal> terminal_;
    std::unique_ptr<Keyboard> keyboard_;
};

}  // namespace

TEST_F(KeyboardTest, PlainCharacterPassesThroughUnchanged) {
    ungetch('a');
    EXPECT_EQ(keyboard_->read_key(), 'a');
}

TEST_F(KeyboardTest, ControlCharacterPassesThroughUnchanged) {
    ungetch(3);  // Ctrl+C
    EXPECT_EQ(keyboard_->read_key(), 3);
}

TEST_F(KeyboardTest, CursesSpecialKeyTranslatesToKeyConstant) {
    ungetch(KEY_UP);
    EXPECT_EQ(keyboard_->read_key(), Key::Up);
}

TEST_F(KeyboardTest, FunctionKeyTranslatesToKeyConstant) {
    ungetch(KEY_F(1));
    EXPECT_EQ(keyboard_->read_key(), Key::F1);
}

TEST_F(KeyboardTest, UnrecognizedSpecialKeyIsUnknown) {
    ungetch(KEY_MOUSE);  // never enabled/mapped
    EXPECT_EQ(keyboard_->read_key(), Key::Unknown);
}

TEST_F(KeyboardTest, LoneEscapeWithNoFollowUpIsEscape) {
    ungetch(27);
    EXPECT_EQ(keyboard_->read_key(), Key::Escape);
}

TEST_F(KeyboardTest, EscapeFollowedByLetterIsAltKey) {
    // ungetch() is a stack, so push in reverse of read order.
    ungetch('a');
    ungetch(27);
    EXPECT_EQ(keyboard_->read_key(), listless::alt_key('a'));
}

TEST_F(KeyboardTest, KeyAvailableReflectsPendingInputWithoutConsumingIt) {
    EXPECT_FALSE(keyboard_->key_available());

    ungetch('x');
    EXPECT_TRUE(keyboard_->key_available());
    EXPECT_EQ(keyboard_->read_key(), 'x');  // still there, not consumed by the check
    EXPECT_FALSE(keyboard_->key_available());
}
