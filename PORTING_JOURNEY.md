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
  `Version.hpp`/`.cpp`, just enough real code to prove the build and test
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
- `src/Text.hpp`/`src/Text.cpp`: `compare_ignore_case()` (case-insensitive
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
- `src/Search.hpp`/`src/Search.cpp`: `HorspoolSearcher` — Boyer-Moore-
  Horspool literal substring search, ported in behaviour (not
  line-by-line) from `func/strsrch.cpp`'s `strsrch()`/`SearchExpression`.
  Construct once per pattern, `find(haystack, start)` repeatedly for
  "find next" style usage; optional case-insensitivity.
- `src/Glob.hpp`/`src/Glob.cpp`: `glob_match()`/`glob_match_any()` — a
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
- `src/Directory.hpp`/`src/Directory.cpp`: `DirEntry` (name, path, size,
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
- `src/Color.hpp`/`src/Color.cpp`: `Color` (the original's 16-value
  DOS/BIOS text-attribute palette, exact same numeric values 0-15,
  preserved for future `Style` config compatibility) and `to_ansi()`, a
  pure function mapping a `Color` to `{ANSI base 0-7, bright}` — a real
  lookup, since DOS and ANSI order their 8 base hues differently.
- `src/ColorPairTable.hpp`/`src/ColorPairTable.cpp`:
  `ColorPairTable` — lazily allocates ncurses-style small integer "pair"
  ids for `(fg, bg)` combinations, capacity-bounded with a documented
  fallback to the default pair, pure logic with an injectable allocation
  callback (no ncurses dependency, fully unit-tested).
- `src/Terminal.hpp`: `Terminal` — a Pimpl class declared once, 0-indexed
  coordinates (no legacy 1-indexed DOS convention to preserve, since
  this is a new interface with no existing call sites),
  `width()`/`height()`/`move_cursor()`/`put_text()`/`clear_to_eol()`/
  `clear()`/`scroll_region()`/`refresh()`.
- `platform/linux/Terminal.cpp`: the ncurses-backed `Terminal::Impl` —
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

---

## 2026-08-19 — Subsystem 05: keyboard input (issue #5)

**Objective:** normalized keycode model reusing the original's synthetic
`0xFFxx` extended-key space, ncurses-backed, sharing `Terminal`'s
already-initialized state — per `docs/05-keyboard-input.md`.

**What the original's keycode model actually is:** `getKey()`
(`osmisc.cpp:104-147`) calls `getch()`, and if it returns `0` (the PC
BIOS extended-key sentinel), calls it again and adds `0xFF00`. This means
`0xFFxx` values throughout `os.cpp`'s key-dispatch `switch` statements
(`case 0xFF3B: // F1`, `case 0xFF47: // Home`, etc.) are literal **PC/AT
keyboard Set-1 scan codes** — cross-checked and confirmed internally
consistent against both `osgetch.cpp`'s Win32 `kbdtab[]` (which maps NT
virtual keycodes back to these same BIOS scan-code values) and
`ostxt.hpp`'s `VKALT_*` table.

**Changes:**
- `src/Key.hpp`/`src/Key.cpp`: `KeyCode` (a plain `int` alias) and a
  `Key` namespace of named constants using the exact numeric values the
  original's `switch` statements already use (`Up`, `Down`, `Left`,
  `Right`, `Home`, `End`, `PageUp`, `PageDown`, `Insert`, `Delete`,
  `ShiftTab`, `F1`-`F12`), plus `Key::Resize` (new — no BIOS scan code
  existed for "the window changed size") and `Key::Unknown` (an honest
  sentinel for anything unmapped, not a silent drop). `alt_key(char)`
  returns `Alt+<letter/digit>`, matching `ostxt.hpp`'s `VKALT_*` table.
- `src/Keyboard.hpp`: `Keyboard` — a Pimpl class taking a `Terminal&` in
  its constructor purely to enforce, at the type level, that ncurses is
  already initialized before keyboard input is read. `read_key()`
  (blocking) and `key_available()` (`kbhit()`-equivalent).
- `platform/linux/Keyboard.cpp`: `translate_curses_key()` maps ncurses'
  `KEY_*` constants to the `Key` constants above; `read_key()` handles
  Alt-key detection by peeking (via `nodelay`) for a byte immediately
  following a bare `ESC` — verified directly in this sandboxed
  environment using ncurses' own `ungetch()` to inject `ESC`+`'a'` and
  confirm it resolves to `Alt+A`, and that a lone `ESC` with nothing
  queued resolves to plain `Escape`.
- `tests/platform_linux/keyboard_test.cpp`: constructs a real
  `Terminal`+`Keyboard` per test and injects input via ncurses'
  `ungetch()` — plain characters, control characters, `KEY_*` specials,
  function keys, an unrecognized special key, a lone `Escape`, an
  `Alt+<letter>` sequence, and `key_available()`'s non-consuming peek —
  exercising the real ncurses input path end-to-end, not a mock.
