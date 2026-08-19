#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace listless {

// One entry in a directory listing.
struct DirEntry {
    std::string name;
    std::filesystem::path path;
    std::uintmax_t size = 0;
    std::filesystem::file_time_type last_write_time{};
    bool is_directory = false;
    bool is_read_only = false;
};

// A single directory level, listed on demand via fill().
class Directory {
  public:
    explicit Directory(std::filesystem::path dir);

    // Populates the listing with entries in this directory whose name
    // matches `pattern` (see glob_match()). Replaces any previous
    // listing. Symlinks are reported as their target's kind; entries
    // that can no longer be queried (e.g. a broken symlink, or removed
    // between iteration and stat) are silently skipped rather than
    // failing the whole listing.
    void fill(std::string_view pattern = "*", bool case_sensitive = true);

    std::size_t size() const;
    const DirEntry& operator[](std::size_t index) const;

    // Default order is case-sensitive by name (matching the original's
    // Unix-branch behaviour). Pass a custom comparator for sort-by-
    // extension/date/size.
    using Comparator = std::function<bool(const DirEntry&, const DirEntry&)>;
    void sort(Comparator less = {});

  private:
    std::filesystem::path dir_;
    std::vector<DirEntry> entries_;
};

// Splits a filespec argument into (directory, pattern). If `input` names
// an existing directory, the pattern is "*". Otherwise the last path
// component is treated as the pattern and the rest as the directory
// (defaulting to "." if `input` has no directory component).
std::pair<std::filesystem::path, std::string> split_path_and_pattern(
    const std::filesystem::path& input);

}  // namespace listless
