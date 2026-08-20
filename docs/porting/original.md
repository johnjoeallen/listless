# The original: OnScreen/2

Listless is an AI-assisted, from-scratch modern port of **OnScreen/2**,
created by its original author using the original codebase as a reference.
OnScreen/2 is a directory and text-file viewer written by John J. Allen and
released for IBM OS/2, Microsoft Windows NT/95, and PC-DOS through the 1990s
and into the 2000s (release 2.50's `readme.txt` covers OS/2 2.x/Warp 3.0/4.0
and Win32; DOS support had already been dropped by that release). It was
distributed as shareware of its era: zipped per-platform archives,
registration forms, a BBS description file, and a manual in three formats
(Word `.doc`, plain-text `.man`, and OS/2 `.inf` help).

The full original source lives in this repository under `original/`
(`original/apps/onscreen` for the application, `original/class` and
`original/func` for its supporting utility library, `original/include`
for headers) — copyright 1993-2006 John J. Allen, licensed GPLv2 or
later. Nothing here is a clean-room reimplementation guessing at
behaviour from the outside: every subsystem doc in this site is
written by reading that source directly, with `file:line` citations
into it.

## What it did

OnScreen/2 is two halves sharing one process:

- **A file manager** (`FileManager`, `apps/onscreen/fileman.cpp`,
  2,484 lines) — a multi-column, sortable directory listing (by name,
  extension, date, or size), glob-pattern multi-select, a DOS/OS2/Win32
  drive-letter bar, and direct file operations (copy, delete, rename,
  move, make-directory) driven by a `:`/`/`-triggered command submenu.
- **A file viewer** (`Viewer`, `apps/onscreen/osview.cpp`, 3,107
  lines) — text-mode display with word wrap, scrolling, bookmarks, and
  literal/regex search; a 16-bytes-per-row hex-dump mode; and
  configurable per-file-type syntax highlighting (reserved words,
  comments, strings, preprocessor blocks) via a `Style` system with
  prototype-style inheritance between named styles, loaded from an
  `os.set` config file.

Both are driven from one top-level `App` object (`apps/onscreen/os.cpp`)
whose `Run()` method is a single large keyboard-driven event loop:
digit keys and Alt+N/P switch between multiple open file buffers, F2-F7
cycle colours live, and everything not handled at that level falls
through to the viewer's own key dispatch. See
[Original architecture](architecture.md) for the full map — top-level
program structure, global state, and a subsystem-by-subsystem
breakdown of what each part does — and the per-subsystem pages linked
from there for the fine detail behind each piece.

## Why a port, and why "Listless"

The original is written in pre-STL, cross-platform C++ against a
custom container/utility library (`CString`, `Array`/`SortableArray`,
`SList`/`DList`/`Set`, hand-rolled `Date`/`Time`) and DOS/OS2/Win32
console APIs, with scattered `#ifdef __OS2__`/`__WIN32__`/`__DPMI32__`
branches throughout. It doesn't build or run as-is on a modern Linux
system. Listless keeps OnScreen/2's behaviour and feel as the reference
point — the goal is a faithful port, not a reinvention — while
rebuilding it on modern standard C++, with the platform-specific
seams collected into a small, explicit interface layer instead of
scattered throughout the application.

The name is a small Unix-style joke: `ls` lists files, `less` views
files, **Listless** does both. See [The target: Listless](target.md)
for the full set of goals and engineering principles guiding the port.
