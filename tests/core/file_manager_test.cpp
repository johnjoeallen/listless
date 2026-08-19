#include "file_manager.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;
using listless::compute_grid;
using listless::FileManager;
using listless::Grid;
using listless::SortKey;

namespace {

class FileManagerTest : public ::testing::Test {
  protected:
    void SetUp() override {
        const auto* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        root_ = fs::temp_directory_path() /
                fs::path(std::string("listless_file_manager_test_") + test_info->name());
        fs::remove_all(root_);
        fs::create_directories(root_);

        touch(root_ / "banana.txt", "small");
        touch(root_ / "apple.cpp", "a bigger file than the others");
        touch(root_ / "cherry.cpp", "mid");
        fs::create_directory(root_ / "zsubdir");
        fs::create_directory(root_ / "asubdir");
        fs::create_directory(root_ / "asubdir" / "child");
    }

    void TearDown() override { fs::remove_all(root_); }

    static void touch(const fs::path& p, std::string_view content) { std::ofstream(p) << content; }

    fs::path root_;
};

// Names of every entry, in listed order.
std::vector<std::string> names(const FileManager& fm) {
    std::vector<std::string> result;
    for (std::size_t i = 0; i < fm.size(); ++i) {
        result.push_back(fm.entry(i).name);
    }
    return result;
}

constexpr Grid kOneColumn{1, 100};

}  // namespace

TEST_F(FileManagerTest, ListsDirectoriesAndFilesByDefault) {
    FileManager fm(root_);

    // root_ is not the filesystem root, so ".." is present too.
    EXPECT_EQ(fm.size(), 6u);
}

TEST_F(FileManagerTest, DirectoriesAreListedUnfilteredByFileSpec) {
    FileManager fm(root_);
    fm.set_file_spec("*.cpp");

    // Both files match *.cpp is false for banana.txt, but directories are
    // never filtered by file_spec.
    auto listed = names(fm);
    EXPECT_NE(std::find(listed.begin(), listed.end(), "zsubdir"), listed.end());
    EXPECT_NE(std::find(listed.begin(), listed.end(), "asubdir"), listed.end());
    EXPECT_NE(std::find(listed.begin(), listed.end(), "apple.cpp"), listed.end());
    EXPECT_EQ(std::find(listed.begin(), listed.end(), "banana.txt"), listed.end());
}

TEST_F(FileManagerTest, DirectoriesAlwaysSortBeforeFilesAndByNameAscending) {
    FileManager fm(root_);
    fm.set_sort(SortKey::Size, /*ascending=*/false);

    // Regardless of the file sort key/direction, directories come first,
    // sorted by name ascending: "..", asubdir, zsubdir.
    auto listed = names(fm);
    ASSERT_GE(listed.size(), 3u);
    EXPECT_EQ(listed[0], "..");
    EXPECT_EQ(listed[1], "asubdir");
    EXPECT_EQ(listed[2], "zsubdir");
}

TEST_F(FileManagerTest, SortByNameAscending) {
    FileManager fm(root_);
    fm.set_file_spec("*.cpp");
    fm.set_sort(SortKey::Name, true);

    auto listed = names(fm);
    ASSERT_EQ(listed.size(), 5u);  // "..", asubdir, zsubdir, apple.cpp, cherry.cpp
    EXPECT_EQ(listed[3], "apple.cpp");
    EXPECT_EQ(listed[4], "cherry.cpp");
}

TEST_F(FileManagerTest, SortByNameDescendingReversesFilesOnly) {
    FileManager fm(root_);
    fm.set_file_spec("*.cpp");
    fm.set_sort(SortKey::Name, false);

    auto listed = names(fm);
    ASSERT_EQ(listed.size(), 5u);
    EXPECT_EQ(listed[3], "cherry.cpp");
    EXPECT_EQ(listed[4], "apple.cpp");
    // Directories are still ascending, unaffected by the flag.
    EXPECT_EQ(listed[1], "asubdir");
    EXPECT_EQ(listed[2], "zsubdir");
}

