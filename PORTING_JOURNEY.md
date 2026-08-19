# Porting Journey

Engineering history of the Listless port. Created once, appended to after
each subsystem iteration — never edited retroactively.

---

## 2026-08-19 — Repo scaffolding (issue #14)

**Objective:** stand up the build system and directory layout so
subsystem work has somewhere to land, per the README's stated principles
(CMake, target-based; GoogleTest, one binary per subsystem; warnings as
errors; clang-format enforced; ASan+UBSan on the Linux CI build).

**Changes:**
- Root `CMakeLists.txt`: C++20, `CMAKE_CXX_EXTENSIONS OFF`,
  `compile_commands.json` export, `LISTLESS_ENABLE_SANITIZERS` and
  `LISTLESS_BUILD_TESTS` options.
- `cmake/CompilerWarnings.cmake`: shared warnings-as-errors flag set
  (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion ... -Werror` on
  GCC/Clang, `/W4 /WX` on MSVC), applied via an interface target so every
  target opts in explicitly rather than via global flags.
- `/src`: a `listless_core` library target with a placeholder
  `version.hpp`/`.cpp`, just enough real code to prove the build and test
  wiring end-to-end.
- `/platform/{linux,windows,macos}`: empty directories (with a `README.md`
  explaining the intent) — implementations land alongside the subsystems
  that need them, starting with directory enumeration (#3) and console/
  keyboard I/O (#4, #5).
- `/tests`: GoogleTest pulled via `FetchContent` (pinned to v1.15.2, no
  system package dependency), `tests/core` as the first per-subsystem
  test binary (`core_tests`), wired into CTest via `gtest_discover_tests`.
- `.clang-format`: Google base style, 4-space indent, 100-column limit,
  C++20.
- `.github/workflows/ci.yml`: three jobs — build+test matrix over
  GCC and Clang, a dedicated Clang+ASan+UBSan job, and a clang-format
  check job.

**Decisions:**
- CI builds and tests with both GCC and Clang from the start (not just
  Clang), to catch compiler-specific issues early rather than only at
  the point a second compiler is added later.
- Sanitizers are opt-in via a CMake option (`LISTLESS_ENABLE_SANITIZERS`),
  not on by default in every build — CI turns them on in a dedicated job;
  local dev builds stay fast unless a contributor asks for them.
- No `clang-tidy` in this first CI pass — deferred until there's enough
  real code for static analysis to be worth configuring against, per
  discussion in issue #14.
- `/platform` starts as empty placeholder directories rather than stub
  interface headers, since the interfaces themselves don't exist yet —
  they'll be designed as part of subsystems 03-05, not invented ahead of
  need.

**Tests:** `core_tests` (`Version.IsNonEmpty`) — proves the build, link,
and CTest wiring work; no real subsystem logic exists yet to test.

**Behaviour notes:** none — no OnScreen/2 behaviour ported yet.

**Next steps:** subsystem 01, string/container primitives (#1) — the
first subsystem with real ported behaviour, per
`docs/01-string-container-primitives.md`.

---

## 2026-08-19 — Subsystem 01: string/container primitives (issue #1)

**Objective:** replace the custom pre-STL container/utility library
(`CString`, `Array`/`SortableArray`, `SList`/`DList`/`Set`, `RefPtr`,
`Date`/`Time`) with standard C++20 types, per
`docs/01-string-container-primitives.md`. Foundational — every later
subsystem depends on this.

**Decisions:**
- `std::string`/`std::string_view`, `std::vector`, `std::list`,
  `std::chrono` are used directly wherever later subsystems need them —
  there is no wrapper-class port of `CString`/`Array`/`SList`/`Date`/
  `Time`. This means subsystem 01 does not deliver one big library the
  way the original did; it delivers small, genuinely shared utilities
  that `std::` doesn't provide and that many later subsystems need, built
  when a real, immediate, cross-cutting need exists rather than
  speculatively ahead of it.
- `RefPtr`/`Storage<T>` — not ported. No use site was found anywhere in
  the surveyed application files, and `Storage::operator=` looks broken
  in the original (see `docs/architecture.md`). If a real need for shared
  ownership shows up in a later subsystem, `std::shared_ptr` is the
  direct replacement at that point.
- `include/memman.hpp`, `func/memcheck.cpp` — not ported (dead/
  non-compiling stub, and superseded by ASan/UBSan respectively; see
  `docs/architecture.md`).
- `Date`/`Time`, the `SList<Viewer>`-vs-`std::vector` decision for
  `App::cViewedFiles`, and `tracer`/`conditio`'s debug scaffolding are
  deliberately deferred to the subsystems that actually consume them
  (07 file viewer core, 04 console I/O) rather than built now — building
  them ahead of a concrete call site would mean guessing at the API shape
  twice.

**Changes:**
- `src/text.hpp`/`src/text.cpp`: `compare_ignore_case()` (case-insensitive
  three-way comparison, matching the original `stricmp()`'s semantics —
  used throughout the original for filename sorting and comparison) and
  `trim()` (leading/trailing whitespace stripped from a
  `std::string_view`). These are the two string primitives pervasive
  enough across the planned subsystem list (directory sorting in #3,
  path/glob handling in #2 and #3) to justify building ahead of any one
  of them specifically.
- `tests/core/text_test.cpp`: coverage for both functions, including
  equal-ignoring-case, strcmp-style ordering, prefix ordering, and
  trim's whitespace/empty-string edge cases.

**Bug fixed:** none new in this iteration — the original's `leftTrim()`/
`rightTrim()` (`class/cstring.cpp`) were confusingly named the wrong way
round relative to what they did, and `leftTrim()` only correctly stripped
trailing whitespace when it was contiguous at the very end of the string.
Rather than porting that behaviour (buggy) or its confusing names, `trim()`
is a single, correctly-named, correctly-behaved replacement — noted here
as a decision, not carried forward as a "fix" of ported code, since
nothing was ported.

**Tests:** `core_tests` — `CompareIgnoreCase.*` (3 cases),
`Trim.*` (3 cases), plus the existing `Version.IsNonEmpty`. All pass
under GCC; format-checked with `.clang-format`.

**Behaviour notes:** `compare_ignore_case()` matches original `stricmp()`
ASCII case-folding behaviour (via `std::tolower` on `unsigned char`);
non-ASCII/locale-sensitive comparison was not a concern in the original
and is out of scope here too.

**Next steps:** subsystem 02, search primitives (#2) — literal search
(port `func/strsrch.cpp`'s Boyer-Moore-Horspool algorithm) and glob
matching (replace `FileExp2RegExp`, resolving the non-functional
`RegExp` blocker).
