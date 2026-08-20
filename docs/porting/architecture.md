# OnScreen/2 architecture (as found in `/original`)

Source: the [OS2World OnScreen/2 repository](https://github.com/OS2World/UTIL-FILEMANAGER-OnScreen2):
`apps/onscreen` (application), `class` and `func` (implementation of the
utility library), and `include` (headers). Copyright 1993-2006 John J.
Allen, GPLv2 or later.

## Top-level program structure

`App` (`apps/onscreen/os.cpp`, globals in `apps/onscreen/globals.cpp`) is
the top-level object:

1. `Init()` parses argv with a custom, non-POSIX `getopt`
   (`include/getopt.hpp`, `func/getopt.cpp`), builds the default `Style`,
   loads `os.set` (`osstyle.cpp: loadConfig`/`saveConfig`), and sets up the
   screen.
2. Depending on arguments, it either opens a `FileManager` (no path
   argument, or the argument is a directory) or loads file(s) directly
   into `cViewedFiles`, an `SList<Viewer>` — one `Viewer` per open file
   buffer, switchable via digit keys 1-9, Alt+N/P, or a buffer picker.
3. `Run()` is a single large keyboard-driven event loop dispatching on raw
   and synthetic key codes (tab width, F2-F7 live colour cycling,
   search-mode toggle, buffer switching, Alt+O shell escape), forwarding
   anything unhandled to `Viewer::handleKey`.

Global state lives in `globals.cpp` as raw globals/statics:
`gDefaultStyle`, `gCurrStyle`, `gCurrFile` (index into `cViewedFiles`),
`gFileManager`, `gStartupDir`, and `setupInfo` (flat struct of UI toggles:
sound, regexp-search-mode, case-sensitivity, keep-files-loaded, status
colours). A Listless port should own this state in an explicit
session/app object instead of globals.

## FileManager — the directory viewer ("ls" half)

`apps/onscreen/fileman.cpp` (2484 lines). One directory is displayed as a
multi-column sorted file list (`iColumns`/`iColumnWidth`/`iLinesPerColumn`);
this is not a two-directory-pane design. It includes a DOS/OS2/Win32
drive-letter bar (`DisplayDisks` — meaningless on Linux, should collapse
to cwd/mount points), sortable by name/ext/date/size (`ChooseSortBy`),
glob-pattern file filtering plus incremental type-ahead selection
(`ChangeFileSpec`/`MatchSelect`), and direct file operations
(Copy/Delete/Rename/Move/Edit/View/MakeDirectory, `fileman.cpp:894-1321`).
`Activate()` (`fileman.cpp:1597` onward, ~850 lines) is its own keyboard
loop, with a `:`/`/`-triggered command submenu
(`fileman.cpp:2110-2246`) and direct Alt+letter shortcuts.

## Viewer — the file viewer ("less" half)

`apps/onscreen/osview.cpp` (3107 lines). Holds `LinePtr[]` (pointers into
a loaded buffer plus `LineStatus` syntax state). Supports:

- text mode and hex mode (`switchToHexMode`/`switchToTextMode`,
  `displayDataAsText`/`displayDataAsHex`)
- line-wrap decisions (`shouldWrap`)
- 10 bookmark slots (`iBookMark[10]`)
- search-selection highlighting, forward/backward, case-toggle,
  plain (Boyer-Moore-Horspool, `func/strsrch.cpp`) or regex search
- per-line syntax colouring driven by the active `Style` (bold/underline
  toggle bytes `BOLD_CODE`/`UNDERLINE_CODE`, comment/preprocessor state
  tracked via `LineStatus`)

`handleKeyInTextMode`/`handleKeyInHexMode` are separate large key-dispatch
functions.

## Style — config and syntax highlighting

Declared in `apps/onscreen/os.hpp`, populated in `apps/onscreen/osstyle.cpp`
(1717 lines). A per-filetype config record (colours, tab width, syntax
rules, reserved words, comment delimiters) with a genuine prototype-style
inheritance mechanism: `Style::AddBaseStyle`/`iBaseStyles`, resolved
lazily through `Item<T>::GetItem()`'s fallback chain (`os.hpp:210-244`) —
an item asks its own value first, then walks base styles depth-first.
`loadConfig`/`saveConfig` (`osstyle.cpp:383,782`) persist this to `os.set`.

F2-F7 cycle foreground/background/selected-fg/selected-bg/bold/underline
colours live while viewing a file — a distinctive OnScreen/2 feature,
persisted with Ctrl+S.

## Platform seams

Today these are raw `#ifdef __OS2__` / `__WIN32__` / `__MSDOS__` /
`__DPMI32__` / `__UNIX__` / `__MACINTOSH__`, scattered through headers and
`.cpp` files — there is no abstraction layer. Four seams matter for a
Linux-first, Windows/macOS-permanent port:

1. **File enumeration.** `class/dir.cpp` textually `#include`s one of
   `class/{OS2,win32,DOS}/dir.cpp` based on macro, each implementing
   `Directory::fill()` via `DosFindFirst`/`FindFirstFile`/
   `_dos_findfirst` respectively. **There is no `__UNIX__` implementation
   in `/original` at all** — Linux directory enumeration is new platform
   work, not a `#ifdef` flip. Target interface: `fill(pattern)` populates
   `Dirent[]` with name/size/mtime/mode/`attrib_t`, backed by
   `std::filesystem::directory_iterator` on Linux.
2. **Console/video I/O.** `apps/onscreen/osvideo.cpp`:
   `GetTextBuf`/`PutTextBuf`/`MoveTextBuf`/`MoveToXY`/`setTextAttr`/
   `clrline`, built on Borland `conio.h` (OS/2, DOS) or hand-rolled
   `WriteConsoleOutput`/`ScrollConsoleScreenBuffer`/
   `SetConsoleCursorPosition` (Win32). Target interface: get/put a
   rectangular cell buffer (char + attribute), move cursor, query screen
   size, scroll a region, set current attribute — maps cleanly onto
   ncurses.
3. **Keyboard input.** `apps/onscreen/osgetch.cpp` reimplements
   `getch`/`kbhit`/`ungetch` per platform (Win32: full NT virtual-key to
   BIOS-scancode table, 90+ entries; DPMI32: `bioskey`). The app already
   uses a synthetic `0xFFxx` range for extended keys (e.g. `0xFF3B` = F1,
   `0xFF47` = Home) — worth preserving as Listless's internal keycode
   model, fed by an ncurses or termios+escape-sequence backend on Linux.
4. **Timing.** `apps/onscreen/ostime.cpp` — a thread updates a live clock
   in the status line, spawned via `_beginthread` on OS/2/Win32. Trivially
   portable via `std::jthread` + `std::chrono`.

There is also a named OS mutex semaphore (`hMtxListSync`, created via
`DosCreateMutexSem`/`CreateMutex` in `os.cpp:444-460`), requested/released
through the `WaitSync()`/`ClearSync()` wrappers (`osmisc.cpp:47-70`).
**Investigated: it is a screen-write mutex, not IPC.** It brackets every
direct screen-buffer write across `osview.cpp`, `fileman.cpp`,
`picklist.cpp`, and `osedit.cpp`, and its actual reason for existing is
`ostime.cpp`'s `displayTime()` — called roughly once a second from a
background clock-update thread (`dispTime`, spawned via `_beginthread` in
`os.cpp`) to redraw the status-line clock and check
`Viewer::fileHasChanged()`. Its job is preventing the main thread's screen
redraws from interleaving with that background thread's status-line
writes; it's named/cross-process only incidentally to the OS/2 API shape
(nothing in `/original` opens it from a second process). A plain
`std::mutex` guarding the screen/terminal-write path is a sufficient
replacement for subsystem 04 (console I/O) — and that subsystem should
also decide whether the background clock-thread model is wanted at all in
the Linux port, versus a single-threaded event loop with a timeout on
input read (common in ncurses TUIs), which would remove the need for this
synchronization entirely.

## Command-line surface to preserve

`-ignorestdin`, `-raw <style>`, `-nosyntax <style>`, `-search regexp|plain`,
`-textwithlayout on|off`, `-highbit on|off`, plus stdin piping (redirected
input becomes a pseudo-file `<stdin>` in the buffer list).

## Known-broken / dead code in `/original`

These are read as historical fact, not requirements — do not port them:

- **`RegExp` is non-functional in the shipped source.** `include/regexp.hpp`
  declares a Henry Spencer-style ERE engine; `class/regexp.cpp` is a
  25-line stub with no function bodies. This matches `original/Readme.txt`'s
  note that the regex engine was stripped for redistribution-rights
  reasons. `CString::operator==(RegExp&)`, `Date`/`Time` string parsing,
  and `dir.cpp`'s `FileExp2RegExp` glob-to-regex translator all depend on
  it — **the shipped source does not build as-is**. Must be designed out
  (`std::regex`, or a dedicated glob matcher) rather than ported.
- `include/memman.hpp` — a vestigial, unused, non-compiling stub
  (`tepmlate <class T>` typo); the real `RefPtr` lives in
  `include/refptr.hpp`.
- `Storage<T>::operator=` in `include/refptr.hpp` looks broken (refcounts
  the wrong object on assignment). `RCString`/`RSubCString`
  (`include/cstring.hpp`) are typedef'd but not obviously used anywhere in
  the application files — confirm zero use sites before deciding whether
  to port `RefPtr` at all.
- `MEnsure()` (`include/conditio.hpp`) silently compiles to a bare
  `while (Condition)` in non-`DEBUG` builds instead of asserting — a
  latent hang bug. Worth fixing during the port per the project's own
  "preserve behaviour, but fix genuine bugs" principle, with a regression
  test.
- `apps/onscreen/install.cpp` (1300 lines) is a DOS/OS2/Win32 `.set`/`.ini`
  installer, branded to the original product. **Out of scope, decided —
  not being ported.** Listless will use native packaging per platform
  instead: a `.deb` (and likely other distro packages later) on Linux, a
  zip or `.msi` on Windows, and a to-be-decided format on macOS (`.pkg`
  or a `.dmg`/`.app` bundle).

## Proposed subsystem breakdown (porting order)

Foundational subsystems first, so later ones are written against modern
types from day one rather than ported-then-refactored:

1. **String/container primitives** — `CString` → `std::string`,
   `Array`/`SortableArray` → `std::vector` (+ `std::sort`, replacing the
   hand-rolled, non-reentrant `QSort`), `SList`/`DList`/`Set` →
   `std::list`/`std::vector`+dedup, `Date`/`Time` → `std::chrono`.
2. **Search primitives** — literal search (`func/strsrch.cpp`'s
   Boyer-Moore-Horspool implementation is correct and directly portable)
   and glob-pattern matching (reimplement `FileExp2RegExp`'s glob syntax
   without a stripped regex engine). Resolves the `RegExp` build blocker.
3. **Directory enumeration** — `Directory`/`Dirent`/`FileTreeWalker` with
   a new Linux backend (`std::filesystem`). Listless's core "ls" behaviour.
4. **Console/terminal I/O** — screen buffer get/put/scroll, cursor
   movement, attributes. Gates everything visual; ncurses vs. a custom
   termios+ANSI layer is an open decision.
5. **Keyboard input** — normalized keycode model (reuse the existing
   synthetic `0xFFxx` extended-key space), ncurses/termios backend.
6. **File-list UI** — port `FileManager`: multi-column list, sort,
   navigate, select. Listless's primary deliverable per the README.
7. **File viewer core** — port `Viewer`: text-mode display, scrolling,
   search, bookmarks.
8. **Style/config system** — `Style`, `Item<T>` inheritance, `os.set`
   load/save. Needed before syntax highlighting, otherwise decoupled from
   the viewer core.
9. **Syntax highlighting** — layers on style + viewer.
10. **Hex-mode viewer** — smaller, mostly self-contained addition.
11. **File operations** (copy/move/rename/delete/mkdir) — destructive,
    sequence after everything above is stable and tested; highest
    regression-test scrutiny.
12. **Editor integration / shell escape** — polish features, lowest risk if
    deferred. Live style editing is no longer planned.
