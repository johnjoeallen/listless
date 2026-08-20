#include "Search.hpp"

#include <cctype>

namespace listless {

namespace {

unsigned char fold(unsigned char c, bool case_sensitive) {
    return case_sensitive ? c : static_cast<unsigned char>(std::toupper(c));
}

}  // namespace

HorspoolSearcher::HorspoolSearcher(std::string_view pattern, bool case_sensitive)
    : pattern_(pattern), case_sensitive_(case_sensitive) {
    std::size_t m = pattern_.size();

    last_occurrence_.fill(static_cast<std::ptrdiff_t>(m));

    if (m == 0) {
        return;
    }

    for (std::size_t k = 0; k + 1 < m; ++k) {
        unsigned char c = fold(static_cast<unsigned char>(pattern_[k]), case_sensitive_);
        last_occurrence_[c] = static_cast<std::ptrdiff_t>(m - 1 - k);
    }
}

std::optional<std::size_t> HorspoolSearcher::find(std::string_view haystack,
                                                  std::size_t start) const {
    std::size_t m = pattern_.size();

    if (m == 0 || start > haystack.size() || m > haystack.size() - start) {
        return std::nullopt;
    }

    std::size_t i = start;

    while (i + m <= haystack.size()) {
        std::size_t j = m - 1;

        while (fold(static_cast<unsigned char>(pattern_[j]), case_sensitive_) ==
               fold(static_cast<unsigned char>(haystack[i + j]), case_sensitive_)) {
            if (j == 0) {
                return i;
            }
            --j;
        }

        unsigned char bad_char =
            fold(static_cast<unsigned char>(haystack[i + m - 1]), case_sensitive_);
        i += static_cast<std::size_t>(last_occurrence_[bad_char]);
    }

    return std::nullopt;
}

}  // namespace listless
