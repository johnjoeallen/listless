# The target: Listless

**A portable file and directory viewer for Linux, Windows, and macOS.**

Listless's primary purpose is to view files. When started without a
file argument or redirected input, it acts as a directory viewer,
allowing files to be selected and opened interactively — the same
two-halves-in-one-process shape as [the original](original.md), kept
deliberately, with the platform-specific plumbing rebuilt underneath.

The command name is `lss`.

## Goals

The aim is to preserve the behaviour and feel of OnScreen/2 while
building a cleaner, safer, portable implementation. Key principles:

- Linux first, with Windows and macOS as permanent targets
- standard C++ wherever practical
- RAII and clear ownership
- STL containers and standard concurrency primitives
- thin platform-specific interfaces
- no scattered platform `#ifdef` logic
- preserve behaviour, but fix genuine bugs
- regression tests for bug fixes
- issue-driven, incremental development
- build, test, document, commit, and push each clean iteration

The guiding rule is:

> **Every iteration should leave the project easier to understand than
> it was before.**

## How the work is organized

Work begins with understanding and documenting the existing OnScreen/2
implementation before making substantial changes — that's what every
subsystem page under [Original architecture](architecture.md) is:
written by reading `original/` directly, *before* the corresponding
port begins.

Development proceeds subsystem by subsystem, using GitHub issues for
traceability. Each iteration compiles cleanly, passes tests and quality
gates, updates documentation where needed, appends an entry to
[`PORTING_JOURNEY.md`](https://github.com/johnjoeallen/listless/blob/main/PORTING_JOURNEY.md)
(the project's append-only engineering log — created once, never edited
retroactively), and is committed and pushed independently, typically as
its own branch and pull request.

See [Porting status](status.md) for where that process currently
stands.
