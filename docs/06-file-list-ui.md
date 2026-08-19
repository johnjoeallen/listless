# Subsystem 06: file-list UI

Source: `original/apps/onscreen/fileman.cpp` (2484 lines), with the
`FileManager`/`FileManDir` class declarations in
`original/apps/onscreen/ostxt.hpp:427-491`. Depends on
[01-string-container-primitives.md](01-string-container-primitives.md),
[02-search-primitives.md](02-search-primitives.md) (glob matching for the
file spec), and [03-directory-enumeration.md](03-directory-enumeration.md)
(`Directory`/`Dirent`). This is `FileManager`, the "ls" half of Listless's
primary deliverable per the README; the file-viewing half is subsystem 07
(`docs/07-file-viewer-core.md`), ported first — see `PORTING_JOURNEY.md`
for the ordering rationale.

## A note on this file's encoding

The checked-in copy of `fileman.cpp` has lost a number of non-ASCII bytes
in transit (e.g. `d.name()[1] != '' && d.name()[1] != ''` at
`fileman.cpp:899`, and box-drawing characters in `DisplayDirent`'s output
format at `fileman.cpp:640-642`). Reading these call sites together (every
"is this a directory?" check tests `d.name()[1]`, i.e. the *second* byte
of the name, against two sentinel values; `MatchSelect` skips the first
byte of a directory's name before substring-matching; `DeleteFile`/
`RenameFile`/`MoveFile` strip the first byte with `+1` before using the
name as a real path) indicates `Dirent::name()` prefixed directory names
with a non-printable marker byte — apparently used to flag "this is a
directory" inline in the name string itself, rather than only through
`isDirectory()`. The marker byte's actual value is unrecoverable from this
checkout. This convention is not reproduced in the port: `DirEntry` (see
`docs/03-directory-enumeration.md`) already carries `is_directory` as an
explicit `bool`, so there is no need for an in-band marker, corrupted or
otherwise.

## What the original provides

- **`FileManDir::fill()`** (`fileman.cpp:439-512`) — a two-pass listing of
  one directory: pass one lists **every** subdirectory (queried with the
  unfiltered pattern `"*"`, skipping `.` but keeping `..`); pass two lists
  files matching the caller's file spec. **The file spec filter never
  applies to directories** — `cd`-ing into a filtered view still shows
  every subdirectory.
