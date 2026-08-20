# Listless

**A portable file and directory viewer for Linux, Windows, and macOS.**

Listless is the modern continuation of the historic **OnScreen/2**, a
DOS/OS2/Win32 directory and file viewer first released in the 1990s.
Its primary purpose is to view files. When started without a file
argument or redirected input, it acts as a directory viewer, allowing
files to be selected and opened interactively.

The command name is `lss` — a small Unix-style joke: `ls` lists files,
`less` views files, **Listless** does both.

!!! note "Project status"
    Listless is in active, incremental development. There is not yet a
    finished release. See [Porting status](status.md) for exactly what
    has landed and what's next.

## Where to start

- **[The original: OnScreen/2](original.md)** — what OnScreen/2 was,
  who wrote it, and why Listless exists as its continuation.
- **[The target: Listless](target.md)** — what Listless is trying to
  become, and the engineering principles guiding how it gets there.
- **[Porting status](status.md)** — what's landed, what's in flight,
  and what's next: the current state of the port.
- **[Original architecture](architecture.md)** — a whole-program map
  of OnScreen/2 as found in `/original`: main loop, major classes,
  global state, and the subsystem breakdown Listless's port follows.

## How the docs are organized

This site mirrors the repository's `docs/` directory almost exactly.
Each **subsystem** page documents the *original* OnScreen/2 behaviour
for one piece of the system — written before that piece was ported,
grounded in `original/`-relative file:line citations — while
[`PORTING_JOURNEY.md`](https://github.com/johnjoeallen/listless/blob/main/PORTING_JOURNEY.md)
in the repository root is the append-only engineering log of what
actually happened at each step: objectives, decisions, bugs fixed,
tests, and deviations from the original. The [Porting status](status.md)
page here is a snapshot summary of that log, not a replacement for it.
