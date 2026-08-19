#pragma once

#include <string_view>

namespace listless {

// Matches `name` against the documented OnScreen/2 wildcard syntax
// (original/apps/onscreen/os.man, section 2.1.1):
//
//   *      any run of characters, including none
//   ?      exactly one character
//   [az]   one character from the given set
//   [a-z]  one character from the given range
//
// All other characters, including '.', match literally. An unterminated
// '[' (no matching ']') is treated as a literal '[', not an error.
//
// This is a direct wildcard matcher, not a regex translation like the
// original's FileExp2RegExp() — original/class/dir.cpp's translator also
// supported an undocumented '+' bracket-repetition-count suffix (e.g.
// "[abc]+3") that isn't mentioned anywhere in os.man; that surface is not
// reproduced here; see docs/02-search-primitives.md.
bool glob_match(std::string_view pattern, std::string_view name, bool case_sensitive = true);

// Matches `name` against one or more ';'-separated glob patterns (e.g.
// "*.cpp;*.hpp"), matching if `name` matches any of them.
bool glob_match_any(std::string_view patterns, std::string_view name, bool case_sensitive = true);

}  // namespace listless
