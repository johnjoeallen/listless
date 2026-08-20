# App entry point / main loop (issue #24)

Source: `original/apps/onscreen/os.cpp` (`App::Init`, `App::Run`),
`original/apps/onscreen/osview.cpp` (`Viewer::handleKey`,
`handleKeyInHexMode`), `original/apps/onscreen/fileman.cpp`
(`FileManager::Activate`). Unlike subsystems 01-10, this isn't in
`docs/architecture.md`'s numbered 1-12 breakdown -- it's the
cross-cutting piece every one of those subsystem docs explicitly
deferred to "an App/main-loop subsystem" that didn't exist yet. This
page wires `FileManager` (06) and `Viewer`+hex mode (07/09/10) into a
real interactive session using `Terminal`/`Keyboard` (04/05), producing
the first runnable `lss` binary.

## What the original does

- **`App::Init`** (`os.cpp:212-521`) does a lot in one function: screen
  geometry checks, OS-specific hard-error suppression, building
  `gDefaultStyle` (colours, tab width, syntax-highlighting defaults) and
  loading `os.set` over it, a `getopt`-based CLI parse (`-ignorestdin`,
  `-search regexp`, `-raw`, `-textwithlayout`, `-highbit`, `-nosyntax
  <style>`, etc. -- `os.cpp:296-419`), allocating the screen buffer, and
  finally mode dispatch: redirected stdin (not a tty) becomes a `Viewer`
  over a synthetic "stdin" `FileInfo`; a single directory-path argument
  (or no argument, when stdin *is* a tty) opens a `FileManager` via
  `Activate()`; one or more file-path arguments load each into a
  `Viewer` via `AddFileList`, appended to `cViewedFiles`
  (`SList<Viewer>`).
- **`App::Run`** (`os.cpp:530+`) is one large keyboard-driven loop over
  `cViewedFiles`/`gCurrFile`: buffer switching (digit keys 1-9, Alt+N/P,
  a buffer picker), tab-width and F2-F7 live-colour-cycling keys, search-
  mode toggle, Alt+O shell escape, forwarding everything else to
  `Viewer::handleKey`.
