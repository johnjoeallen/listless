#include <gtest/gtest.h>

#include <fstream>

#include "AppActions.hpp"

namespace fs = std::filesystem;
using listless::alt_key;
using listless::BrowsingAction;
using listless::DisplayMode;
using listless::FileManager;
using listless::Grid;
using listless::handle_browsing_key;
using listless::handle_viewing_key;
using listless::Viewer;
using listless::ViewingAction;
namespace Key = listless::Key;

namespace {

constexpr Grid kOneColumn{1, 100};

Viewer make_viewer(std::string content) { return Viewer(std::move(content), "test"); }

class BrowsingActionsTest : public ::testing::Test {
  protected:
    void SetUp() override {
        const auto* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        root_ = fs::temp_directory_path() /
                fs::path(std::string("listless_app_actions_test_") + test_info->name());
        fs::remove_all(root_);
        fs::create_directories(root_);

        std::ofstream(root_ / "apple.txt") << "a";
        fs::create_directory(root_ / "zsubdir");

        fm_ = std::make_unique<FileManager>(root_);
    }

    void TearDown() override { fs::remove_all(root_); }

    fs::path root_;
    std::unique_ptr<FileManager> fm_;
};

}  // namespace

// --- browsing --------------------------------------------------------

TEST_F(BrowsingActionsTest, DownArrowMovesSelection) {
    std::size_t before = fm_->selected_index();
    EXPECT_EQ(handle_browsing_key(*fm_, kOneColumn, Key::Down), BrowsingAction::None);
    EXPECT_NE(fm_->selected_index(), before);
}

std::size_t index_of(const FileManager& fm, std::string_view name) {
    for (std::size_t i = 0; i < fm.size(); ++i) {
        if (fm.entry(i).name == name) {
            return i;
        }
    }
    ADD_FAILURE() << "no entry named " << name;
    return 0;
}

TEST_F(BrowsingActionsTest, EnterOnDirectoryEntersItWithoutOpenSelected) {
    fm_->select(index_of(*fm_, "zsubdir"), kOneColumn);
    ASSERT_TRUE(fm_->selected().is_directory);

    EXPECT_EQ(handle_browsing_key(*fm_, kOneColumn, '\r'), BrowsingAction::None);
    EXPECT_EQ(fm_->current_directory(), root_ / "zsubdir");
}

TEST_F(BrowsingActionsTest, EnterOnFileReturnsOpenSelected) {
    fm_->select(index_of(*fm_, "apple.txt"), kOneColumn);
    ASSERT_FALSE(fm_->selected().is_directory);

    EXPECT_EQ(handle_browsing_key(*fm_, kOneColumn, '\r'), BrowsingAction::OpenSelected);
}

TEST_F(BrowsingActionsTest, QQuits) {
    EXPECT_EQ(handle_browsing_key(*fm_, kOneColumn, 'q'), BrowsingAction::Quit);
}

TEST_F(BrowsingActionsTest, EscapeQuits) {
    EXPECT_EQ(handle_browsing_key(*fm_, kOneColumn, Key::Escape), BrowsingAction::Quit);
}

TEST_F(BrowsingActionsTest, PrintableCharacterTypesAhead) {
    EXPECT_EQ(handle_browsing_key(*fm_, kOneColumn, 'a'), BrowsingAction::None);
    EXPECT_EQ(fm_->type_ahead_text(/*directories=*/false), "a");
    EXPECT_EQ(fm_->selected().name, "apple.txt");
}

TEST_F(BrowsingActionsTest, BackspaceRemovesLastTypeAheadCharacter) {
    handle_browsing_key(*fm_, kOneColumn, 'a');
    handle_browsing_key(*fm_, kOneColumn, 127);
    EXPECT_EQ(fm_->type_ahead_text(/*directories=*/false), "");
}

// --- viewing -----------------------------------------------------------

