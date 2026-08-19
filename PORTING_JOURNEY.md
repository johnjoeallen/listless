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

---

## 2026-08-19 — Subsystem 02: search primitives (issue #2)

**Objective:** literal search and glob-pattern matching, replacing the
non-functional `RegExp` (stub `class/regexp.cpp`) — per
`docs/02-search-primitives.md`. Resolves the build blocker noted in
`docs/architecture.md` for filename-glob use cases; in-file regex search
mode is deferred to subsystem 07.

**Changes:**
- `src/search.hpp`/`src/search.cpp`: `HorspoolSearcher` — Boyer-Moore-
  Horspool literal substring search, ported in behaviour (not
  line-by-line) from `func/strsrch.cpp`'s `strsrch()`/`SearchExpression`.
  Construct once per pattern, `find(haystack, start)` repeatedly for
  "find next" style usage; optional case-insensitivity.
- `src/glob.hpp`/`src/glob.cpp`: `glob_match()`/`glob_match_any()` — a
  direct wildcard matcher (classic two-pointer backtracking algorithm,
  no regex engine) implementing the wildcard syntax documented in
  `os.man` section 2.1.1 (`*`, `?`, `[az]`, `[a-z]`, literal `.`).
  `glob_match_any()` handles `;`-separated pattern lists as used for
  file-manager filespecs.
- `tests/core/search_test.cpp`, `tests/core/glob_test.cpp`: coverage
  including the exact wildcard example from `os.man`
  (`xxx.[qa]*.xyz`), start-offset "find next" behaviour, case
  sensitivity in both directions, and edge cases (empty pattern,
  pattern longer than haystack, unterminated bracket).
- `docs/02-search-primitives.md` added and linked from `docs/README.md`.

**Decisions:**
- The original's undocumented `+`-suffix bracket-repetition-count syntax
  (e.g. `[abc]+3`) in `FileExp2RegExp` is not reproduced — it isn't part
  of the documented wildcard syntax in `os.man` and looks like an
  incidental detail of the regex-translation approach, not a real
  feature. Can be added later if a real use case appears.
