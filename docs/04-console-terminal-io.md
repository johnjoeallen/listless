# Subsystem 04: console/terminal I/O

Source: `original/apps/onscreen/osvideo.cpp`, `DisplayCell`/
`DisplayBufferInfo` (`original/apps/onscreen/ostxt.hpp`). Gates
everything visual — needed by subsystems 06 (file-list UI) and 07 (file
viewer core).

## What the original provides

A screen-buffer API built on Borland's `conio.h` (OS/2, DOS) or
hand-rolled Win32 console calls:

- `GetDisplayBufferInfo(&di)` — screen width/height, cursor position,
  current attribute (`DisplayBufferInfo`: `screenwidth`, `screenheight`,
  `x`, `y`, `attribute`).
- `MoveToXY(x, y)` — move the cursor, 1-indexed (DOS/`conio.h`
  convention).
- `PutTextBuf`/`GetTextBuf(x, y, wx, wy, buf)` — write/read a rectangular
  region of `DisplayCell`s (`{char, attribute}`, where `attribute` packs
  foreground (low nibble) and background (high nibble) as `bg*16 + fg`).
- `MoveTextBuf(left, top, right, bottom, destleft, desttop)` — move/copy
  an arbitrary rectangular region, used for both scrolling and (in
  principle) horizontal block moves.
- `clrline()` — clear from the cursor to the end of the current line,
  using the current attribute.
- `setTextAttr(fg, bg)` — set the current attribute (`bg*16 + fg`).
- `tprintf(fmt, ...)` — `printf`-style output at the cursor position,
  advancing the cursor.

Colours are the classic 16-value PC BIOS/CGA text-attribute palette
(0=Black, 1=Blue, 2=Green, 3=Cyan, 4=Red, 5=Magenta, 6=Brown,
7=LightGray, 8=DarkGray, 9=LightBlue, 10=LightGreen, 11=LightCyan,
12=LightRed, 13=LightMagenta, 14=Yellow, 15=White) — **this exact
numeric encoding matters**: `Style`'s colour fields (subsystem 08,
`osstyle.cpp`) store raw `BYTE` values 0-15 in this order, persisted to
`os.set`, so a faithful port needs to preserve the same palette and
numbering, not just "16 colours in some order."

## Decision: ncurses

Chosen over a custom termios+ANSI layer (see conversation/issue #4):
mature, handles terminal-capability differences automatically, and the
same interface extends to Windows (PDCurses) and macOS (ncurses) later
without redesigning the API. Verified in this sandboxed environment
(no real TTY) that `initscr()`/`endwin()` work correctly as long as
`TERM` is set — screen size falls back to a sane default (24x80) when
the terminal size can't be queried, and colour support
(`has_colors()`/`COLORS`/`COLOR_PAIRS`) populates correctly after
`start_color()`. This makes the ncurses-backed implementation
genuinely unit-testable (constructing/tearing down a real `Terminal`
per test, reading back cell contents via ncurses' own `mvinch()`), not
just manually verifiable.

## What's ported here

- **`Color`** (`src/color.hpp`/`.cpp`) — the original's 16-value DOS
  palette, preserved with the same numeric values for future `Style`
  compatibility. `to_ansi(Color)` is a pure function (no ncurses
  dependency, fully unit-tested) mapping a `Color` to `{ansi_base 0-7,
  bright}` — the DOS palette's colour *order* differs from ANSI/xterm's
  (e.g. DOS puts Blue at 1 and Red at 4; ANSI has Red at 1 and Blue at 4),
  so this is a real lookup table, not an identity mapping.
- **`ColorPairTable`** (`src/color_pair_table.hpp`/`.cpp`) — lazily
  allocates small integer "pair" ids for `(fg, bg)` combinations, the
  model ncurses (and similar cell-attribute terminal APIs) use instead of
  arbitrary independent fg/bg attributes. Pure logic, no ncurses
  dependency — capacity-bounded with a documented fallback (returns the
  default pair once exhausted, rather than failing), fully unit-tested
  with an injected allocation callback standing in for `init_pair()`.
- **`Terminal`** (`src/terminal.hpp`, implemented in
  `platform/linux/terminal.cpp`) — a Pimpl-style class (declared once in
  `/src`, one implementation file per platform selected by CMake, no
  `#ifdef` in the shared header): `width()`/`height()`, `move_cursor(x, y)`
  (0-indexed — new code, no reason to keep the DOS 1-indexing
  convention), `put_text(x, y, text, fg, bg)`, `clear_to_eol(x, y, fg, bg)`,
  `clear()`, `scroll_region(top, bottom, lines)`, `refresh()`.
  Colours degrade gracefully to `A_NORMAL` if the terminal reports no
  colour support at all.

## What's deliberately narrowed or not ported

- **`MoveTextBuf`'s generic rectangular block-move** is narrowed to
  `scroll_region(top, bottom, lines)` — full-width vertical scrolling
  only. This is the realistic use case (the file viewer scrolling its
  text display), it's what ncurses' `wscrl`/`wsetscrreg` natively
  support, and ncurses has no built-in arbitrary horizontal block-move
  primitive to justify replicating the original's fully general (but,
  as far as could be found, never used for anything but scrolling)
  signature.
- **`GetTextBuf`** (reading an arbitrary screen region back out, e.g. for
  save-and-restore-under-a-popup) is not ported. ncurses' own windowing
  model (subwindows/pads) is the idiomatic way to layer temporary UI
  (menus, dialogs) without manual buffer save/restore, and no concrete
  call site exists yet to design against — if popups need this in a
  later subsystem, they should be built on ncurses windows rather than
  reviving a manual `GetTextBuf`/`PutTextBuf` round-trip.
- **1-indexed coordinates** are not carried forward — `Terminal` is a new
  interface with no legacy call sites, so it uses 0-indexed coordinates
  throughout, matching ncurses and the rest of modern C++ (`std::`
  containers, `std::filesystem`).
- **The named screen-write mutex** (`hMtxListSync`, see
  `docs/architecture.md`) is not carried forward as a synchronization
  primitive here. Whether Listless needs a background clock-update
  thread at all (the original's actual reason for that mutex) is left
  as an open question for whichever later subsystem needs a live status
  line — a single-threaded event loop with a timeout on input read
  (idiomatic for ncurses TUIs) may remove the need entirely.
