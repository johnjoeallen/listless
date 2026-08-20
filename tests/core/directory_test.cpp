#include "Directory.hpp"

#include <gtest/gtest.h>

#include <fstream>

namespace fs = std::filesystem;
using listless::Directory;
using listless::DirEntry;
using listless::split_path_and_pattern;

namespace {

class DirectoryTest : public ::testing::Test {
  protected:
    void SetUp() override {
        const auto* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        root_ = fs::temp_directory_path() /
                fs::path(std::string("listless_directory_test_") + test_info->name());
        fs::remove_all(root_);
        fs::create_directories(root_);

        touch(root_ / "banana.txt");
        touch(root_ / "apple.cpp");
        touch(root_ / "cherry.cpp");
        fs::create_directory(root_ / "subdir");
    }

    void TearDown() override { fs::remove_all(root_); }

    static void touch(const fs::path& p) { std::ofstream(p) << "x"; }

    fs::path root_;
};

}  // namespace

TEST_F(DirectoryTest, FillListsAllEntriesByDefault) {
    Directory dir(root_);
    dir.fill();

    EXPECT_EQ(dir.size(), 4u);
}

TEST_F(DirectoryTest, FillFiltersByGlobPattern) {
    Directory dir(root_);
    dir.fill("*.cpp");

    EXPECT_EQ(dir.size(), 2u);
    for (std::size_t i = 0; i < dir.size(); ++i) {
        EXPECT_TRUE(dir[i].name == "apple.cpp" || dir[i].name == "cherry.cpp");
    }
}

TEST_F(DirectoryTest, DirectoriesAreFlaggedCorrectly) {
    Directory dir(root_);
    dir.fill("subdir");

    ASSERT_EQ(dir.size(), 1u);
    EXPECT_TRUE(dir[0].is_directory);
}

TEST_F(DirectoryTest, FilesAreNotFlaggedAsDirectories) {
    Directory dir(root_);
    dir.fill("banana.txt");

    ASSERT_EQ(dir.size(), 1u);
    EXPECT_FALSE(dir[0].is_directory);
    EXPECT_EQ(dir[0].size, 1u);
}

TEST_F(DirectoryTest, SortDefaultsToCaseSensitiveNameOrder) {
    Directory dir(root_);
    dir.fill("*.cpp");
    dir.sort();

    ASSERT_EQ(dir.size(), 2u);
    EXPECT_EQ(dir[0].name, "apple.cpp");
    EXPECT_EQ(dir[1].name, "cherry.cpp");
}

TEST_F(DirectoryTest, SortAcceptsCustomComparator) {
    Directory dir(root_);
    dir.fill("*.cpp");
    dir.sort([](const DirEntry& a, const DirEntry& b) { return a.name > b.name; });

    ASSERT_EQ(dir.size(), 2u);
    EXPECT_EQ(dir[0].name, "cherry.cpp");
    EXPECT_EQ(dir[1].name, "apple.cpp");
}

TEST_F(DirectoryTest, FillOnMissingDirectoryYieldsEmptyListing) {
    Directory dir(root_ / "does_not_exist");
    dir.fill();

    EXPECT_EQ(dir.size(), 0u);
}

TEST_F(DirectoryTest, IndexingOutOfRangeThrows) {
    Directory dir(root_);
    dir.fill("does_not_match_anything_*");

    EXPECT_THROW(dir[0], std::out_of_range);
}

TEST_F(DirectoryTest, SplitPathAndPatternOnExistingDirectory) {
    auto [path, pattern] = split_path_and_pattern(root_);

    EXPECT_EQ(path, root_);
    EXPECT_EQ(pattern, "*");
}

TEST_F(DirectoryTest, SplitPathAndPatternOnDirectoryPlusGlob) {
    auto [path, pattern] = split_path_and_pattern(root_ / "*.cpp");

    EXPECT_EQ(path, root_);
    EXPECT_EQ(pattern, "*.cpp");
}

TEST(SplitPathAndPattern, BarePatternDefaultsDirectoryToDot) {
    auto [path, pattern] = split_path_and_pattern("*.cpp");

    EXPECT_EQ(path, ".");
    EXPECT_EQ(pattern, "*.cpp");
}
