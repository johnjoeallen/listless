#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace listless {

// Boyer-Moore-Horspool literal substring search, ported from the
// original OnScreen/2 strsrch()/SearchExpression (func/strsrch.cpp,
// include/jac.h). Construct once per pattern, then reuse across
// multiple searches (e.g. repeated "find next" in a file viewer).
class HorspoolSearcher {
  public:
    explicit HorspoolSearcher(std::string_view pattern, bool case_sensitive = true);

    // Index of the first occurrence of the pattern in `haystack` at or
    // after `start`, or std::nullopt if not found. An empty pattern
    // never matches, matching the original's behaviour.
    std::optional<std::size_t> find(std::string_view haystack, std::size_t start = 0) const;

  private:
    std::string pattern_;
    bool case_sensitive_;
    std::array<std::ptrdiff_t, 256> last_occurrence_{};
};

}  // namespace listless