TEST_F(FileManagerTest, SortBySize) {
    // apple.cpp (30 bytes) > cherry.cpp (3 bytes) > banana.txt (5 bytes)
    // is not quite right -- recompute from the actual SetUp() contents:
    // banana.txt="small"(5), apple.cpp="a bigger file than the others"(30),
    // cherry.cpp="mid"(3).
    FileManager fm(root_);
    fm.set_sort(SortKey::Size, true);

    auto listed = names(fm);
    ASSERT_EQ(listed.size(), 6u);
    EXPECT_EQ(listed[3], "cherry.cpp");
    EXPECT_EQ(listed[4], "banana.txt");
    EXPECT_EQ(listed[5], "apple.cpp");
}

TEST_F(FileManagerTest, SortByDateOrdersOldestFirstWhenAscending) {
    touch(root_ / "older.log", "x");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    touch(root_ / "newer.log", "x");

    FileManager fm(root_);
    fm.set_file_spec("*.log");
    fm.set_sort(SortKey::Date, true);

    auto listed = names(fm);
    ASSERT_EQ(listed.size(), 5u);  // "..", asubdir, zsubdir, older.log, newer.log
    EXPECT_EQ(listed[3], "older.log");
    EXPECT_EQ(listed[4], "newer.log");
}

TEST_F(FileManagerTest, SortByExtensionTreatsDotDotAsExtensionless) {
    FileManager fm(root_);
    fm.set_sort(SortKey::Extension, true);

    // ".." must not crash/misbehave despite containing a literal '.'.
    auto listed = names(fm);
    EXPECT_NE(std::find(listed.begin(), listed.end(), ".."), listed.end());
}

TEST_F(FileManagerTest, ChangeDirectoryFailsForNonDirectory) {
    FileManager fm(root_);
    EXPECT_FALSE(fm.change_directory(root_ / "apple.cpp"));
    EXPECT_EQ(fm.current_directory(), root_);
}

TEST_F(FileManagerTest, ChangeDirectorySucceedsAndResetsSelection) {
    FileManager fm(root_);
    fm.select(fm.size() - 1, kOneColumn);

    ASSERT_TRUE(fm.change_directory(root_ / "asubdir"));
    EXPECT_EQ(fm.current_directory(), root_ / "asubdir");
    EXPECT_EQ(fm.selected_index(), 0u);
    EXPECT_EQ(fm.directory_history().back(), root_);
}

TEST_F(FileManagerTest, ChangeDirectoryDoesNotChangeProcessCwd) {
    fs::path before = fs::current_path();
    FileManager fm(root_);
    fm.change_directory(root_ / "asubdir");
    EXPECT_EQ(fs::current_path(), before);
}

TEST_F(FileManagerTest, EnterSelectedNavigatesIntoDirectory) {
    FileManager fm(root_);
    fm.set_sort(SortKey::Name, true);

    // asubdir sorts right after "..".
    fm.select(1, kOneColumn);
    ASSERT_EQ(fm.selected().name, "asubdir");

    ASSERT_TRUE(fm.enter_selected(kOneColumn));
    EXPECT_EQ(fm.current_directory(), root_ / "asubdir");
}

TEST_F(FileManagerTest, EnterSelectedOnDotDotReselectsThePreviousDirectory) {
    FileManager fm(root_ / "asubdir");

    // Select "..", go up, and expect "asubdir" to be reselected in root_.
    ASSERT_TRUE(fm.enter_selected(kOneColumn));
    EXPECT_EQ(fm.current_directory(), root_);
    EXPECT_EQ(fm.selected().name, "asubdir");
}

TEST_F(FileManagerTest, EnterSelectedFailsOnAFile) {
    FileManager fm(root_);
    fm.set_file_spec("*.cpp");
    fm.set_sort(SortKey::Name, true);
    fm.select(3, kOneColumn);  // apple.cpp
    ASSERT_FALSE(fm.selected().is_directory);

    EXPECT_FALSE(fm.enter_selected(kOneColumn));
    EXPECT_EQ(fm.current_directory(), root_);
}