- Both `glob_match()` and `HorspoolSearcher` default to case-sensitive
  (Linux's filesystem convention), with an explicit per-call
  `case_sensitive` parameter rather than a global toggle, so a later
  Windows/macOS platform layer can opt out per call site.
- In-file regex search mode is out of scope here; `std::regex` is the
  planned backend when subsystem 07 (file viewer core) needs it — no
  reason to build a regex wrapper ahead of that concrete call site.

**Tests:** `core_tests` — 12 new `GlobMatch*`/`GlobMatchAny*` cases, 10
new `HorspoolSearcher*` cases, plus all prior tests (29 total). Pass
under GCC, plain and under `-DLISTLESS_ENABLE_SANITIZERS=ON`
(Clang+ASan+UBSan); format-checked with `.clang-format`.

**Behaviour notes:** none diverging from `os.man`'s documented wildcard
syntax; see the "deliberately not reproduced" decision above for the one
piece of original behaviour intentionally dropped.

**Next steps:** subsystem 03, directory enumeration (#3) — `Directory`/
`Dirent`/`FileTreeWalker` with a new Linux backend via
`std::filesystem`, using this subsystem's glob matching for filename
filtering.

---

## 2026-08-19 — Subsystem 03: directory enumeration (issue #3)

**Objective:** `Directory`/`Dirent` listing with a new Linux backend via
`std::filesystem`, per `docs/03-directory-enumeration.md`. Listless's
core "ls" behaviour; depends on subsystems 01 and 02.

**Changes:**
- `src/directory.hpp`/`src/directory.cpp`: `DirEntry` (name, path, size,
  `std::filesystem::file_time_type`, `is_directory`/`is_read_only`) and
  `Directory` (`fill(pattern, case_sensitive)`, `size()`, `operator[]`,
  `sort()` with a default case-sensitive name comparator and an optional
  custom one). `fill()` filters via subsystem 02's `glob_match()`, and
  silently skips entries that can't be stat'd (e.g. a broken symlink)
  rather than failing the whole listing.
- `split_path_and_pattern()`: splits a filespec argument (e.g.
  `/home/user/*.cpp`) into `(directory, pattern)` — the Linux-relevant
  subset of the original's `splitPath()`, with no drive-letter parsing.
- `tests/core/directory_test.cpp`: coverage using a real scratch
  directory (created/torn down per test) — listing, glob filtering,
  directory-vs-file flagging, default and custom sort, a missing
  directory yielding an empty listing rather than throwing,
  out-of-range indexing throwing `std::out_of_range`, and
  `split_path_and_pattern()`'s three cases.

**Decisions:**
- `FileTreeWalker` is **not ported**. Grepped the entire `/original`
  tree — zero use sites anywhere, in either `apps/onscreen` or `class`.
  It's dead/unused infrastructure in the shipped source. If a real
  recursive-walk need appears later (recursive copy/delete in #11, or a
  recursive search feature), it'll be built as callbacks against that
  concrete call site rather than revived as a virtual-inheritance
  extension point with no other subclasses to justify the design.
- Drive-letter path logic (`splitPath`/`changeDir`/`queryCurrentDir`/
  `queryCurrentDisk`'s DOS/OS2/Win32 concepts) and `expandDir()`
  (wildcard-directory-component resolution — obscure, undocumented,
  no found call site) are not ported. `std::filesystem::path` operations
  replace `mergePath`/`nativePathName`/`unixPathName`/`dosPathName`/
  `conv2NativePathSep` directly — callers use `std::filesystem`, no
  Listless-specific wrapper.
- No `/platform/linux` implementation for this subsystem —
  `std::filesystem` is already portable across the three target
  platforms, so there's no OS-specific seam to isolate here. `/platform`
  is reserved for genuinely OS-specific APIs (console/keyboard I/O,
  subsystems 04/05).

**Bug fixed:** none — no OnScreen/2 directory-enumeration behaviour was
carried forward literally enough to inherit a bug from it; `Directory`
is a new implementation over `std::filesystem`, not a port of the
DOS/OS2/Win32 `FindFirst`-family backends (none of which target Linux
anyway).

**Tests:** `core_tests` — 11 new `DirectoryTest*`/`SplitPathAndPattern*`
cases, plus all prior tests (40 total). Pass under GCC, plain and under
`-DLISTLESS_ENABLE_SANITIZERS=ON` (Clang+ASan+UBSan); format-checked
with `.clang-format`.

**Behaviour notes:** default sort order is case-sensitive by name,
matching the original's `__UNIX__` branch (the only original branch
relevant to a Linux-first port).

**Next steps:** subsystem 04, console/terminal I/O (#4) — screen buffer
get/put/scroll, cursor movement, attributes, behind a `/platform/linux`
implementation. Needs a decision first: ncurses vs. a custom
termios+ANSI layer.

---

## 2026-08-19 — Subsystem 04: console/terminal I/O (issue #4)

**Objective:** screen buffer get/put/scroll, cursor movement, and
colour attributes, behind a `/platform/linux` implementation — per
`docs/04-console-terminal-io.md`. Gates subsystems 06/07.

**Decision: ncurses**, chosen over a custom termios+ANSI layer. Verified
directly in this sandboxed environment (no real TTY attached to the tool
shell) that `initscr()`/`endwin()` work correctly given `$TERM` is set —
screen size falls back to a sane default (24x80) when the real terminal
size can't be queried, colour support (`has_colors()`/`COLORS`/
`COLOR_PAIRS`) populates correctly after `start_color()` (confirmed
`COLORS=256`/`COLOR_PAIRS=256` under `TERM=xterm-256color`), and repeated
`initscr()`/`endwin()` cycles within one process work — which is what
makes the ncurses backend itself genuinely unit-testable rather than
manual-verification-only.

**Changes:**
- `src/color.hpp`/`src/color.cpp`: `Color` (the original's 16-value
  DOS/BIOS text-attribute palette, exact same numeric values 0-15,
  preserved for future `Style` config compatibility) and `to_ansi()`, a
  pure function mapping a `Color` to `{ANSI base 0-7, bright}` — a real
  lookup, since DOS and ANSI order their 8 base hues differently.
- `src/color_pair_table.hpp`/`src/color_pair_table.cpp`:
  `ColorPairTable` — lazily allocates ncurses-style small integer "pair"
  ids for `(fg, bg)` combinations, capacity-bounded with a documented
  fallback to the default pair, pure logic with an injectable allocation
  callback (no ncurses dependency, fully unit-tested).
- `src/terminal.hpp`: `Terminal` — a Pimpl class declared once, 0-indexed
  coordinates (no legacy 1-indexed DOS convention to preserve, since
  this is a new interface with no existing call sites),
  `width()`/`height()`/`move_cursor()`/`put_text()`/`clear_to_eol()`/
  `clear()`/`scroll_region()`/`refresh()`.
- `platform/linux/terminal.cpp`: the ncurses-backed `Terminal::Impl` —
  `curses_color_number()` converts `to_ansi()`'s output to an actual
  curses colour number (base, or base+8 for bright when `COLORS>=16`);
  colours degrade to `A_NORMAL` if the terminal reports no colour
  support.
- `platform/linux/CMakeLists.txt`: new `listless_platform_linux` target,
  `find_package(Curses)` with `CURSES_NEED_NCURSES` set (so it resolves
  to the real ncurses library, not a generic `libcurses.so` alias);
  publicly links `listless_core` (for `Color`/`ColorPairTable`) — the
  dependency direction is platform → core, not core → platform, since
  `listless_core` itself has no platform-specific code.
- `tests/platform_linux/terminal_test.cpp` (new test binary,
  `platform_linux_tests`): constructs a real `Terminal` per test (forcing
  `TERM=xterm-256color` in `SetUp()` regardless of the ambient
  environment) and verifies behaviour by reading back the actual ncurses
  screen buffer via `mvinch()` — no mocking, this exercises the real
  ncurses calls.
- `.github/workflows/ci.yml`: installs `libncurses-dev` in the
  build-and-test and sanitizers jobs.
- `tests/core/color_test.cpp`, `tests/core/color_pair_table_test.cpp`:
  full coverage of `to_ansi()`'s 16-entry mapping table and
  `ColorPairTable`'s allocation/reuse/exhaustion/callback behaviour.

**Decisions:**
- `MoveTextBuf`'s generic rectangular block-move is narrowed to
  `scroll_region(top, bottom, lines)` — full-width vertical scrolling
  only, matching both the realistic use case (the file viewer scrolling
  its text display) and what ncurses' `wscrl`/`wsetscrreg` natively
  support.
- `GetTextBuf` (arbitrary screen-region readback, e.g. for popup
  save/restore) is not ported — ncurses' own windowing model
  (subwindows/pads) is the idiomatic way to layer temporary UI without
  manual buffer save/restore; no concrete call site exists yet to design
  against.
- The named screen-write mutex (`hMtxListSync`) is not carried forward
  here as a synchronization primitive — whether a background
  clock-update thread is even wanted in the Linux port (its original
  purpose) is left open for whichever later subsystem needs a live
  status line.

**Bug fixed:** none new — `Terminal` is a new implementation, not a
line-by-line port of `osvideo.cpp`'s Borland-`conio.h`/Win32-console
backends.

**Tests:** 23 new (7 `ColorPairTable*`, 16 parameterized `ToAnsiTest`
cases, 7 `TerminalTest*` against the real ncurses backend) — 70 total
across `core_tests` and the new `platform_linux_tests` binary. Pass
under GCC, plain and under `-DLISTLESS_ENABLE_SANITIZERS=ON`
(Clang+ASan+UBSan); format-checked with `.clang-format`.

**Behaviour notes:** the original's `bg*16 + fg` packed-byte attribute
encoding is not carried forward as a wire format — `Terminal::put_text()`
takes `Color fg, Color bg` as separate arguments, matching how every
other modern colour API (including ncurses' own pair model) represents
this.

**Next steps:** subsystem 05, keyboard input (#5) — normalized keycode
model (reusing the original's synthetic `0xFFxx` extended-key space),
ncurses backend, sharing `Terminal`'s already-initialized ncurses state.
