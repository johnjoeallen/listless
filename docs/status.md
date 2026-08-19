# Porting status

A snapshot of where the port currently stands. The full detail behind
every entry here — objectives, decisions, bugs fixed, tests, and
deviations from the original — lives in
[`PORTING_JOURNEY.md`](https://github.com/johnjoeallen/listless/blob/main/PORTING_JOURNEY.md),
the project's append-only engineering log. This page summarizes it; it
doesn't replace it.

## Landed

| # | Subsystem | Doc | PR |
|---|-----------|-----|----|
| 1 | String/container primitives | [01](01-string-container-primitives.md) | [#17](https://github.com/johnjoeallen/listless/pull/17) |
| 2 | Search primitives | [02](02-search-primitives.md) | [#18](https://github.com/johnjoeallen/listless/pull/18) |
| 3 | Directory enumeration | [03](03-directory-enumeration.md) | [#19](https://github.com/johnjoeallen/listless/pull/19) |
| 4 | Console/terminal I/O | [04](04-console-terminal-io.md) | [#20](https://github.com/johnjoeallen/listless/pull/20) |
| 5 | Keyboard input | [05](05-keyboard-input.md) | [#21](https://github.com/johnjoeallen/listless/pull/21) |
| 7 | File viewer core | [07](07-file-viewer-core.md) | [#22](https://github.com/johnjoeallen/listless/pull/22) |
| 6 | File-list UI (`FileManager`) | [06](06-file-list-ui.md) | [#23](https://github.com/johnjoeallen/listless/pull/23) |
| 8 | Style/config system | [08](08-style-config.md) | [#25](https://github.com/johnjoeallen/listless/pull/25) |
| 9 | Syntax highlighting | [09](09-syntax-highlighting.md) | [#26](https://github.com/johnjoeallen/listless/pull/26) |
| 10 | Hex-mode viewer | [10](10-hex-mode-viewer.md) | [#27](https://github.com/johnjoeallen/listless/pull/27) |

Subsystem 7 landed ahead of subsystem 6 in numbering order — the file
viewer core was sequenced before the file-list UI; see
`PORTING_JOURNEY.md` for the rationale.

## In flight

- **App entry point / main loop** ([issue #24](https://github.com/johnjoeallen/listless/issues/24))
  — wires `FileManager` and `Viewer` into the first runnable `lss`
  binary: a `FileManager` renderer, a minimal modal-prompt widget, pure
  key-dispatch tables for browsing/viewing, and a real executable
  target. Implemented in
  [PR #28](https://github.com/johnjoeallen/listless/pull/28), open at
  time of writing. Its subsystem doc will appear here as
  `app-main-loop.md` once merged.

## Not started

- **Syntax highlighting wiring** ([issue #29](https://github.com/johnjoeallen/listless/issues/29))
  — subsystem 9 produces colour spans (`highlight_line`); nothing calls
  it from the renderer yet, so no file gets syntax colouring in the
  running app regardless of type. Filed as a follow-up once the
  App/main-loop layer existed to own *which* style is active per file.
- **Subsystem 11: file operations** (copy/move/rename/delete/mkdir) —
  deferred until everything above is stable, given the higher
  regression-test scrutiny destructive operations warrant.
- **Subsystem 12: editor integration, shell escape, live style editing**
  — polish features, lowest risk if deferred; not started.

## What "done" means here

A subsystem is "landed" once its port compiles, its tests pass, its
documentation (this site's subsystem page plus the `PORTING_JOURNEY.md`
entry) is written, and it's merged to `main` — not necessarily once
every original behaviour is reproduced. Each subsystem page's own
"deliberately narrowed or deferred" section is the authoritative list
of exactly what that subsystem left out and why; this status page just
tracks which subsystems exist yet, not their internal completeness.
