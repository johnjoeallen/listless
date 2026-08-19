# Subsystem 09: syntax highlighting

Source: `original/apps/onscreen/ostxt.hpp` (`LineStatus` enum,
`BOLD_CODE`/`UNDERLINE_CODE`), `original/apps/onscreen/osview.cpp`
(`IsString`/`IsEolComment`/`IsBeginComment`/`IsEndComment`/
`IsNumericPrefix`/`IsSymbol`/`keywordCmp`/`IsReservedWord`,
`findStyleForFile`, `Viewer::scanData`, `Viewer::displayData`). Layers
per-line syntax colouring onto the viewer core (subsystem 07) using the
style system (subsystem 08).

## What the original does

- **Per-character classification helpers** (`osview.cpp:34-215`) each take
  a style and answer "does the syntax rule for X match here": `IsString`
  (does `*sty->iString` — a set of quote characters — contain this char),
  `IsEolComment`/`IsBeginComment`/`IsEndComment` (does any entry of
  `iEolComment`/`iOpenComment`/`iCloseComment` match as a prefix at this
  position; `IsEolComment` respects `iCaseSensitive`, the comment
  delimiter matchers do not), `IsNumericPrefix` (any entry of
  `iNumericPrefix`, e.g. `"0x"`), `IsSymbol` (is this char in `iSymbols`),
  and `IsReservedWord`/`keywordCmp` (binary search over `iReserved`,
  sorted, comparing case-(in)sensitively per `iCaseSensitive` and
  requiring the character after the match not continue an identifier —
  `isalnum` or `_`).
- **`findStyleForFile`** (`osview.cpp:251-294`) — walks the global
  `styles` list, returning the first style whose `iExtensions` contains
  the file's extension (case-insensitive); falls back to `gDefaultStyle`
  if none match.
- **`LineStatus`** (`ostxt.hpp:298-306`) — one enum value stored per line
  (`LinePtr::iLineStatus`): `LS_NONE`, `LS_BOLD`, `LS_UNDERLINED`,
  `LS_BOLD_UNDERLINED`, `LS_INCOMMENT`, `LS_INPREPROCESSOR`. It's the
  state carried *into* the next line — a multi-line `/* */` comment sets
  the *next* line's status to `LS_INCOMMENT` (not the line the `/*`
  itself is on), so `Viewer::displayData` seeds `iInComment`/
  `iInPreprocessorStatement` for line N from `lp->iLineStatus` (the value
  computed while processing line N-1).
- **`BOLD_CODE`/`UNDERLINE_CODE`** (`ostxt.hpp:326-327`) — the control
  bytes `\x02`/`\x13` (`('B'-'A')+1`/`('S'-'A')+1`). A completely
  separate feature from syntax highlighting called "text with layout"
  (`iTextWithLayout`): a file can embed these bytes to toggle bold/
  underline on/off from that point forward, and the original explicitly
  disables this when syntax highlighting is active — "no support for text
  with layout when syntax highlighting has been enabled"
  (`osview.cpp:839-840`).
- **`Viewer::scanData`** (`osview.cpp:1386-1698`) — the line-indexing
  pass. While indexing, if `iSyntaxHighlightEnabled`, it runs a small
  state machine (`ST_NONE`/`ST_INSTRING`/`ST_INPREPROCESSOR`/
  `ST_INEOLCOMMENT`/`ST_INMULTICOMMENT`) purely to decide each line's
  *exit* `iLineStatus` — string state isn't tracked into the *next*
  line (only comments and preprocessor blocks are), and an
  end-of-line comment or a preprocessor block without a trailing
  `iLineContinuation` character resets to `ST_NONE` before the next
  line starts. A preprocessor block found via `iOpenPreprocessor` only
  triggers when it starts at column 0 of non-space content
  (`nonSpaceCount == 0`). Independently, this same pass tracks
  `BOLD_CODE`/`UNDERLINE_CODE` toggles to compute `LS_BOLD`/
  `LS_UNDERLINED`/`LS_BOLD_UNDERLINED` for lines not in a comment/
  preprocessor block (comment/preprocessor status always wins over
  bold/underline in the stored `iLineStatus`, even though the toggle
  bytes are still tracked underneath).
- **`Viewer::displayData`** (`osview.cpp:756-1060`) — the per-line render
  pass. Seeds `iInComment`/`iInPreprocessorStatement` from the *current*
  line's `iLineStatus` (set by the *previous* line's pass through this
  same function, or by `scanData` for the first render), then walks the
  line character by character with this precedence: already-in-comment
  continuation → end-of-line comment → begin-comment → already-in-
  preprocessor continuation (itself checking for an embedded comment
  start, which breaks out of preprocessor colouring) → quoted string →
  symbol → numeric-prefixed number → decimal number → reserved word →
  identifier (`isalpha`/`_`) → default colour. This chain **only runs
  when `iSyntaxHighlightEnabled` *and* `iReserved.Size() > 0`** — a style
  with syntax highlighting on but zero reserved words falls through to
  the `BOLD_CODE`/`UNDERLINE_CODE`-toggle-and-backspace-bolding branch
  instead, a real quirk of the original's `if`/`else if` chain, not
  independently documented behaviour elsewhere in the codebase.
  Reserved-word matches are recoloured in the *stored* casing from
  `iReserved` when `iCaseConvert` is set (case-canonicalizing keywords
  as displayed), not just coloured.
