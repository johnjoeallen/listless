# Subsystem 10: hex-mode viewer

Source: `original/apps/onscreen/ostxt.hpp` (`BYTES_PER_LINE_HEX_MODE`,
`handleKeyInHexMode`), `original/apps/onscreen/osview.cpp`
(`hexLineCount`, `displayHexLine`, `displayDataAsHex`,
`calcNearestHexTopLine`, `switchToHexMode`, `switchToTextMode`,
`handleKeyInHexMode`). Layers a second display mode onto the viewer core
(subsystem 07): the same loaded file, shown as a 16-bytes-per-row hex
dump instead of text lines.

## What the original does

- **`BYTES_PER_LINE_HEX_MODE`** (`ostxt.hpp:329`) is a fixed `16`.
  **`hexLineCount`** (`osview.cpp:404-410`) is `iFileInfo->iFileSize /
  16`, rounded up when the size isn't a multiple of 16.
- **`switchToHexMode`** (`osview.cpp:2599-2617`) calls
  `calcNearestHexTopLine(iTopLine)` to position the hex viewport, then
  sets `gDisplayMode`/`iCurrStyle->iDisplayMode` to `DM_HEX` and clears
  the screen body. **`calcNearestHexTopLine`** (`osview.cpp:2587-2593`)
  takes the current text top line's `LinePtr::iText` byte offset,
  divides by 16, and rounds *up* (`iHexTopLine++`) if that offset isn't
  itself a multiple of 16 — so the hex viewport starts at the first row
  that doesn't precede the text line's start.
- **`switchToTextMode`** (`osview.cpp:2623-2646`) does the reverse: takes
  the hex viewport's byte offset (`iHexTopLine * 16`), scans
  `iLinePtr[]` for the first line whose start is *past* that offset, and
  backs up one (`i-1`, floored at 0). If no line starts past that offset
  (viewport at/after the last line), the scan's `for` loop simply
  finishes without ever assigning `iTopLine` a second time, so it keeps
  the `iTopLine = 0` the function set before the loop.
- **`displayDataAsHex`/`displayHexLine`** (`osview.cpp:1099-1160`,
  `1256-1274`) render rows `iHexTopLine..hexLineCount()` (inclusive —
  one past the last real row) into the screen body. A row past
  `hexLineCount()` is blank, except under `#if
  defined(DISPLAY_END_OF_FILE)` (not defined in the shipped build) where
  the exact `hexLineCount()` row prints a `"  END OF FILE  "` marker
  instead of hex bytes. Each real row is `"%08X <corrupted-byte> "`
  (an offset in hex, then a stray non-ASCII separator byte that reads as
  a mojibake artifact of the original's codepage rather than an
  intentional glyph) followed by 16 bytes grouped in fours (a literal
  space inserted every 4th byte), each byte as two uppercase hex digits
  plus a trailing space, with bytes inside the current selection
  (`iSelectedOffset`/`iSelectedCount`) drawn in `iSelectedForeGndColor`/
  `iSelectedBackGndColor` instead of the normal colours. There is no
  ASCII gutter column in the original's hex view at all — just the
  offset and the hex bytes.
- **`handleKeyInHexMode`** (`osview.cpp:1983-2100+`) implements
  scrolling and `g`/`G` "goto offset": home (`iHexTopLine = 0`), end
  (clamped to `hexLineCount() - (screenheight-2)`, floored at 0), page
  down/up (±`screenheight-2`, clamped the same way), and line-at-a-time
  variants further down in the same function (not excerpted above). Each
  scroll re-renders only if the position actually changed, otherwise
  shows an error message ("Begining of file" / "End of file") — the
  same "did the position change" contract subsystem 07's text-mode
  `scroll_*()` methods already use. `g`/`G` prompts for a hex offset via
  `LineEdit`, parses it with `sscanf("%lx")`, validates
  `0 <= offset < iFileInfo->iFileSize`, rounds down to the nearest
  16-byte boundary, and jumps there.

## What's ported here

- **`Viewer::kBytesPerHexLine = 16`**, **`hex_line_count()`** — direct
  equivalents of `BYTES_PER_LINE_HEX_MODE`/`hexLineCount`, using
  `data_.size()` in place of `iFileInfo->iFileSize`.