TEST(ViewingActionsTest, DownArrowScrollsTextMode) {
    Viewer v = make_viewer("a\nb\nc\nd\ne\n");
    EXPECT_EQ(handle_viewing_key(v, /*visible_lines=*/2, /*visible_width=*/80, Key::Down),
              ViewingAction::None);
    EXPECT_EQ(v.top_line(), 1);
}

TEST(ViewingActionsTest, HTogglesHexMode) {
    Viewer v = make_viewer("abcdefgh");
    handle_viewing_key(v, 10, 80, 'h');
    EXPECT_EQ(v.display_mode(), DisplayMode::Hex);
    handle_viewing_key(v, 10, 80, 'H');
    EXPECT_EQ(v.display_mode(), DisplayMode::Text);
}

TEST(ViewingActionsTest, SlashPromptsSearchForward) {
    Viewer v = make_viewer("abc\n");
    EXPECT_EQ(handle_viewing_key(v, 10, 80, '/'), ViewingAction::PromptSearchForward);
    EXPECT_EQ(handle_viewing_key(v, 10, 80, 's'), ViewingAction::PromptSearchForward);
}

TEST(ViewingActionsTest, FPromptsCaseInsensitiveSearchForward) {
    Viewer v = make_viewer("abc\n");
    EXPECT_EQ(handle_viewing_key(v, 10, 80, 'f'),
              ViewingAction::PromptSearchForwardCaseInsensitive);
}

TEST(ViewingActionsTest, ARepeatsPreviousSearchForward) {
    Viewer v = make_viewer("x\nneedle\nx\nneedle\n");
    ASSERT_TRUE(v.search_forward("needle", /*case_sensitive=*/true));
    ASSERT_EQ(v.selection().line, 1);

    EXPECT_EQ(handle_viewing_key(v, 10, 80, 'a'), ViewingAction::None);
    EXPECT_EQ(v.selection().line, 3);
}

TEST(ViewingActionsTest, GInHexModePromptsGotoOffset) {
    Viewer v = make_viewer(std::string(32, 'x'));
    v.switch_to_hex_mode();
    EXPECT_EQ(handle_viewing_key(v, 10, 80, 'g'), ViewingAction::PromptGotoOffset);
}

TEST(ViewingActionsTest, GInTextModeIsUnhandled) {
    Viewer v = make_viewer("abc\n");
    EXPECT_EQ(handle_viewing_key(v, 10, 80, 'g'), ViewingAction::None);
}

TEST(ViewingActionsTest, ColonInTextModePromptsGotoLine) {
    Viewer v = make_viewer("a\nb\nc\n");
    EXPECT_EQ(handle_viewing_key(v, 10, 80, ':'), ViewingAction::PromptGotoLine);
}

TEST(ViewingActionsTest, ColonInHexModeIsUnhandled) {
    Viewer v = make_viewer(std::string(32, 'x'));
    v.switch_to_hex_mode();
    EXPECT_EQ(handle_viewing_key(v, 10, 80, ':'), ViewingAction::None);
}

TEST(ViewingActionsTest, QClosesTheViewer) {
    Viewer v = make_viewer("abc\n");
    EXPECT_EQ(handle_viewing_key(v, 10, 80, 'q'), ViewingAction::Close);
    EXPECT_EQ(handle_viewing_key(v, 10, 80, Key::Escape), ViewingAction::Close);
}

TEST(ViewingActionsTest, PlainDigitJumpsToSetBookmark) {
    Viewer v = make_viewer("a\nb\nc\nd\ne\n");
    v.goto_line(3);
    v.set_bookmark(5);
    v.goto_line(0);

    EXPECT_EQ(handle_viewing_key(v, 10, 80, '5'), ViewingAction::None);
    EXPECT_EQ(v.top_line(), 3);
}

TEST(ViewingActionsTest, AltDigitSetsBookmarkAtCurrentPosition) {
    Viewer v = make_viewer("a\nb\nc\nd\ne\n");
    v.goto_line(2);

    EXPECT_EQ(handle_viewing_key(v, 10, 80, alt_key('3')), ViewingAction::None);
    EXPECT_EQ(v.bookmark(3).line, 2);
}