- **Sorting** (`sortByName`/`sortByExt`/`sortByDate`/`sortBySize`,
  `fileman.cpp:20-193`) — a `direntCompareFunc*` selected by
  `ChooseSortBy()` (`fileman.cpp:821-889`, bound to `Alt+S` and the `:`/`/`
  command submenu's `S`). All four comparators special-case directories
  identically: two directories always compare by name, ascending,
  regardless of the current sort direction; a directory always sorts
  before a non-directory. Only the non-directory/non-directory comparison
  respects `sortAscending` (toggled by pressing `-` before the sort-key
  letter). `sortByExt` special-cases `".."`, treating it as having no
  extension (`strrchr` on `".."` would otherwise find a spurious `.`).
- **Multi-column grid layout and navigation** — `iColumns`/`iColumnWidth`:
  the constructor sets `iColumns = 3`, then a `do { iColumnWidth =
  di.screenwidth / --iColumns; } while (iColumnWidth < 40)` loop searches
  *downward* for the widest column count that keeps each column at least
  40 characters wide. Because the decrement happens inside the same
  expression as the first division, the loop's first candidate is
  `iColumns - 1`, not `iColumns` — so the default (`iColumns = 3`)
  actually searches from 2 columns downward, never trying 3. `Alt+1`/
  `Alt+2`/`Alt+3` (`fileman.cpp:2402-2418`, `VKALT_1..3`) set
  `iColumns = (key-VKALT_1)+2` (2/3/4) before the same search — so `Alt+1`
  searches from 1 down (effectively forcing 1 column), `Alt+2` searches
  from 2 down (identical to the unmodified default), and `Alt+3` searches
  from 3 down (the only way to actually reach a 3-column layout). This
  off-by-one is mechanical and deterministic, not data-losing, so it's
  preserved as-is rather than "fixed" — see `compute_grid()` below, which
  takes the requested count (3/2/3/4) and reproduces the same
  search-from-`requested-1` behaviour. `iLinesPerColumn` is
  `di.screenheight-3`, reserving 3 status rows. The list is one flat,
  sorted, 0-indexed sequence (`iCurFile`) laid out column-major: index `i`
  sits at row `i % iLinesPerColumn`, column `i / iLinesPerColumn`.
  `iCurColumn` is the left-most *visible* column (there can be more
  logical columns than fit on screen). Up/Down/Left/Right/Home/End/
  PageUp/PageDown (`fileman.cpp:1755-1936`) move `iCurFile` and scroll
  `iCurColumn` to keep it in view; `Home`/`End` have asymmetric special
  cases (jump to the top/bottom of the *current* visible column-window
  first, and only recompute `iCurColumn` if that lands outside the data or
  the cursor was already there) rather than always recomputing from
  scratch.
- **`MatchSelect()`** (`fileman.cpp:1468-1592`) — incremental type-ahead
  select: every plain keypress (not a command/nav key) appends to a
  per-kind accumulated string (`iCurTextFile` normally, `iCurTextDir` if
  Shift is held) and searches for an entry of that kind (file or
  directory) whose name *starts with* the accumulated text
  (case-insensitive `strstri`, matched only at offset 0), moving the
  cursor there. If appending a character finds no match, that character is
  dropped from the accumulated string (so typing continues to filter the
  previous match rather than resetting). Backspace (`0x0008`) removes the
  last accumulated character and re-searches; `Tab`/`Shift+Tab` repeat the
  search forward (find the *next* entry starting with the same text).
  Forward search (`Tab`, typing) scans from `iCurFile+1` to the end, and
  if nothing is found there, wraps by scanning from `iCurFile` back down
  to `0`. Backward search (used by Backspace's re-search) scans from
  `iCurFile-1` down to `0` only — **no wraparound**. This asymmetry reads
  as intentional (typing forward wants to find *something*; narrowing a
  search by deleting a character shouldn't jump somewhere unrelated), and
  is preserved in the port. One further asymmetry is *not* preserved: the
  backward branch is guarded `#if defined(__UNIX__)` to use case-sensitive
  `strstr` instead of `strstri` — but per `docs/03-directory-enumeration.md`,
  **no `__UNIX__` backend exists anywhere in `/original`**, so this branch
  never actually built or ran. The port uses case-insensitive matching in
  both directions.
- **Navigation commands** — `ChangeDirectory()` (`fileman.cpp:1321-1403`,
  bound to `Alt+H`/submenu `H`) and `ChangeFileSpec()`
  (`fileman.cpp:1443-1463`, `Alt+F`/submenu `F`) each prompt via
  `LineEdit` (a modal single-line editor with history — not ported; see
  below) and, on success, reset `iCurFile`/the type-ahead accumulators and
  call `Refresh()` (`fileman.cpp:795-816`, which re-lists, re-sorts, and
  clamps `iCurFile` to the new size). `ChangeDirectory` additionally
  pushes the *previous* directory onto `directoryHistory` and contains
  DOS/OS2/Win32 drive-letter parsing (`s[1] == ':'`) with `IsValidDisk()`/
  `setdisk()`/`chdir()` disk-switching — meaningless on Linux (see below).
  Pressing Enter on a directory entry (`fileman.cpp:1938-1990`) does the
  same `chdir` + `Refresh()` inline, plus (only when the entry was `..`)
  re-selects, in the freshly-listed parent, the child directory just left
  — a "remember where I came from" touch preserved in the port.
- **The `:`/`/` command submenu** (`fileman.cpp:2109-2246`) — a prompt
  (`"~View ~Edit ~Copy ~Del ~Ren ~Move ~Sort C~hdir M~kdir ~FileSpec"`)
  reading one more key to dispatch to `ViewFile`/`EditFile`/`CopyFile`/
  `DeleteFile`/`RenameFile`/`MoveFile`/`ChooseSortBy`/`ChangeDirectory`/
  `MakeDirectory`/`ChangeFileSpec`. Every one of these actions also has its
  own direct `Alt+<letter>` shortcut in the main `Activate()` switch
  (`fileman.cpp:2248-2347`, e.g. `Alt+C` copy, `Alt+D`/`Del` delete,
  `Alt+M` move, `Alt+R` rename, `Alt+S` sort, `Ins`/`Alt+K` make
  directory) — the submenu is a discoverable menu over the same
  operations, not a separate feature surface.
- **File operations** — `EditFile`/`ViewFile`/`CopyFile`/`DeleteFile`/
  `RenameFile`/`MoveFile`/`MakeDirectory` (`fileman.cpp:894-1463`). `View`
  and `Edit` hand off to a `Viewer`/external editor (`ViewFile` also
  de-dupes against already-open buffers via `isDupe`/`findDirent`, which
  belong to `viewedFiles` bookkeeping that doesn't exist yet in
  Listless — no `App`/main-loop subsystem has landed). `Copy`/`Delete`/
  `Rename`/`Move`/`MakeDirectory` mutate the filesystem directly (`unlink`,
  `rmdir`, `rename`, a hand-rolled buffered `CopyFile`, `mkdir`), each with
  a confirmation prompt (Delete) or a `LineEdit` prompt for the
  destination (Copy/Rename/Move/MakeDir), and each reconciles
  `viewedFiles` afterward (dumping/removing the buffer for a file that was
  just deleted/renamed/moved out from under it).
- **`DisplayDisks()`** (`fileman.cpp:562-613`) — a status-bar row of every
  valid DOS/OS2/Win32 drive letter (from `iDriveMap`, a per-platform
  bitmask queried in `Activate()`, `fileman.cpp:1601-1627`), with the
  current drive parenthesized. `Ctrl+A`-`Ctrl+Z` (`fileman.cpp:1653-1684`)
  jump directly to the corresponding drive letter.
- **Per-instance color cycling** (`fileman.cpp:1995-2105`, `F2`-`F6` and
  their Shift-variants) — cycles `setupInfo`'s file/current-file/status/
  disk/current-disk fore/back colors. `setupInfo` is the not-yet-ported
  `Style`/config system (subsystem 08).
- **`LineEdit`** (`ostxt.hpp:404-424`) — a modal single-line text editor
  with an optional history list (`SList<CString>*`), used by every prompt
  above (`Directory`, `MakeDir`, `FileSpec`, `Copy To`, `Rename`,
  `Move To`). Not part of `FileManager` itself; a general input-prompt
  widget shared with other parts of the original app.

## What's ported here

- **`FileManager`** (`src/file_manager.hpp`/`.cpp`) — pure logic and
  state: listing (directories unfiltered, files filtered by the current
  glob file spec, exactly matching the original's two-pass `fill()`),
  sorting (`SortKey::Name`/`Extension`/`Date`/`Size`, ascending/descending,
  directories-first-then-name-ascending in every mode, matching all four
  original comparators), the column-major grid navigation model
  (`move_up`/`move_down`/`move_left`/`move_right`/`move_home`/`move_end`/
  `move_page_up`/`move_page_down`, each taking a `Grid{columns,
  lines_per_column}` the same way subsystem 07's `Viewer::scroll_*()`
  takes `visible_lines`/`visible_width` per call rather than storing
  screen geometry), `compute_grid()` (the 40-column-minimum,
  search-from-`requested-1` layout math described above, as a free
  function taking the requested column count so a caller can implement
  the `Alt+1`/`2`/`3` override by passing 2/3/4, matching the original's
  `VKALT_1..3` mapping), type-ahead multi-select
  (`match_select_file`/`match_select_directory`, both directions, with the
  forward-wraps/backward-doesn't asymmetry preserved), `change_directory()`
  and `set_file_spec()` (re-list + reset selection + reset type-ahead,
  matching `ChangeDirectory`/`ChangeFileSpec`'s post-success behaviour),
  and directory history (`directory_history()`, appended to on every
  successful `change_directory()`, mirroring `directoryHistory`). No
  terminal/rendering dependency, matching the `Viewer`/`viewer_render`
  split from subsystem 07 — a `file_manager_render` layer is left for
  whichever subsystem first wires a real screen loop.
- **`enter_selected()`** — separate from `change_directory()` (a typed
  path), matching the original keeping the Enter-key handler
  (`fileman.cpp:1938-1990`) and the typed-path `ChangeDirectory()`
  command as two different code paths with different behaviour: entering
  `..` this way re-selects, in the freshly-listed parent, the directory
  just left, porting `fileman.cpp:1958-1983`'s "remember which
  subdirectory I came from" touch. `change_directory()` has no such
  re-selection (matching the original -- `ChangeDirectory()` never had
  it either).
- **`change_directory()` does not change the process's working
  directory** — the original's `chdir()`-based navigation is a real
  process-wide `chdir(2)`/`setdisk()` (`fileman.cpp:1376`,
  `fileman.cpp:1661`), consistent with DOS/OS2/Win32 apps generally
  running as the sole occupant of their console. `FileManager` instead
  just tracks `current_directory()` as its own state, like
  `Directory`/`DirEntry` already do (`docs/03-directory-enumeration.md`)
  -- a real process-wide `chdir()` would be global, hard-to-reverse
  state that races across parallel unit tests and has no test-only
  upside; nothing here needs the process's actual working directory to
  match.

## What's deliberately not ported

- **File operations** (Copy/Delete/Rename/Move/MakeDirectory) — every one
  of these mutates the filesystem. Per this subsystem's issue, they're
  deferred to subsystem 11 (file operations), sequenced after everything
  else is stable given their destructive nature and the higher regression-
  test scrutiny that implies. `FileManager` exposes only the read-only
  surface (listing, sorting, selecting, navigating); there is no
  `copy_file()`/`delete_file()`/etc. here to accidentally call.
- **`EditFile`/`ViewFile`** — both depend on `viewedFiles`/`currFile`
  buffer-list bookkeeping that belongs to an `App`/main-loop subsystem
  that doesn't exist yet (no subsystem in `docs/architecture.md`'s
  breakdown has landed a `main()` that owns a `SList<Viewer>` the way the
  original's `os.cpp` does). `FileManager` surfaces enough to let a future
  caller build this (`selected()`, `selected().is_directory`) without
  committing to a buffer-list design here.
- **`DisplayDisks()`/drive-letter bar, and `Ctrl+A`-`Ctrl+Z` drive
  jumps** — meaningless on Linux, which has one filesystem namespace, not
  lettered drives. A Linux-relevant reinterpretation (a bar of mounted
  filesystems, or of frequently-used directories) is real UI design work
  with no concrete rendering call site yet (no screen loop exists to draw
  it in) — deferred rather than guessed at now, the same call made for
  `Style`-dependent formatting in `docs/07-file-viewer-core.md`.
- **Per-instance `F2`-`F6` color cycling** — depends on `setupInfo`
  (subsystem 08, not yet ported), exactly as subsystem 07 deferred its
  three `Style`-selected status-line formats for the same reason.
- **`LineEdit`** — a general modal-prompt widget, not specific to
  `FileManager`; out of scope here. A caller wiring `change_directory()`/
  `set_file_spec()` to a real keyboard loop will need some prompt input
  mechanism, but building it against this one call site would bake in
  assumptions; better built (or reused, if another subsystem needs it
  first) against a second concrete call site.
- **`Activate()`'s keyboard-loop dispatch itself** — the giant `switch` in
  `fileman.cpp:1649-2472` is what a future screen-loop/`App` subsystem
  will translate into calls against `FileManager`'s ported surface (plus
  whatever `file_manager_render` eventually draws). Documented above for
  that future work's reference; not itself code to port, since it's
  entirely glue over pieces (menu prompts, color cycling, drive bar, file
  operations, `viewedFiles`) that are either deferred or don't exist yet.
</content>
