#include "Glob.hpp"

#include <cctype>
#include <cstddef>

namespace listless {

namespace {

char fold(char c, bool case_sensitive) {
    return case_sensitive ? c : static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

bool bracket_matches(std::string_view cls, char ch, bool case_sensitive) {
    ch = fold(ch, case_sensitive);

    std::size_t i = 0;
    while (i < cls.size()) {
        if (i + 2 < cls.size() && cls[i + 1] == '-') {
            char lo = fold(cls[i], case_sensitive);
            char hi = fold(cls[i + 2], case_sensitive);
            if (ch >= lo && ch <= hi) {
                return true;
            }
            i += 3;
        } else {
            if (fold(cls[i], case_sensitive) == ch) {
                return true;
            }
            ++i;
        }
    }

    return false;
}

// End index (one past the token) of the pattern token starting at p:
// p+1 for a single character or '?', or one past a terminated '[...]'.
std::size_t token_end(std::string_view pattern, std::size_t p) {
    if (pattern[p] == '[') {
        std::size_t close = pattern.find(']', p + 1);
        if (close != std::string_view::npos) {
            return close + 1;
        }
    }

    return p + 1;
}

bool token_matches(std::string_view pattern, std::size_t p, std::size_t end, char ch,
                   bool case_sensitive) {
    if (pattern[p] == '?') {
        return true;
    }

    if (pattern[p] == '[' && end - p >= 2 && pattern[end - 1] == ']') {
        return bracket_matches(pattern.substr(p + 1, end - p - 2), ch, case_sensitive);
    }

    return fold(pattern[p], case_sensitive) == fold(ch, case_sensitive);
}

}  // namespace

bool glob_match(std::string_view pattern, std::string_view name, bool case_sensitive) {
    std::size_t p = 0;
    std::size_t n = 0;
    std::size_t star_p = std::string_view::npos;
    std::size_t star_n = 0;

    while (n < name.size()) {
        if (p < pattern.size() && pattern[p] == '*') {
            star_p = p;
            star_n = n;
            ++p;
            continue;
        }

        if (p < pattern.size()) {
            std::size_t end = token_end(pattern, p);
            if (token_matches(pattern, p, end, name[n], case_sensitive)) {
                p = end;
                ++n;
                continue;
            }
        }

        if (star_p != std::string_view::npos) {
            ++star_n;
            n = star_n;
            p = star_p + 1;
            continue;
        }

        return false;
    }

    while (p < pattern.size() && pattern[p] == '*') {
        ++p;
    }

    return p == pattern.size();
}

bool glob_match_any(std::string_view patterns, std::string_view name, bool case_sensitive) {
    std::size_t start = 0;

    while (start <= patterns.size()) {
        std::size_t semi = patterns.find(';', start);
        std::size_t end = semi == std::string_view::npos ? patterns.size() : semi;
        std::string_view part = patterns.substr(start, end - start);

        if (!part.empty() && glob_match(part, name, case_sensitive)) {
            return true;
        }

        if (semi == std::string_view::npos) {
            break;
        }

        start = semi + 1;
    }

    return false;
}

}  // namespace listless
