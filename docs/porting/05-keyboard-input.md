# Subsystem 05: keyboard input

Source: `original/apps/onscreen/osgetch.cpp` (`getch`/`kbhit`/`ungetch`
reimplementations for Win32 and DPMI32), `original/apps/onscreen/osmisc.cpp`
(`getKey()`, the app-level wrapper), `original/apps/onscreen/ostxt.hpp`
(`VKALT_*`/`VKCTRL_*` constants). Depends on subsystem 04 (shares its
already-initialized ncurses state).

## The original's keycode model

`getKey()` (`osmisc.cpp:104-147`) calls `getch()`, and if it returns `0`
(the PC BIOS extended-key sentinel — AL=0 in the real INT 16h keyboard
interrupt convention that `conio.h`'s `getch()` mimics), calls `getch()`
again and adds `0xFF00`:

```c
key = getch();
if (key == 0) {
    key = getch() + 0xFF00;
}
```

So the app's internal keycode space is:

- **0-255**: a plain ASCII character or control code (`Ctrl+A`=1 ...
  `Ctrl+Z`=26, `Escape`=27, etc.) — resolved by the platform's `getch()`
  exactly as a DOS/BIOS keyboard read would.
- **`0xFF00 + scancode`**: an "extended" key — arrows, function keys,
  Home/End/PgUp/PgDn/Insert/Delete, `Alt+letter`/`Alt+digit`. `scancode`
  is a literal **PC/AT keyboard Set-1 scan code** (the same table used by
  real DOS software and, not coincidentally, by `osgetch.cpp`'s Win32
  `kbdtab[]`, which maps NT virtual keycodes back to these same BIOS
  scan-code values for compatibility).

This is why `os.cpp`'s key-dispatch `switch` statements use literals like
`case 0xFF3B: // F1` and `case 0xFF47: // Home` directly — cross-checked
against `kbdtab[]` and `ostxt.hpp`'s `VKALT_*` table, these are internally
consistent standard PC/AT scan codes (F1=0x3B, Home=0x47, Up=0x48,
PgUp=0x49, Left=0x4B, Right=0x4D, End=0x4F, Down=0x50, Insert=0x52,
Delete=0x53, F2-F10=0x3C-0x44, F11=0x85, F12=0x86, Shift+Tab=0x0F, and the
`VKALT_*` table for `Alt+A`-`Alt+Z`/`Alt+1`-`Alt+9`, plus `Alt+0` at scan
code `0x81` — no named `VKALT_0` constant exists, but this scan code is
used at a real call site, subsystem 07's bookmark slot 0; see
`docs/07-file-viewer-core.md`).

## What's ported here

`docs/architecture.md` already flagged this exact `0xFFxx` encoding as
worth preserving as Listless's internal keycode model — this subsystem
implements that:

- **`KeyCode`** (`src/Key.hpp`/`.cpp`) — a plain `int` alias, plus a
  `Key` namespace of named constants for the unambiguous, directly-
  mappable extended keys (arrows, Home/End/PgUp/PgDn/Insert/Delete,
  Shift+Tab, F1-F12), using the exact same numeric values as the
  original's literals above — so later subsystems porting `os.cpp`'s/
  `fileman.cpp`'s/`osview.cpp`'s key-dispatch `switch` statements can
  reuse the original's `case` literals directly. `alt_key(char)` returns
  the `Alt+<letter/digit>` keycode from the same `VKALT_*` table.
- **`Keyboard`** (`src/Keyboard.hpp`, implemented in
  `platform/linux/Keyboard.cpp`) — a Pimpl class, constructed with a
  `Terminal&` purely to enforce (at the type level and in practice) that
  ncurses is already initialized before keyboard input is read. Two
  operations: `read_key()` (blocking read, translated into the `KeyCode`
  space) and `key_available()` (`kbhit()`-equivalent non-blocking poll).
- **Alt-key detection**: ncurses reports `Alt+<key>` as a bare `ESC`
  (27) followed by the key's own byte on the wire (this is how terminals
  actually send Meta/Alt — there is no separate "Alt" signal like the
  Win32 console API's `dwControlKeyState` the original relied on).
  `read_key()` uses the standard technique for this: on seeing `ESC`,
  switch to non-blocking (`nodelay`) and peek for an immediately-following
  byte. If one arrives, it's `Alt+<that key>`; if not (`ERR`), it was a
  genuine standalone `Escape` keypress. Verified directly in this
  sandboxed environment using ncurses' own `ungetch()` to inject
  `ESC`+`'a'` and confirm this resolves to `Alt+A`, and a lone `ESC` with
  nothing queued resolves to plain `Escape` — which is also how the
  automated tests for this exercise the real ncurses input path (`ungetch()`
  injecting sequences, not a mock).
- **`Key::Resize`**: not part of the original's key space at all (no BIOS
  scan code for "the window changed size" — DOS/OS2/Win32 consoles didn't
  need one the same way). ncurses reports terminal resizes as a
  `KEY_RESIZE` pseudo-key from `wgetch()`, and the idiomatic way most
  ncurses TUIs handle it is folding it into the same key-event stream
  rather than a separate signal — so `read_key()` surfaces it that way
  too, as `Key::Resize`.
- **`Key::Unknown`**: returned for any extended key ncurses reports that
  isn't in the mapped set (rather than silently dropping it or crashing),
  and for `ESC` immediately followed by another special (non-ASCII)
  key — a combination the original's model has no representation for
  either.

## What's deliberately narrowed or deferred

- **Modifier-combined extended keys beyond what's listed above** —
  `Shift+F1`-`F10` (`0xFF54`-`0xFF5D` in the original), `Ctrl+PgUp`/
  `Ctrl+PgDn` (`0xFF84`/`0xFF76`), and similar combinations that appear as
  scattered literals in `os.cpp` — are **not** predefined as named
  constants here. Unlike the base arrow/function/editing keys, ncurses'
  reporting of *modified* function/navigation keys is terminal-
  (terminfo-)dependent, not a universal scan code the way DOS BIOS gave
  every program for free. Getting these right requires testing against
  real terminals, which is better done against the concrete keybinding
  that needs it (subsystems 06/07) than guessed at now. `Key::Unknown`
  is the honest answer for these today; a later subsystem can extend
  `translate_curses_key()` (`platform/linux/Keyboard.cpp`) once a real
  terminal's actual reporting for a specific combination is confirmed.
- **`ungetch()`/pushback of a Listless `KeyCode`** — the original's
  `ungetch(int c)` pushes one raw platform character back for the next
  `getch()`. No concrete call site was found needing this at the
  `Keyboard` level (the original's own usage is internal to its
  `getch()`/extended-key-continuation implementation, not something the
  app calls directly). Not exposed; can be added if a real need appears.