TEST_F(FileManagerTest, SetFileSpecFiltersFilesAndResetsSelection) {
    FileManager fm(root_);
    fm.select(fm.size() - 1, kOneColumn);

    fm.set_file_spec("*.txt");

    EXPECT_EQ(fm.file_spec(), "*.txt");
    EXPECT_EQ(fm.selected_index(), 0u);
    auto listed = names(fm);
    EXPECT_NE(std::find(listed.begin(), listed.end(), "banana.txt"), listed.end());
    EXPECT_EQ(std::find(listed.begin(), listed.end(), "apple.cpp"), listed.end());
}

// --- Grid navigation -------------------------------------------------

TEST(FileManagerGridTest, ComputeGridSearchesFromRequestedMinusOne) {
    // Default request (3): first candidate is 2 columns.
    Grid g = compute_grid(/*screen_width=*/80, /*screen_height=*/24, /*requested_columns=*/3);
    EXPECT_EQ(g.columns, 2);
    EXPECT_EQ(g.lines_per_column, 21);

    // Alt+1 (2): first candidate is 1 column.
    Grid alt1 = compute_grid(80, 24, 2);
    EXPECT_EQ(alt1.columns, 1);

    // Alt+3 (4): first candidate is 3 columns, and 80/3 == 26 < 40, so it
    // falls back to 2.
    Grid alt3 = compute_grid(80, 24, 4);
    EXPECT_EQ(alt3.columns, 2);

    // A very wide screen keeps 3 columns for Alt+3.
    Grid alt3_wide = compute_grid(200, 24, 4);
    EXPECT_EQ(alt3_wide.columns, 3);
}

TEST_F(FileManagerTest, MoveDownAndUpWithinOneColumn) {
    FileManager fm(root_);
    Grid grid{1, 100};

    EXPECT_TRUE(fm.move_down(grid));
    EXPECT_EQ(fm.selected_index(), 1u);
    EXPECT_TRUE(fm.move_up(grid));
    EXPECT_EQ(fm.selected_index(), 0u);
    EXPECT_FALSE(fm.move_up(grid));  // already at index 0
}

TEST_F(FileManagerTest, MoveDownStopsAtLastEntry) {
    FileManager fm(root_);
    Grid grid{1, 100};

    while (fm.move_down(grid)) {
    }
    EXPECT_EQ(fm.selected_index(), fm.size() - 1);
    EXPECT_FALSE(fm.move_down(grid));
}

TEST_F(FileManagerTest, LeftRightMoveByOneColumnAndScrollTheView) {
    FileManager fm(root_);
    ASSERT_EQ(fm.size(), 6u);
    Grid grid{2, 2};  // 2 visible columns of 2 rows -> 4 visible of 6 total

    ASSERT_TRUE(fm.move_right(grid));  // 0 -> 2 (column 1)
    EXPECT_EQ(fm.selected_index(), 2u);
    EXPECT_EQ(fm.view_column(), 0);

    ASSERT_TRUE(fm.move_right(grid));  // 2 -> 4 (column 2, scrolls into view)
    EXPECT_EQ(fm.selected_index(), 4u);
    EXPECT_EQ(fm.view_column(), 1);

    ASSERT_TRUE(fm.move_left(grid));  // 4 -> 2, which is still visible (view spans [2,5])
    EXPECT_EQ(fm.selected_index(), 2u);
    EXPECT_EQ(fm.view_column(), 1);
}

TEST_F(FileManagerTest, HomeJumpsToTopOfVisibleColumnThenToTrueHome) {
    FileManager fm(root_);
    Grid grid{2, 2};

    fm.select(5, grid);  // forces a scroll: view_column becomes 1
    ASSERT_EQ(fm.view_column(), 1);

    ASSERT_TRUE(fm.move_home(grid));
    EXPECT_EQ(fm.selected_index(), 2u);  // top of the currently visible column

    ASSERT_TRUE(fm.move_home(grid));
    EXPECT_EQ(fm.selected_index(), 0u);  // now jumps all the way home
    EXPECT_EQ(fm.view_column(), 0);
}