- **`switch_to_hex_mode`/`switch_to_text_mode`** — direct ports of
  `calcNearestHexTopLine`+mode-set and the text-mode scan-back, using
  `lines_[i].offset` in place of `LinePtr::iText - iData`. The
  scan-back's "falls through to 0 if no line starts past the offset"
  behaviour is preserved rather than clamped to the last line (see
  `Viewer::switch_to_text_mode`'s comment) — it's a faithful port of the
  original's uninitialized-loop fallthrough, not a considered design
  choice, so it's called out at the call site rather than silently
  fixed.
- **`hex_scroll_to_top/bottom`, `hex_scroll_page_up/down`,
  `hex_scroll_line_up/down`** — direct equivalents of
  `handleKeyInHexMode`'s home/end/page/line branches, using the same
  "return true only if the position changed" contract as subsystem 07's
  `scroll_*()` (replacing the original's `displayErrMsg`-on-no-change
  with a bool the caller can act on however it likes, same as the
  text-mode methods already do).
- **`hex_goto_offset`** — a direct port of the `g`/`G` handler's offset
  math (clamp into range, round down to a 16-byte boundary), minus the
  `LineEdit` prompt and `sscanf` parsing, which are input/UI concerns
  for the `App`/main-loop layer (issue #24), not this subsystem.
- **`hex_line_bytes(line)`** — returns the raw bytes for one hex row
  (fewer than 16 for a partial final row) as a `std::string_view` into
  `data_`, the equivalent of `displayHexLine`'s `pData`/`pEnd` walk,
  minus the rendering.
- **`format_hex_line`/`draw_hex_line`** (`ViewerRender.cpp`) — a
  standard hex-dump line: 8-hex-digit offset, 16 bytes in groups of 4,
  each as two uppercase hex digits, plus an ASCII gutter (printable
  bytes 0x20-0x7E as themselves, everything else as `.`). `render_viewer`
  dispatches on `Viewer::display_mode()` to this or the existing
  text-mode row renderer.

## What's deliberately narrowed or deferred

- **No corrupted separator byte** — the original's `"%08X <byte>"` gap
  between offset and hex bytes is reproduced here as a plain two-space
  gap; the original byte reads as a mojibake artifact of its source
  encoding, not a considered design choice worth preserving byte-for-
  byte.
- **ASCII gutter added, not in the original** — the original's hex view
  has no character-preview column at all. This port adds one (a
  conventional hex-dump feature) since there's no faithfulness reason to
  omit it and every other hex viewer has one; flagged here as a genuine
  addition, not a port.
- **No `END OF FILE` marker row** — gated behind `DISPLAY_END_OF_FILE`,
  undefined in the original's shipped build, so there's no observable
  behaviour to port. `hex_line_count()`/rendering here simply stop at
  the last real row.
- **No selection highlighting in hex mode** — `iSelectedOffset`/
  `iSelectedCount`-driven colour changes on selected bytes are not
  reproduced; hex mode has no notion of an active selection yet in this
  port (subsystem 07's `Selection` is text-line-oriented). Would need a
  byte-range selection concept shared between modes, deferred until a
  real use case (copy/search-in-hex) needs it.
- **No `g`/`G` goto-offset input handling, no key dispatch at all** —
  `hex_goto_offset` takes an already-parsed `std::size_t`; the prompt,
  hex-string parsing, and key routing (`switchToHexMode`/
  `switchToTextMode`/`handleKeyInHexMode`'s key switch in
  `osview.cpp:3083-3103`) are `App`/main-loop concerns (issue #24), same
  as every other interactive-input deferral noted in
  docs/07-file-viewer-core.md and docs/09-syntax-highlighting.md.
- **No `replaceNULLS`/`insertNULLS`** — the original swaps embedded NUL
  bytes for a placeholder glyph while in text mode and restores them
  when entering hex mode (so text-mode string handling doesn't choke on
  NULs while hex mode still shows the real byte). This port's line model
  (subsystem 07) already handles embedded NULs as ordinary bytes via
  `std::string`/`string_view`, so there's nothing to swap.
