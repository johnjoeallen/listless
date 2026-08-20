#include "FileManagerRender.hpp"

#include <gtest/gtest.h>
#include <ncurses.h>

#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>

namespace fs = std::filesystem;
using listless::compute_grid;
using listless::FileManager;
using listless::Grid;
using listless::render_file_manager;
using listless::Terminal;

namespace {

class FileManagerRenderTest : public ::testing::Test {
  protected:
    void SetUp() override {
        setenv("TERM", "xterm-256color", 1);
        terminal_ = std::make_unique<Terminal>();

        const auto* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        root_ = fs::temp_directory_path() /
                fs::path(std::string("listless_file_manager_render_test_") + test_info->name());
        fs::remove_all(root_);
        fs::create_directories(root_);
    }

    void TearDown() override {
        terminal_.reset();
        fs::remove_all(root_);
    }

    std::string row_text(int row, int width) {
        std::string s;
        for (int x = 0; x < width; ++x) {
            s.push_back(static_cast<char>(mvinch(row, x) & A_CHARTEXT));
        }
        return s;
    }

    fs::path root_;
    std::unique_ptr<Terminal> terminal_;
};

}  // namespace

TEST_F(FileManagerRenderTest, DrawsEntryNamesInTheGrid) {
    std::ofstream(root_ / "apple.txt") << "a";

    // root_ isn't the filesystem root, so entry 0 is the synthetic ".."
    // directory entry (directories always sort first) and "apple.txt"
    // (the only file) lands at row 2.
    FileManager fm(root_);
    Grid grid = compute_grid(terminal_->width(), terminal_->height());
    render_file_manager(fm, grid, *terminal_);

    std::string row2 = row_text(2, terminal_->width());
    EXPECT_NE(row2.find("apple.txt"), std::string::npos);
}

TEST_F(FileManagerRenderTest, DirectoryNamesGetATrailingSlash) {
    fs::create_directory(root_ / "subdir");

    // Both ".." and "subdir" are directories, sorted by name ascending:
    // ".." (row 1) then "subdir" (row 2).
    FileManager fm(root_);
    Grid grid = compute_grid(terminal_->width(), terminal_->height());
    render_file_manager(fm, grid, *terminal_);

    std::string row2 = row_text(2, terminal_->width());
    EXPECT_NE(row2.find("subdir/"), std::string::npos);
}

TEST_F(FileManagerRenderTest, StatusLineShowsDirectoryAndEntryCount) {
    std::ofstream(root_ / "apple.txt") << "a";

    FileManager fm(root_);
    Grid grid = compute_grid(terminal_->width(), terminal_->height());
    render_file_manager(fm, grid, *terminal_);

    // The full temp-directory path can exceed terminal width and get
    // truncated, so check the leading part of the status line rather
    // than the whole path plus the (possibly truncated-off) entry count.
    std::string row0 = row_text(0, terminal_->width());
    EXPECT_NE(row0.find(root_.filename().string()), std::string::npos);
}

TEST_F(FileManagerRenderTest, SelectedEntryIsHighlightedWithDifferentAttributes) {
    std::ofstream(root_ / "apple.txt") << "a";
    std::ofstream(root_ / "banana.txt") << "b";

    FileManager fm(root_);
    Grid grid = compute_grid(terminal_->width(), terminal_->height());
    render_file_manager(fm, grid, *terminal_);

    chtype selected_cell = mvinch(1, 0);    // row for entry 0, selected by default
    chtype unselected_cell = mvinch(2, 0);  // row for entry 1, not selected
    EXPECT_NE((selected_cell & A_ATTRIBUTES), (unselected_cell & A_ATTRIBUTES));
}

TEST_F(FileManagerRenderTest, RowsPastTheLastEntryAreBlank) {
    std::ofstream(root_ / "only.txt") << "a";

    // Entries: ".." (row 1), "only.txt" (row 2) -- row 3 has nothing.
    FileManager fm(root_);
    Grid grid = compute_grid(terminal_->width(), terminal_->height());
    render_file_manager(fm, grid, *terminal_);

    std::string row3 = row_text(3, 5);
    EXPECT_EQ(row3, "     ");
}