- **String/escape handling** (`osview.cpp:928-948`): a quoted-string scan
  that, after displaying a character, checks whether the *new* current
  character equals `iEscape` and if so unconditionally consumes it plus
  the character after — regardless of whether that character is the
  closing quote. This reads as a simplification/bug in the original
  itself (a true "backslash escapes the very next character, including
  an escaped quote" semantics would check the escape *before* advancing
  past it, not after), documented here rather than silently "fixed"
  since a byte-faithful reading of the original was the goal.

## What's ported here

- **`highlight_line(text, style, state)`** (`src/syntax_highlight.hpp`/
  `.cpp`) — one function combining `scanData`'s state-machine (which
  `LineStatus`-equivalent state carries into the next line) and
  `displayData`'s per-character colouring chain into a single pass,
  since both walk the same rules over the same text; the original only
  splits them because `scanData` runs once up front (needing just the
  exit state) while `displayData` re-derives entry state per render.
  Returns a `std::vector<ColorSpan>` (offset/length/colour/bold/
  underlined runs covering the line end to end, adjacent same-attribute
  spans merged) and mutates a `HighlightState` in place (`in_comment`,
  `in_preprocessor`, `bold`, `underlined` — the original's single
  `LineStatus` enum split into independent fields, since a line can
  genuinely be bold *and* underlined at once, a combination the enum
  only expresses when neither comment nor preprocessor state also
  holds).
- **The classification/matching logic** — `match_any`/`match_prefix`
  (comment/preprocessor delimiter and numeric-prefix matching),
  `match_reserved` (word-boundary-aware reserved-word lookup), and
  `char_in_set` (symbol/string-delimiter membership) are direct,
  unexported (anonymous-namespace) equivalents of `IsEolComment`/
  `IsBeginComment`/`IsEndComment`/`IsNumericPrefix`/`IsReservedWord`/
  `keywordCmp`/`IsSymbol`/`IsString`.
- **`syntax_highlight_enabled && !reserved.empty()` gate** — ported
  faithfully as the switch between the syntax-colouring pass and the
  `BOLD_CODE`/`UNDERLINE_CODE`-toggle pass, matching the original's own
  `if`/`else if` structure rather than treating the empty-reserved-list
  case as a bug to fix.
- **`BOLD_CODE`/`UNDERLINE_CODE` toggling** — ported as the fallback
  path (`highlight_line` internally: `highlight_layout`), gated on
  `text_with_layout` exactly as `WithLayout(iCurrStyle)` gates it in the
  original; toggle bytes are consumed (not emitted as a visible span)
  and only flip `HighlightState::bold`/`underlined`.
- **Per-extension style selection** — already covered by subsystem 08's
  `StyleSet::style_for_extension()` (a direct port of
  `findStyleForFile`'s matching logic); this subsystem adds no new code
  for it, just uses it in tests.

## What's deliberately narrowed or deferred

- **Comment delimiters always matched case-sensitively** — the original
  applies `iCaseSensitive` to `IsEolComment` but not to
  `IsBeginComment`/`IsEndComment` (an inconsistency in the original
  itself, `osview.cpp:73-112` use plain `strncmp` unconditionally); this
  port matches `IsBeginComment`/`IsEndComment`'s always-case-sensitive
  behaviour for open/close comments too, and applies `case_sensitive`
  only where the original does (reserved words).
- **Escape handling in strings is a simplified, standard reading**
  (backslash-escapes-the-following-character), not the original's exact
  "consume 2 chars unconditionally after redisplaying the escape-matched
  char" sequence documented above — a faithful-enough reinterpretation
  in the spirit of the `Viewer` search-backward decision in
  docs/07-file-viewer-core.md, not a byte-for-byte port of what reads
  like an original bug.
- **No case-conversion of displayed reserved-word text** — `ColorSpan`
  only carries an offset/length into the caller's original text, not
  replacement text, so `iCaseConvert`'s "redisplay the keyword in its
  stored casing" is not reproduced; reserved words are always coloured
  in the casing they appear in the source. Revisiting this would need
  `ColorSpan` (or a caller) to carry replacement text, not just a
  colour — deferred until a real renderer (issue #24) needs it.
- **Preprocessor state tracking is gated the same as colouring**
  (`syntax_highlight_enabled && !reserved.empty()`), whereas the
  original's `scanData` state machine only checks
  `iSyntaxHighlightEnabled` (not `iReserved.Size() > 0`) — so a style
  with syntax highlighting on, zero reserved words, but real comment/
  preprocessor delimiters would track cross-line comment state in the
  original but not here. Judged not worth reproducing: a style enabling
  syntax highlighting with no reserved words is a degenerate
  configuration the original itself barely supports (falls through to
  the bold/underline-toggle path for actual colouring either way).
- **No backspace-based manual bolding** (`osview.cpp:1024-1041`, the
  `char\bchar` overstrike convention for bolding/underlining a
  character without a control byte) — a `WithLayout`-gated feature
  independent of `BOLD_CODE`/`UNDERLINE_CODE`, not implemented; no test
  or call site in this port exercises it.
- **No rendering** — `highlight_line` produces colour/attribute spans
  only; painting them to a terminal is `viewer_render.cpp`'s job
  (subsystem 07), not wired up here since that would need the
  `App`/main-loop layer (issue #24) to own *which* `Style` is active for
  the file being viewed and feed `highlight_line`'s output into the
  renderer per visible line.
