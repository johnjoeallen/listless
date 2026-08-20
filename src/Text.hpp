#pragma once

#include <string_view>

namespace listless {

// Case-insensitive three-way comparison (ASCII), matching the original
// OnScreen/2 stricmp()'s semantics: negative if a < b, zero if equal,
// positive if a > b.
int compare_ignore_case(std::string_view a, std::string_view b);

// Leading and trailing ASCII whitespace stripped from s.
//
// The original OnScreen/2 leftTrim()/rightTrim() (class/cstring.cpp)
// were confusingly named the wrong way round relative to what they did,
// and leftTrim() only correctly stripped trailing whitespace when it was
// contiguous at the very end of the string. This is a single,
// correctly-named replacement rather than a port of that pair.
std::string_view trim(std::string_view s);

}  // namespace listless
