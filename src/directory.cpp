#include "directory.hpp"

#include <algorithm>

#include "glob.hpp"

namespace listless {

Directory::Directory(std::filesystem::path dir) : dir_(std::move(dir)) {}

void Directory::fill(std::string_view pattern, bool case_sensitive) {
    entries_.clear();

    std::error_code iter_ec;
    auto it = std::filesystem::directory_iterator(
        dir_, std::filesystem::directory_options::skip_permission_denied, iter_ec);

    if (iter_ec) {
        return;
    }

    for (const auto& dir_entry : it) {
        if (!glob_match(pattern, dir_entry.path().filename().string(), case_sensitive)) {
            continue;
        }

        std::error_code status_ec;
        std::filesystem::file_status status = dir_entry.status(status_ec);
        if (status_ec) {
            continue;  // e.g. a broken symlink: skip rather than fail the whole listing
        }

        bool is_dir = std::filesystem::is_directory(status);

        std::uintmax_t size = 0;
        if (!is_dir) {
            std::error_code size_ec;
            size = dir_entry.file_size(size_ec);
            if (size_ec) {
                continue;
            }
        }

        std::error_code time_ec;
        std::filesystem::file_time_type mtime = dir_entry.last_write_time(time_ec);
        if (time_ec) {
            continue;
        }

        bool read_only = (status.permissions() & std::filesystem::perms::owner_write) ==
                         std::filesystem::perms::none;

        entries_.push_back(DirEntry{
            dir_entry.path().filename().string(),
            dir_entry.path(),
            size,
            mtime,
            is_dir,
            read_only,
        });
    }
}

std::size_t Directory::size() const { return entries_.size(); }

const DirEntry& Directory::operator[](std::size_t index) const { return entries_.at(index); }

void Directory::sort(Comparator less) {
    if (!less) {
        less = [](const DirEntry& a, const DirEntry& b) { return a.name < b.name; };
    }

    std::sort(entries_.begin(), entries_.end(), less);
}

std::pair<std::filesystem::path, std::string> split_path_and_pattern(
    const std::filesystem::path& input) {
    std::error_code ec;
    if (std::filesystem::is_directory(input, ec) && !ec) {
        return {input, "*"};
    }

    std::filesystem::path dir = input.parent_path();
    std::string pattern = input.filename().string();

    if (dir.empty()) {
        dir = ".";
    }

    if (pattern.empty()) {
        pattern = "*";
    }

    return {dir, pattern};
}

}  // namespace listless