- `tests/core/key_test.cpp`: `alt_key()` and the `Key` constants,
  cross-checked directly against the original's `VKALT_*` table and
  `os.cpp`'s literal `case` values.

**Decisions:**
- Modifier-combined extended keys beyond the base set above
  (`Shift+F1`-`F10`, `Ctrl+PgUp`/`PgDn`, etc. — present as scattered
  literals in the original's `os.cpp`) are **not** predefined here.
  Unlike arrows/function/editing keys, ncurses' reporting of *modified*
  navigation/function keys is terminal-(terminfo-)dependent, not a
  universal scan code the way DOS BIOS gave every program for free —
  better verified against a real terminal when a concrete keybinding
  needs it (subsystems 06/07) than guessed at now. `Key::Unknown` is the
  honest answer for these today.
- The original's `ungetch(int c)` (push one character back for the next
  `getch()`) is not exposed at the `Keyboard` level — no concrete call
  site needs it; the original's own usage is internal to its `getch()`
  implementation, not something the app calls directly.

**Bug fixed:** none new — `Keyboard` is a new implementation, not a
line-by-line port of `osgetch.cpp`'s Win32/DPMI32 backends (which don't
target Linux anyway).

**Tests:** 22 new (5 `AltKey*`, 1 `Key.ExtendedConstantsMatchOriginalLiterals`,
8 `KeyboardTest*` against the real ncurses input path) — 83 total across
`core_tests` and `platform_linux_tests`. Pass under GCC, plain and under
`-DLISTLESS_ENABLE_SANITIZERS=ON` (Clang+ASan+UBSan); format-checked
with `.clang-format`.

**Behaviour notes:** Alt-key detection relies on the terminal sending
`ESC` as a Meta/Alt prefix (standard for terminal emulators) rather than
the original's Win32 console API `dwControlKeyState` flag — functionally
equivalent from the app's perspective (`Alt+<key>` still resolves to the
same `0xFFxx` value), but the detection mechanism itself is necessarily
different since there is no Linux terminal equivalent of a per-keystroke
modifier-state field.

