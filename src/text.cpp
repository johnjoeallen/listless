#include "text.hpp"

#include <cctype>

namespace listless {

int compare_ignore_case(std::string_view a, std::string_view b) {
    std::size_t n = a.size() < b.size() ? a.size() : b.size();

    for (std::size_t i = 0; i < n; ++i) {
        unsigned char ca =
            static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(a[i])));
        unsigned char cb =
            static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(b[i])));

        if (ca != cb) {
            return ca < cb ? -1 : 1;
        }
    }

    if (a.size() == b.size()) {
        return 0;
    }

    return a.size() < b.size() ? -1 : 1;
}

std::string_view trim(std::string_view s) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };

    while (!s.empty() && is_space(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }

    while (!s.empty() && is_space(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }

    return s;
}

}  // namespace listless