TEST_F(FileManagerTest, EndJumpsToBottomOfVisibleColumnThenToTrueEnd) {
    FileManager fm(root_);
    ASSERT_EQ(fm.size(), 6u);
    Grid grid{2, 2};  // visible bottom at index 3

    ASSERT_TRUE(fm.move_end(grid));
    EXPECT_EQ(fm.selected_index(), 3u);

    ASSERT_TRUE(fm.move_end(grid));
    EXPECT_EQ(fm.selected_index(), 5u);  // true end
}

TEST_F(FileManagerTest, PageUpAndPageDownMoveByLinesPerColumn) {
    FileManager fm(root_);
    Grid grid{2, 2};

    ASSERT_TRUE(fm.move_page_down(grid));
    EXPECT_EQ(fm.selected_index(), 2u);
    EXPECT_EQ(fm.view_column(), 1);

    ASSERT_TRUE(fm.move_page_up(grid));
    EXPECT_EQ(fm.selected_index(), 0u);
    EXPECT_EQ(fm.view_column(), 0);
}

// --- Type-ahead multi-select ------------------------------------------

TEST_F(FileManagerTest, TypeAheadSelectsMatchingFileByPrefix) {
    FileManager fm(root_);
    fm.set_file_spec("*.cpp");
    fm.set_sort(SortKey::Name, true);
    Grid grid{1, 100};

    EXPECT_TRUE(fm.type_ahead_append('c', /*directories=*/false, grid));
    EXPECT_EQ(fm.selected().name, "cherry.cpp");
    EXPECT_EQ(fm.type_ahead_text(false), "c");
}

TEST_F(FileManagerTest, TypeAheadRejectsNonMatchingCharacterAndKeepsPriorText) {
    FileManager fm(root_);
    fm.set_file_spec("*.cpp");
    Grid grid{1, 100};

    ASSERT_TRUE(fm.type_ahead_append('c', false, grid));
    EXPECT_FALSE(fm.type_ahead_append('x', false, grid));
    EXPECT_EQ(fm.type_ahead_text(false), "c");  // 'x' was not appended
}

TEST_F(FileManagerTest, TypeAheadDirectorySwitchJumpsOffAFileFirst) {
    FileManager fm(root_);
    fm.set_file_spec("*.cpp");
    fm.set_sort(SortKey::Name, true);
    Grid grid{1, 100};

    fm.select(3, grid);  // apple.cpp, not a directory
    ASSERT_FALSE(fm.selected().is_directory);

    EXPECT_TRUE(fm.type_ahead_append('z', /*directories=*/true, grid));
    EXPECT_EQ(fm.selected().name, "zsubdir");
}

TEST_F(FileManagerTest, TypeAheadBackspaceRemovesLastCharacterAndResearches) {
    FileManager fm(root_);
    fm.set_file_spec("*.cpp");
    Grid grid{1, 100};

    ASSERT_TRUE(fm.type_ahead_append('c', false, grid));
    ASSERT_TRUE(fm.type_ahead_backspace(false, grid));
    EXPECT_EQ(fm.type_ahead_text(false), "");
}

TEST_F(FileManagerTest, ClearTypeAheadResetsBothAccumulators) {
    FileManager fm(root_);
    Grid grid{1, 100};

    fm.type_ahead_append('a', false, grid);
    fm.type_ahead_append('a', true, grid);
    fm.clear_type_ahead();

    EXPECT_EQ(fm.type_ahead_text(false), "");
    EXPECT_EQ(fm.type_ahead_text(true), "");
}

TEST_F(FileManagerTest, TypeAheadForwardSearchWrapsAround) {
    FileManager fm(root_);
    fm.set_file_spec("*.cpp");
    fm.set_sort(SortKey::Name, true);
    Grid grid{1, 100};

    // Start at the last entry (cherry.cpp); searching for "a" (apple.cpp,
    // earlier in the list) should still succeed via wraparound.
    fm.select(fm.size() - 1, grid);
    ASSERT_EQ(fm.selected().name, "cherry.cpp");

    EXPECT_TRUE(fm.type_ahead_append('a', false, grid));
    EXPECT_EQ(fm.selected().name, "apple.cpp");
}