**Next steps:** Phase 2 (I/O layer) is complete. Next is subsystem 07,
file viewer core (#7) — text-mode display, scrolling, search, bookmarks —
before subsystem 06 (file-list UI), since the viewer is simpler and more
self-contained, and the file manager's "View" action ends up calling into
it anyway.

---

## 2026-08-19 — Subsystem 07: file viewer core (issue #7)

**Objective:** text-mode display, scrolling, search, bookmarks — the
"less"-equivalent half of Listless, per `docs/07-file-viewer-core.md`.
A full research pass over the whole of `osview.cpp` (3107 lines, the
largest file in `/original`) preceded this iteration, since the file
tangles viewer-core concerns with hex mode, syntax highlighting, and the
`Style` system throughout; the doc records exactly where those seams
are so later subsystems land against known integration points rather
than rediscovering them.

**Changes:**
- `src/Viewer.hpp`/`src/Viewer.cpp`: `Viewer` — pure logic and state, no
  ncurses dependency, fully unit-tested in `tests/core` (35 new cases).
  Line model as `offset+length` spans (`LineSpan`) into a loaded
  `std::string`, replacing the original's in-place-NUL-termination trick
  (which existed only to let text/hex mode share one `char*` buffer —
  spans don't need it). Word wrap: simplified greedy word-wrap kept
  separate from the original (unwrapped) line spans, so disabling it
  restores exact original line boundaries. Viewport (`top_line()`/
  `column()`) with `scroll_line_up/down`, `scroll_page_up/down`,
  `scroll_to_top/bottom`, `scroll_left/right` (10-column steps,
  1024-column cap, matching the original) — all return `bool` (did the
  position change) so a caller can show "already at top/bottom"-style
  messages. Search (`search_forward`/`search_backward`/`repeat_search`)
  built on subsystem 02's `HorspoolSearcher`; goto-line and per-`Viewer`
  bookmarks (`Alt+0`-`Alt+9` toggle, `Alt+G`+digit jump) ported directly
  with no `Style` dependency.
- `src/ViewerRender.hpp`/`src/ViewerRender.cpp`: a minimal renderer
  (tested against the real `Terminal` in `tests/platform_linux`, 6 new
  cases) doing exactly the style-independent subset identified in the
  survey: iterate visible lines, fixed-width tab expansion, the
  selection-range highlight overlay, one status-line format. Explicitly
  designed to be extended, not rewritten, once subsystems 08/09 add real
  styling.
- `src/Key.cpp`: corrected `alt_key()` to cover `'0'`-`'9'` (was
  `'1'`-`'9'`) — the survey found `Alt+0` (scan code `0x81`) is a real
  bookmark-slot-0 binding in `osview.cpp`, contradicting subsystem 05's
  docs, which claimed no `VKALT_0` exists. `docs/05-keyboard-input.md`
  corrected to match. `tests/core/key_test.cpp` updated accordingly.

**Decisions:**
- Search "backward" visits lines in decreasing order, taking each
  line's leftmost match (via `HorspoolSearcher`, which is forward-only)
  rather than a true "rightmost match" reverse search — a faithful-
  enough reading of the original, which also reused its forward-only
  `strsrch` per line, differing only in which lines it visited. "Repeat,
  continuing on the same line" for the backward case needed "the
  rightmost match strictly before the current one," which
  `HorspoolSearcher` doesn't expose directly — built as a small
  accumulate-until-limit loop (`find_last_before()`) rather than adding
  a new primitive to subsystem 02 for one call site.
- `ensure_selection_visible()` is a separate method from the `search_*()`
  calls, rather than folding viewport repositioning into search itself
  (as the original's `AdjustRowAndColumn` does inline) — keeps "was a
  match found" and "how do we scroll to show it" independently
  testable, and lets a caller choose not to auto-scroll if it wants
  different behaviour later.
- One status-line format, not the original's three `Style`-selected
  alternates — the cycling mechanism lives on `Style`, which doesn't
  exist yet. A fixed-width (8-column) tab width for the same reason.
  Both are one-line changes to wire in real configured values once
  subsystem 08 lands, not a redesign.
- No incremental scroll-region rendering optimization (the original
  shifts existing screen content for single-line scrolls) — always a
  full redraw of the visible page. Performance optimization, not a
  correctness concern; `Terminal::scroll_region` remains available to
  add this later without changing `Viewer`'s interface.
- No live file-change detection/reload, no external-filter/`d`/`D`
  reload feature (both `Style`/App-level concerns), no stdin/pipe
  loading plumbing (Windows-console-specific in the original; `Viewer`
  supports being constructed directly from an in-memory buffer, so an
  `App`-level stdin reader can hand it one without a real path).

**Bug fixed:** none new — `Viewer` is a new implementation over a
different line-storage model, not a line-by-line port of `osview.cpp`'s
NUL-termination-based buffer.

**Tests:** 41 new (35 `Viewer.*` covering line model, word wrap,
scrolling, search, goto-line, bookmarks; 6 `ViewerRenderTest.*` against
the real ncurses backend; `AltKey` test set updated for the `Alt+0`
fix) — 125 total across `core_tests` and `platform_linux_tests`. Pass
under GCC, plain and under `-DLISTLESS_ENABLE_SANITIZERS=ON`
(Clang+ASan+UBSan); format-checked with `.clang-format`.

**Behaviour notes:** default sort/search case-sensitivity, horizontal
scroll step/cap, and the search-continuation semantics all match the
original; see "Decisions" above for the two places search behaviour is
a faithful reinterpretation rather than a literal port, given
`HorspoolSearcher`'s forward-only API.

**Next steps:** subsystem 06, file-list UI (#6, `FileManager`) — the
"ls" half. Also still open: the subsystem-06-behaviour doc (a separate
earlier-filed issue) should land before or alongside that port.

---

## 2026-08-19 — File-list UI (issues #6, #8)

**Objective:** port `FileManager` (`fileman.cpp`, 2484 lines), the "ls"
half of Listless's primary deliverable, landing the documentation issue
(#6) and the port (#8) together, per this project's usual pattern (see
subsystems 04-07, where the doc and its port share one commit).

**Changes:**
- `docs/06-file-list-ui.md`: documents the original's two-pass directory
  listing (directories unfiltered, files filtered by the glob file
  spec), all four sort comparators (directories always first, always
  name-ascending among themselves), the column-major grid navigation
  model including a real off-by-one in the constructor's column-count
  search (the pre-decrement-in-the-same-expression idiom means the
  requested column count is never actually tried), type-ahead
  multi-select's forward-wraps/backward-doesn't asymmetry, and a
  reasoned reconstruction of what turned out to be a corrupted encoding
  artifact in this checkout of `fileman.cpp` (`d.name()[1]` checks
  against dropped non-ASCII bytes, evidence of a leading directory-marker
  byte in the original `Dirent::name()` that has no counterpart in the
  port's explicit `DirEntry::is_directory` bool).
- `src/FileManager.hpp`/`.cpp`: `FileManager` — pure logic and state,
  no terminal/rendering dependency (same split as `Viewer`/
  `viewer_render` from subsystem 07). Listing/sorting (`refresh()`,
  `set_sort()`), the grid navigation model (`move_up/down/left/right/
  home/end/page_up/page_down`, `compute_grid()`, `select()`), type-ahead
  multi-select (`type_ahead_append()`/`type_ahead_backspace()`/
  `clear_type_ahead()`), navigation (`change_directory()` for a typed
  path, `enter_selected()` for moving into the currently-selected
  directory — kept as two methods because the original keeps them as two
  distinct code paths with different behaviour, only one of which
  re-selects the just-left child directory on `..`), and
  `directory_history()`.
- 28 new `FileManagerTest`/`FileManagerGridTest` cases in
  `tests/core/file_manager_test.cpp` covering listing, all four sort
  keys/directions, directory-unfiltered-by-spec, grid navigation (every
  move direction, the Home/End two-step special cases, page up/down),
  type-ahead (append/backspace/wraparound/mode-switch-off-a-file), and
  navigation (`change_directory`/`enter_selected`/`set_file_spec`) —
  153 total across `core_tests` and `platform_linux_tests`. Pass under
  GCC (plain and under `-DLISTLESS_ENABLE_SANITIZERS=ON`, GCC's
  ASan+UBSan); format-checked with `.clang-format`. Clang wasn't
  available in this sandbox to reproduce CI's Clang leg locally; left
  for CI to confirm.

**Decisions:**
- `change_directory()`/`enter_selected()` never call a real
  `std::filesystem::current_path()`/`chdir()` — `FileManager` tracks
  `current_directory()` as its own state, the same choice
  `Directory`/`DirEntry` made in subsystem 03. A real process-wide
  `chdir()` would be global, hard-to-reverse state that races across
  parallel unit tests, for no benefit here.
- File operations (Copy/Delete/Rename/Move/MakeDirectory) are not
  ported at all — deferred to subsystem 11 per issue #8, given their
  destructive nature and the higher regression-test scrutiny that
  implies. `FileManager` exposes only the read-only surface.
- `DisplayDisks()`'s drive-letter bar (and `Ctrl+A`-`Ctrl+Z` drive
  jumps) is not reinterpreted for Linux yet, despite issue #6 asking for
  that reinterpretation to at least be *documented*. It's documented (as
  "deliberately not ported, no concrete rendering call site exists" in
  `docs/06-file-list-ui.md`) but not designed or implemented — the same
  call made for `Style`-dependent formatting in subsystem 07, since no
  screen loop/`App` subsystem exists yet to actually draw it.
- `LineEdit` (the modal prompt widget every `FileManager` command uses
  for input) is out of scope — it's a general widget, not
  `FileManager`-specific, and building it against just these call sites
  would bake in assumptions better validated against a second concrete
  need.
- Case-insensitive type-ahead matching in both directions, not just
  forward — the original's backward branch is `#if defined(__UNIX__)`
  gated to case-sensitive `strstr`, but (per subsystem 03's finding) no
  `__UNIX__` backend exists anywhere in `/original`, so that branch
  never built or ran; not worth preserving as a real behaviour.

**Bug fixed:** none new — `FileManager` is a new implementation over
`Directory`'s already-ported listing primitive, not a line-by-line port
of `fileman.cpp`'s DOS/OS2/Win32-specific state machine.

**Tests:** 28 new (`FileManagerTest`/`FileManagerGridTest`) — 153 total
across `core_tests` and `platform_linux_tests`. Pass under GCC, plain and
under `-DLISTLESS_ENABLE_SANITIZERS=ON`; format-checked with
`.clang-format`.

**Behaviour notes:** the column-count search-from-`requested-1` off-by-one,
the Home/End two-step special cases, and the forward-wraps/backward-
doesn't type-ahead asymmetry are all preserved exactly rather than
"fixed" — see `docs/06-file-list-ui.md`'s "What the original provides"
section for the full reasoning on each.

**Next steps:** subsystem 08, style/config system (`Style`, `Item<T>`
inheritance, `os.set` load/save) — needed before syntax highlighting,
and before `FileManager`'s per-instance color cycling (`F2`-`F6`) or a
real `Style`-driven status line can be wired in. A future `App`/main-loop
subsystem (not yet named in `docs/architecture.md`'s numbered breakdown)
will need to exist before `FileManager`'s `Activate()`-equivalent
keyboard loop, `file_manager_render`, the Linux disk-bar
reinterpretation, or `viewedFiles`-backed View/Edit dispatch can be
built.

## 2026-08-19 — Subsystem 08: style/config system (issue #9)

**Objective:** port `Style` and the config load/save path
(`os.hpp`/`osstyle.cpp`, 2054 lines combined) — the prototype-style
inheritance mechanism and config file format that syntax highlighting
(subsystem 09) and a real per-file-type status line depend on.

**Changes:**
- `docs/08-style-config.md`: documents `Item<T>`'s lazy-fallback
  resolution (own value, else first non-null result walking base items
  in *most-recently-added-first* order — `AddBaseItem` prepends), the
  ~34-field `Style` record, `AddBaseStyle`'s field-by-field linking
  (everything except `iExtensions`, which never inherits; `iReserved`
  copies rather than links, marked `iInherited`), the hand-rolled
  brace-delimited config grammar (`getSymbol`'s tokenizer, the `=>`
  rest-of-line value capture, multi-line key continuation), and the
  separate `Settings` block (FileManager UI colours/app toggles — out of
  scope, `App`-level).
- `src/Style.hpp`/`src/Style.cpp`: `Item<T>` (using `std::optional<T>`
  and `std::vector<Item<T>*>` in place of the original's manually
  `new`/`delete`d `T*` and `Set<Item<T>>`); `Style` with the same field
  set, typed with subsystem 04's `Color` enum instead of a raw `BYTE`;
  `StyleSet` (new — no direct original equivalent; owns every `Style`
  behind `std::unique_ptr` since `Item<T>::add_base_item()` stores raw
  pointers into other styles' members, so a `Style` must never move once
  linked); `load_config()`/`save_config()`, a from-scratch parser/writer
  producing the same brace-delimited shape (same section-key spellings,
  `=>`, continuation lines) but not byte-identical output (no `.bak`
  rotation, no tab-aligned columns); `cycle_color()`, the palette-cycling
  primitive `F2`-`F7` would call once a keyboard loop exists to call it.
- `tests/core/style_test.cpp` (new, 29 cases): `Item<T>` resolution
  (unset, own-overrides-base, multi-level fallback, most-recent-base-
  wins-ties), `Style::add_base_style` (linking, override, extensions-
  don't-inherit, reserved-word inheritance/dedup), `StyleSet` (always
  has `Default`, case-insensitive lookup, extension lookup),
  `default_config_path()`'s `XDG_CONFIG_HOME` handling, and
  `load_config`/`save_config` round-tripping (scalars, list fields
  across continuation lines, base-style resolution, the `Default`-style-
  reuses-the-built-in-instance special case, comment/blank-line
  handling, and malformed-value-is-skipped-not-fatal).

**Decisions:**
- **Malformed config input is skipped, not fatal** — the original calls
  `exit(3)` on the first parse error (invalid color name, missing `(`,
  etc.), taking down the whole program over one bad line in a hand-
  edited file. `load_config()` instead ignores the one bad key/value
  pair and keeps parsing. A one-line intentional deviation, not an
  oversight — see `docs/08-style-config.md`'s "narrowed" section.
- **`extensions`/`open_comment`/`close_comment`/`eol_comment`/
  `numeric_prefix` use `std::vector<std::string>`, not a `Set`** — every
  real call site (iterate in insertion order, check membership by
  linear scan for a handful of entries) never needed `Set`'s dedup
  semantics; a `Set<Item<T>>` for `Item<T>`'s own base-item list becomes
  a plain `std::vector<Item<T>*>` for the same reason (a style is only
  ever linked as a base once in practice).
- **The `Settings` block (FileManager UI colours, sound, search-mode
  default) is out of scope** — it's `App`-level state on a global
  `setupInfo` struct in the original, unrelated to any `Style`, and
  there's no `App`/main-loop subsystem yet to own it (issue #24).
  `load_config()` simply never matches the `Settings` keyword, so a
  hand-edited file with one is silently skipped rather than parsed or
  rejected.
- **`F2`-`F7` colour cycling and `Ctrl+S` persistence are not wired to
  any keybinding** — both are keyboard-loop features belonging to the
  not-yet-built `App`/main-loop subsystem (issue #24). `cycle_color()`
  is the primitive; wiring it to a real key is a small addition once
  that loop exists, not a redesign.

**Bug fixed:** none new — this is a from-scratch parser/writer over a
new field-owning model, not a line-by-line port of `osstyle.cpp`'s
`fscanf`-adjacent tokenizer.

**Tests:** 29 new (`Item`/`Style`/`CycleColor`/`StyleSet`/
`DefaultConfigPath`/`LoadConfig`/`SaveConfig`) — 182 total across
`core_tests` and `platform_linux_tests`. Pass under GCC, plain and under
`-DLISTLESS_ENABLE_SANITIZERS=ON`; format-checked with `.clang-format`.
(Clang wasn't available in the environment this was ported in, so the
Clang CI leg is unverified locally — CI will confirm.)

**Behaviour notes:** `Item<T>`'s "most recently added base wins ties"
resolution order, `AddBaseStyle`'s extensions-never-inherit /
reserved-words-copy-not-link split, and the config format's exact
section-key spellings and `=>`-continuation syntax are all preserved
deliberately — see `docs/08-style-config.md` for the full reasoning.

**Next steps:** subsystem 09, syntax highlighting (layers on `Style` +
the viewer core). The `App`/main-loop subsystem (issue #24) remains a
prerequisite for wiring any of `Style`'s live-editing features
(`F2`-`F7` cycling, `Ctrl+S`) or `FileManager`/`Viewer`'s keyboard loops
into an actual running program.

## 2026-08-19 — Subsystem 09: syntax highlighting (issue #10)

**Objective:** layer per-line syntax colouring onto the viewer core
(subsystem 07) using the style system (subsystem 08) — reserved-word/
comment/string/preprocessor/number/symbol colouring, cross-line comment
and preprocessor state tracking, and the separate `BOLD_CODE`/
`UNDERLINE_CODE` "text with layout" toggle-byte feature.

**Changes:**
- `docs/09-syntax-highlighting.md`: documents the classification helpers
  (`IsString`/`IsEolComment`/`IsBeginComment`/`IsEndComment`/
  `IsNumericPrefix`/`IsSymbol`/`keywordCmp`/`IsReservedWord`),
  `findStyleForFile`, the `LineStatus` cross-line state model, the
  `BOLD_CODE`/`UNDERLINE_CODE` toggle-byte feature and its "disabled
  whenever syntax highlighting is on" gating, and `scanData`'s (state-
  tracking) and `displayData`'s (colouring) two separate passes over the
  same rule set — including the `iReserved.Size() > 0` quirk that
  silently falls back to layout-toggle mode when syntax highlighting is
  enabled but a style has zero reserved words.
- `src/SyntaxHighlight.hpp`/`src/SyntaxHighlight.cpp` (new):
  `highlight_line(text, style, state)`, unifying `scanData`'s state
  machine and `displayData`'s colouring chain into one pass over the
  line, since both need the same rule evaluations and the original only
  splits them because `scanData` runs once up front while `displayData`
  re-derives entry state per render. Returns `std::vector<ColorSpan>`
  (offset/length/colour/bold/underlined runs, adjacent same-attribute
  spans merged) and mutates a `HighlightState` (`in_comment`,
  `in_preprocessor`, `bold`, `underlined` — the original's single
  `LineStatus` enum split into independent fields since a line can be
  bold *and* underlined at once). Internally: `highlight_syntax()` (the
  comment/preprocessor/string/symbol/number/reserved-word/identifier
  precedence chain, active when `syntax_highlight_enabled &&
  !reserved.empty()`) and `highlight_layout()` (the `BOLD_CODE`/
  `UNDERLINE_CODE` toggle path, gated on `text_with_layout`, used
  otherwise). Per-extension style selection needed no new code — reuses
  `StyleSet::style_for_extension()` from subsystem 08.
- `tests/core/syntax_highlight_test.cpp` (new, 18 cases): plain
  identifiers, reserved-word matching with word-boundary and case-
  sensitivity handling, hex/decimal numbers, escaped strings, end-of-
  line comments, multi-line `/* */` comments persisting `in_comment`
  across 3 lines, preprocessor blocks requiring column-0 start and
  persisting `in_preprocessor` only when the line ends in the
  continuation character, syntax-highlighting-disabled and empty-
  reserved-list fallback to a single default-coloured span, `BOLD_CODE`/
  `UNDERLINE_CODE` toggling and cross-line persistence gated on
  `text_with_layout`, and per-extension style selection feeding into
  `highlight_line`.
- `docs/README.md`: added the subsystem 09 entry to the index.

**Decisions:**
- **Comment delimiters always matched case-sensitively** — the original
  is internally inconsistent here (`IsEolComment` respects
  `iCaseSensitive`; `IsBeginComment`/`IsEndComment` use plain `strncmp`
  regardless). This port picks the always-case-sensitive behaviour for
  all three, applying `case_sensitive` only where the original
  consistently does (reserved words).
- **String escape handling uses a standard "backslash escapes the next
  character" reading**, not the original's exact post-increment
  double-consume sequence (`osview.cpp:936-940`), which reads as an
  accidental simplification in the original rather than deliberate
  behaviour — a faithful-enough reinterpretation in the same spirit as
  subsystem 07's search-backward decision, not a byte-for-byte port of
  what looks like an original bug.
- **No case-conversion of displayed reserved-word text** — `ColorSpan`
  carries an offset/length into the caller's text, not replacement
  text, so `iCaseConvert`'s "redisplay the keyword in its stored casing"
  isn't reproduced; reserved words keep the source's casing, only their
  colour changes. Revisiting needs `ColorSpan` (or a caller) to carry
  replacement text, deferred until a real renderer needs it.
- **Preprocessor/comment state tracking gated identically to colouring**
  (`syntax_highlight_enabled && !reserved.empty()`), where the original's
  `scanData` state machine only checks `iSyntaxHighlightEnabled` — judged
  not worth reproducing since a style with syntax highlighting on and
  zero reserved words is a degenerate case the original barely supports
  either way (falls back to layout-toggle colouring regardless).
- **No backspace-overstrike manual bolding** (`char\bchar`,
  `osview.cpp:1024-1041`) — a separate `WithLayout`-gated feature from
  `BOLD_CODE`/`UNDERLINE_CODE`, not implemented; no call site in this
  port needs it.

**Bug fixed:** none new — this is a from-scratch tokenizer built against
`Style`'s field API, not a line-by-line port of `scanData`/`displayData`.

**Tests:** 18 new (`HighlightLine.*`) — 197 total across `core_tests` and
`platform_linux_tests`. Pass under GCC plain and under
`-DLISTLESS_ENABLE_SANITIZERS=ON`; `clang-format --dry-run --Werror`
clean.

**Behaviour notes:** cross-line comment/preprocessor state tracking, the
column-0-only preprocessor start rule, word-boundary reserved-word
matching, and the syntax-highlighting/layout-toggle mutual exclusivity
all preserve the original's behaviour deliberately — see
`docs/09-syntax-highlighting.md`'s "narrowed or deferred" section for
where this port diverges.

**Next steps:** subsystem 10, hex-mode viewer (smaller, mostly self-
contained). Rendering `highlight_line`'s output to a real terminal, and
wiring `F2`-`F7`/`Ctrl+S`/per-file-type style selection into an
interactive session, both remain blocked on the `App`/main-loop
subsystem (issue #24).

## 2026-08-19 — Subsystem 10: hex-mode viewer (issue #10)

**Objective:** a second display mode on the viewer core (subsystem 07)
— the same loaded file shown as a 16-bytes-per-row hex dump — per
`docs/10-hex-mode-viewer.md`.

**Changes:**
- `src/Viewer.hpp`/`src/Viewer.cpp`: `DisplayMode` (`Text`/`Hex`) and
  `kBytesPerHexLine = 16` on `Viewer`. `switch_to_hex_mode`/
  `switch_to_text_mode` port `calcNearestHexTopLine`/the text-mode
  scan-back directly, translating between the text viewport's current
  line and the hex viewport's byte offset. `hex_line_count`,
  `hex_line_bytes`, and `hex_scroll_to_top/bottom`,
  `hex_scroll_page_up/down`, `hex_scroll_line_up/down` mirror
  `hexLineCount`/`displayHexLine`/`handleKeyInHexMode`'s scroll branches,
  using the same "return true iff the position changed" contract as
  subsystem 07's text-mode `scroll_*()`. `hex_goto_offset` ports the
  `g`/`G` handler's clamp-and-round-to-16-byte-boundary math, minus the
  `LineEdit` prompt/parsing (an `App`/main-loop concern, issue #24).
- `src/ViewerRender.cpp`/`.hpp`: `format_hex_line`/`draw_hex_line` — a
  standard hex-dump row (8-hex-digit offset, 16 bytes grouped in 4s,
  ASCII gutter). `render_viewer` dispatches on `Viewer::display_mode()`
  to this or the existing text-mode renderer.

**Deviations:** the original's stray non-ASCII separator byte between
offset and hex bytes (a mojibake artifact, not a deliberate glyph) is
dropped in favour of a plain gap; an ASCII gutter is added even though
the original's hex view has none (a genuine addition, not a port, since
every other hex viewer has one and there's no faithfulness reason to
omit it); the `DISPLAY_END_OF_FILE` marker row is skipped (undefined in
the original's shipped build, so no observable behaviour exists to
port); selection highlighting in hex mode is not implemented (hex mode
has no byte-range selection concept yet). Full rationale in
`docs/10-hex-mode-viewer.md`'s "narrowed or deferred" section.

**Bug fixed:** none — a direct port of working original logic.

**Tests:** 22 new (`Viewer.Hex*` in `tests/core`, `ViewerRenderTest.Hex*`
in `tests/platform_linux`) — 223 total. Pass under GCC plain.

**Next steps:** all remaining ported subsystems (viewer text/hex modes,
syntax highlighting, style config, file list UI) are now blocked on the
`App`/main-loop subsystem (issue #24) to wire keyboard input, mode
switching, and per-file style selection into an interactive session.

## 2026-08-19 — App entry point / main loop (issue #24)

**Objective:** the first runnable `lss` binary, wiring
`FileManager` (06) and `Viewer`+hex mode (07/09/10) into a real
interactive session, per `docs/app-main-loop.md`. Unlike every
subsystem before it, this one isn't in `docs/architecture.md`'s
numbered 1-12 breakdown -- it's the cross-cutting App/main-loop layer
every one of those subsystem docs explicitly deferred to.

**Changes:**
- `src/LineEdit.hpp`/`.cpp`: `line_edit_key` -- the original's
  `LineEdit` modal-prompt widget's key-handling core as a pure function
  (append/backspace/submit/cancel), no history.
- `src/AppActions.hpp`/`.cpp`: `handle_browsing_key`/
  `handle_viewing_key` -- pure key-dispatch tables ported from the
  reachable-key subset of `FileManager::Activate`'s and
  `Viewer::handleKey`'s switch statements, unit-tested against real
  `FileManager`/`Viewer` instances (18 new cases).
- `src/FileManagerRender.hpp`/`.cpp`: `render_file_manager` --
  `FileManager`'s status line plus column-major grid, the renderer
  subsystem 06 never built. Modeled on `ViewerRender.cpp`'s shape (5
  new `tests/platform_linux` cases).
- `src/App.hpp`/`.cpp`, `src/main.cpp`: `App` -- owns `Terminal`/
  `Keyboard`/`FileManager`/a single `Viewer`, alternating browsing and
  viewing screens; `run_prompt()` drives `line_edit_key` for search,
  case-insensitive search, hex goto-offset, and a new goto-line prompt.
  Positional-path argument parsing (empty/directory/file) mirrors
  `App::Init`'s dispatch, minus stdin piping and multi-file arguments.
- Root `CMakeLists.txt`/`src/CMakeLists.txt`: new `lss` executable
  target, the first binary besides the two test runners.

**Deviations:** no `cViewedFiles` multi-buffer list (single active
`Viewer`, replaced on open); no F2-F7 colour cycling; no `FileManager`
command submenu; no syntax-highlighted rendering (renderer doesn't take
a `Style` yet); narrower CLI surface (one positional path argument
only, no `-ignorestdin`/`-raw`/`-nosyntax`/etc., no config-file
loading); browsing type-ahead is file-only (no shift signal for plain
letters over a terminal); bookmark jump uses a plain digit instead of
the original's Alt+G+digit chord; a `:` goto-line prompt in text mode
is a genuine addition, not a port. Full rationale in
`docs/app-main-loop.md`'s "narrowed or deferred" section.

**Bug fixed:** none in ported code. One caught during this work's own
manual testing: an invented `Ctrl+Q`-quits-the-viewer binding never
reached the app because `Terminal` runs ncurses in `cbreak()` mode,
which leaves XON/XOFF flow control active -- dropped rather than
chasing another Ctrl-chord equally likely to be intercepted.

**Tests:** 23 new (`LineEdit.*`, `BrowsingActionsTest.*`/
`ViewingActionsTest.*` in `tests/core`; `FileManagerRenderTest.*` in
`tests/platform_linux`) -- 253 total. `App`'s main loop itself is
deliberately not unit-tested (glue over already-tested pieces, coupled
to a real terminal, the same limitation the original's `App::Run`/
`FileManager::Activate` had) -- verified instead by manually driving
the built `lss` binary in a real terminal (via `tmux send-keys`/
`capture-pane`): browsing, entering directories, opening/closing a
file, scrolling, searching, hex-mode toggling, and quitting from both
entry points (`lss` and `lss <file>`) all confirmed working,
including clean terminal restoration on exit.

**Next steps:** multi-buffer switching, F2-F7 live colour cycling, the
full original CLI flag surface and config-file loading, `FileManager`'s
command submenu, and syntax-highlighted rendering are all real
follow-up work now that there's a runnable binary to hang them off of;
subsystem 11 (file operations) remains next in the numbered breakdown.

## 2026-08-20 — Structured-text syntax highlighting (issue #78)

**Objective:** add generic, configurable structural syntax rules suited
to YAML, JSON, and XML without introducing a language-specific lexer.

**Changes:**
- `Style` and its config parser now support contextual key-before-
  delimiter, line-start prefix, prefix-token, block-text, and associated
  colour rules. The syntax scanner carries block-text indentation state
  across lines and handles multiple nested flow-collection keys while
  respecting quoted delimiters.
- The bundled YAML, JSON, and XML styles use those shared rules. YAML
  now distinguishes mapping keys, sequence markers/data, block scalar
  text, and anchors/tags/aliases/directives; JSON colours quoted object
  keys; XML has a baseline configured style.
- `docs/08-style-config.md` documents the contextual rule vocabulary and
  `docs/09-syntax-highlighting.md` records its activation semantics.

**Decisions:** the scanner remains generic rather than growing a YAML
parser. It supports explicit block scalars and common structured tokens;
unusual YAML constructs requiring full grammar knowledge remain outside
the intended highlighting fidelity.

**Tests:** contextual keys (including quoted and nested flow keys),
block indentation, structural prefixes, sequence data, prefix tokens,
and structured scalar values. The full suite passed with 279 tests.

## 2026-08-20 — Standard pager navigation (issue #39)

**Objective:** make the viewer follow familiar `less`/`more` navigation
conventions while preserving existing Listless bindings.

**Changes:** Space/Ctrl+F page down; `b`/`B`/Ctrl+B page up; `d`/Ctrl+D
and `u`/Ctrl+U move by half a page; Enter/`j`/`k` move one line; and
text-mode `g`/`G` move to the top/bottom. Each navigation binding uses
the existing text or hex scrolling operation as appropriate. `n`/`N`
also alias the already-supported next/previous search-repeat actions.

**Decisions:** `f`/`F` and `h`/`H` retain their existing search and
text/hex-toggle meanings. `?` is deferred: a genuine backward search
needs a reverse-search prompt, not merely a key-dispatch alias.

**Tests:** action-level coverage exercises every new page, half-page,
and line navigation alias in both text and hex modes, text-mode `g`/`G`,
and `n`/`N` search repetition. The full suite passed with 282 tests.