- **`Viewer::handleKey`** (`osview.cpp:2650+`) dispatches search
  (`/`/`s`/`S` case-sensitive, `f`/`F`/`\` case-insensitive, both
  prompted via `LineEdit`), repeat-search (`a`/`A`, Alt+A, `^L`),
  `h`/`H` to toggle hex/text mode (`osview.cpp:3077-3090`), Alt+E to
  invoke an external editor, and falls through to
  `handleKeyInTextMode`/`handleKeyInHexMode` for scrolling, bookmarks
  (Alt+0-9 set, Alt+G+digit jump), and hex mode's `g`/`G` goto-offset
  prompt (also via `LineEdit`).
- **`FileManager::Activate`** (`fileman.cpp:1597+`, ~850 lines) is its
  own separate keyboard loop: navigation, a `:`/`/` command submenu,
  sort-key toggles, and `Enter` opening the selected entry (a directory
  re-enters `Activate` on it; a file starts a `Viewer` and returns to
  `App::Run`'s buffer-list handling).
- **`LineEdit`** (`os.hpp`/`oswidget.cpp`) is a general single-line
  modal prompt with its own history list per call site
  (`searchTextHistory`, `gotoOffsetHistory`, etc.), used by every prompt
  above.

## What's ported here

- **`App`** (`src/App.hpp`/`.cpp`, `src/main.cpp`) -- owns `Terminal`,
  `Keyboard`, a `FileManager`, and a single `std::optional<Viewer>`
  (no `cViewedFiles` buffer list -- see "narrowed" below), alternating
  between a browsing screen and a viewing screen. `main.cpp` parses at
  most one positional path argument: empty -> browse the current
  directory; a directory -> browse it; a file -> open it in the
  viewer, with `FileManager` positioned on its parent directory as
  where `Close` (`q`/Escape from the viewer) returns to. This mirrors
  `App::Init`'s directory-vs-file dispatch, minus the tty/stdin-piping
  branch and the multi-file-argument case (both deferred).
- **`handle_browsing_key`/`handle_viewing_key`**
  (`src/AppActions.hpp`/`.cpp`) -- pure key-dispatch tables, direct
  ports of the *reachable-key* subset of `FileManager::Activate`'s and
  `Viewer::handleKey`'s switch statements, factored out so they're
  unit-testable against real `FileManager`/`Viewer` instances without a
  `Terminal`:
  - Browsing: arrows/Home/End/PageUp/PageDown -> `FileManager::move_*`;
    Enter -> `enter_selected()` on a directory, else `OpenSelected`
    (App opens a `Viewer`); printable characters -> `type_ahead_append`
    (files only -- see below); Backspace -> `type_ahead_backspace`;
    `q`/`Q`/Escape -> `Quit`.
  - Viewing: arrows/PageUp/PageDown/Home/End/Left/Right -> `scroll_*`/
    `hex_scroll_*` depending on `display_mode()`; `h`/`H` -> direct
    port of the hex/text toggle (`osview.cpp:3077-3090`); `s`/`S`/`/`
    -> `PromptSearchForward` (case-sensitive, matches the original's
    letters exactly); `f`/`F` -> `PromptSearchForwardCaseInsensitive`;
    `a`/`A` -> `repeat_search(true/false)`, a direct port; `g`/`G` in
    hex mode -> `PromptGotoOffset`, a direct port of
    `handleKeyInHexMode`'s goto-offset prompt; plain `0`-`9` -> jump to
    that bookmark slot, Alt+`0`-`9` -> set it at the current position;
    `q`/`Q`/Escape -> `Close`.
- **`line_edit_key`** (`src/LineEdit.hpp`/`.cpp`) -- `LineEdit`'s
  key-handling core as a pure function: appends printable ASCII,
  Backspace pops a character, Enter submits, Escape cancels. `App`'s
  `run_prompt()` drives it in a loop, rendering the prompt on the
  terminal's last row, for all four call sites above (search pattern,
  case-insensitive search pattern, hex goto-offset, and one addition --
  see below).
- **`render_file_manager`** (`src/FileManagerRender.hpp`/`.cpp`) --
  the renderer `FileManager` never had (subsystem 06 explicitly left
  this for whoever built the screen loop): a status line plus
  `FileManager`'s column-major grid, using `compute_grid()`'s geometry
  exactly as computed. Modeled directly on `ViewerRender.cpp`'s shape.
- **The `lss` executable** -- `add_executable(lss src/main.cpp
  src/App.cpp)` in the root `CMakeLists.txt`, the first binary target in
  the project besides the two test runners.

## What's deliberately narrowed or deferred

- **No `cViewedFiles`/`gCurrFile` buffer list** -- `App` holds exactly
  one `Viewer` at a time; opening a new file replaces it rather than
  adding to a switchable list. Digit-key/Alt+N/Alt+P buffer switching
  and the buffer picker are not implemented. A real multi-buffer design
  is enough work (and enough new test surface) to be its own follow-up.
- **No F2-F7 live colour cycling** -- depends on per-instance style
  state (`setupInfo`) the same way subsystems 06/07 already deferred it;
  `Style::cycle_color()` (subsystem 08) exists but nothing here calls
  it yet.
- **No `FileManager` `:`/`/` command submenu** -- `Activate()`'s command
  menu (sort-key changes, explicit `cd`, file-spec filter entry) isn't
  wired to any key; `FileManager::set_sort()`/`change_directory()`/
  `set_file_spec()` are all ready for it, just not reachable yet.
- **No file operations** -- copy/move/rename/delete/mkdir are already
  explicitly subsystem 11 in `docs/architecture.md`, sequenced after
  everything else for their destructive-operation regression-test
  scrutiny; nothing here adds them.
- **No syntax-highlighted rendering** -- `render_viewer`/`draw_text_line`
  don't take a `Style` or call `highlight_line` (subsystem 09); wiring
  colour spans into the text renderer (and deciding which `Style` is
  active per file) is real work of its own, left for a follow-up.
- **Narrower CLI surface** -- only a single optional positional path
  argument. `-ignorestdin`, `-raw`, `-nosyntax <style>`, `-search
  regexp`, `-textwithlayout`, `-highbit`, redirected-stdin piping, and
  multiple file arguments are all unimplemented; there is no config-file
  (`os.set`-equivalent) loading into `App` either (subsystem 08's
  `load_config`/`StyleSet` exist but nothing here calls them).
- **File-only type-ahead in browsing mode** -- the original splits
  type-ahead between files and directories based on whether Shift is
  held (`iCurTextFile` vs `iCurTextDir`); a terminal's key stream carries
  no shift signal for plain letters, so this port always searches files.
  Directories are still reachable via arrow keys.
- **Alt+`0`-`9` bookmark jump uses a plain digit, not Alt+G+digit** --
  the original's jump is a two-key chord (`Alt+G` then a digit); this
  port binds a plain digit directly to "jump to that slot" instead,
  simpler to implement and to discover, at the cost of an exact key-chord
  match.
- **`:` goto-line prompt in text mode is a genuine addition, not a
  port** -- the original has no single keystroke for "jump to line N";
  this reuses the already-tested `Viewer::goto_line` behind a `LineEdit`
  prompt since it was nearly free to add once the prompt machinery
  existed for search/goto-offset.
- **No `LineEdit` history** -- `searchTextHistory`/`gotoOffsetHistory`
  (recall a previous prompt entry with an up-arrow) aren't implemented;
  `line_edit_key` has no history state to carry.
- **No `Alt+E` external editor invocation**, **no `Alt+O` shell escape**
  -- both spawn an external process from inside the session, out of
  scope for getting a runnable binary working.
- **`App`'s main loop itself is not unit-tested** -- it's glue over
  already-tested pieces (`FileManager`, `Viewer`, `handle_browsing_key`/
  `handle_viewing_key`, `line_edit_key`), coupled to a real `Terminal`/
  `Keyboard`, the same limitation the original's `App::Run`/
  `FileManager::Activate` had. Verified manually in a real terminal
  instead (browsing, entering directories, opening/closing a file,
  scrolling, searching, hex-mode toggling, and quitting all confirmed
  working, including that the terminal is restored cleanly on exit).
  One thing manual testing did catch: a `Ctrl+Q`-to-quit-from-viewer
  binding was tried and dropped after finding `Terminal`'s `cbreak()`
  ncurses mode leaves XON/XOFF flow control active, so `Ctrl+Q`/`Ctrl+S`
  never reach the application at all.
