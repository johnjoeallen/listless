# Documentation index

This directory documents the *original* OnScreen/2 behaviour, read from
`/original`, before each subsystem is ported to Listless. Pages here are
written before the corresponding port begins, and are not modified to
describe the new implementation — new behaviour and deviations belong in
`PORTING_JOURNEY.md` and in the new code's own comments/tests.

- [architecture.md](architecture.md) — whole-program architecture: main
  loop, major classes, global state, platform seams, and the proposed
  subsystem breakdown for porting.
- [01-string-container-primitives.md](01-string-container-primitives.md) —
  the custom pre-STL container/utility library (`CString`, `Array`,
  `SList`/`DList`/`Set`, `RefPtr`, `Date`/`Time`, etc).
- [02-search-primitives.md](02-search-primitives.md) — literal search
  (Boyer-Moore-Horspool) and the documented wildcard/glob syntax.
- [03-directory-enumeration.md](03-directory-enumeration.md) — directory
  listing, `Directory`/`Dirent`, and path/filespec splitting.
- [04-console-terminal-io.md](04-console-terminal-io.md) — screen buffer
  I/O, colours, and the ncurses backend decision.
- [05-keyboard-input.md](05-keyboard-input.md) — the original's `0xFFxx`
  extended-keycode model and its ncurses-backed translation.

Subsystem docs are added in porting order; see `architecture.md` for the
full ordered list and the rationale behind it.
