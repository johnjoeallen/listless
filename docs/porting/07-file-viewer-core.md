# Subsystem 07: file viewer core

Source: `original/apps/onscreen/osview.cpp` (3107 lines — the largest
file in `/original`), `original/apps/onscreen/ostxt.hpp`'s `Viewer`/
`LinePtr`/`BookMark` declarations. This is the "less"-equivalent half of
Listless. Depends on subsystems 01, 02 (search), 04/05 (I/O). Explicitly
**excludes** hex mode (subsystem 10), syntax highlighting and the
`Style`/config system (subsystems 08/09), and editor invocation
(subsystem 12) — noted below wherever the original tangles them in.

## What the original does (summary; full derivation was a research pass
over the whole file, not repeated here)

- **Line model**: whole file loaded into one buffer; a two-pass scan
  finds line boundaries and — load-bearing for the *original's*
  implementation only — NUL-terminates each line **in place**, swapping
  bytes back and forth when switching to/from hex mode. `LinePtr` also
  carries per-line syntax-highlight state (`LS_INCOMMENT` etc. — out of
  scope) and "bold/underline via control-character toggle" state (a
  "text with layout" feature that affects plain rendering too, not pure
  syntax highlighting).
- **Scrolling**: `iTopLine` (vertical) + `iColumn` (horizontal, for long
  lines) are the whole viewport state. Line up/down do an incremental
  scroll (shift existing screen content, draw one new line) using the
  same primitive as subsystem 04's `Terminal::scroll_region`; page
  up/down/Home/End do a full redraw, clamped so the last page still
  fills the screen. Horizontal scroll moves in fixed 10-column
  increments, capped at a 1024-column line-buffer limit.
- **Word wrap**: optional, re-splits the whole line index at word
  boundaries when toggled; off by default.
- **Search**: three key bindings (case-sensitive, case-insensitive,
  repeat), sharing one line-by-line scan using the original's literal
  search primitive (`strsrch`/`SearchExpression` — replaced here by
  subsystem 02's `HorspoolSearcher`). "Repeat" first tries continuing on
  the same line past the current match before falling through to
  scanning subsequent/preceding lines. Regex search mode is out of
  scope (deferred per subsystem 02's docs — `std::regex` if/when a
  concrete need appears). Goto-line (`g`/`G`) is search-adjacent: no
  actual text search, just repositions the viewport and highlights the
  whole target line.
- **Bookmarks**: `iBookMark[10]`, per-`Viewer` (not global), each storing
  a `(line, column)` position. `Alt+1`-`Alt+9`/`Alt+0` toggle
  set/clear/reset a slot; `Alt+G` then a digit jumps to one.
- **Status line**: three alternate formats cycled via a *Style* field
  (`Style::iTopLineFormat`) — genuinely tangled with the config
  subsystem (08), which doesn't exist yet.
- **Multi-buffer**: `Viewer` has no knowledge of the shared
  `App::cViewedFiles` list it's one entry of — no interface changes
  needed here to support that (App-level concern, subsystem 06).
- **Rendering** (`displayTextLine`, the single largest function in the
  file): almost entirely syntax-highlighting/text-with-layout character
  classification, inseparable from the `Style` system at the
  per-character-attribute level. The only style-independent parts are:
  iterating characters, tab expansion, the selected-range highlight
  overlay, and writing the result via the platform text-output
  primitive.

## What's ported here

- **`Viewer`** (`src/Viewer.hpp`/`.cpp`) — pure logic and state, no
  ncurses dependency, fully unit-tested in `tests/core`:
  - Line model as `offset+length` spans into a loaded `std::string`
    buffer (`LineSpan`), **not** the original's in-place-NUL-termination
    trick — that trick exists purely to let text/hex mode share one
    `char*`-based buffer; spans don't need it, and hex mode (subsystem
    10) can index the same raw buffer independently when it lands.
    `is_binary()` is `true` if any NUL byte appears anywhere in the
    file (matching the original's *purpose* — flagging non-text
    content — without the original's line-splits-on-NUL side effect).
  - Word wrap: a simplified greedy word-wrap (break at the last space
    before the width limit, or hard-break if none) over the original
    (un-wrapped) line spans, kept separately so toggling wrap off
    restores exact original line boundaries without re-reading the
    file.
  - Viewport: `top_line()`/`column()`, `scroll_line_up/down`,
    `scroll_page_up/down`, `scroll_to_top`, `scroll_to_bottom`,
    `scroll_left/right` (10-column increments, 1024-column cap,
    matching the original), `reset_horizontal_scroll`. All return
    `bool` (whether the position actually changed), so a caller can
    show "already at top/bottom"-style messages as the original did.
    The original's scroll-region *rendering* optimization for
    single-line scrolling is not reproduced — this subsystem always
    redraws the full visible page; see "narrowed" below.
  - Search: `search_forward`/`search_backward`/`repeat_search`, built on
    subsystem 02's `HorspoolSearcher`. Because `HorspoolSearcher` only
    finds the leftmost match on a line (forward-only), "backward" here
    means visiting lines in decreasing order and taking each line's
    leftmost match — a faithful-enough reading of the original's
    behavior (which also reused its forward-only `strsrch` per line,
    varying only which lines it visited and in what order) given the
    ambiguity in the exact original algorithm. "Repeat, continuing on
    the same line" is implemented exactly for the forward case
    (`find(text, match_pos + 1)`); the backward case needs "the
    rightmost match strictly before the current one," which
    `HorspoolSearcher` doesn't expose directly, so it's built as a
    small loop finding successive matches up to that limit.
  - Goto-line, bookmarks: direct, self-contained ports — no `Style`
    dependency.
- **A minimal renderer** (`src/ViewerRender.hpp`/`.cpp`, tested against
  the real `Terminal` in `tests/platform_linux`) doing exactly the four
  style-independent things identified above: iterate visible lines, a
  fixed-width (8-column, see below) tab expansion, the selection-range
  highlight overlay, and writing through subsystem 04's `Terminal`. One
  status-line format (not the original's three — see "narrowed" below).

## What's deliberately narrowed or deferred

- **A single status-line format**, not the original's three
  `Style`-selected alternates. All three formats' *content* (line/column
  position, percentage, filename, date/time) is legitimately useful, but
  the format-cycling mechanism itself lives on `Style`, which doesn't
  exist yet — inventing a Style-shaped field now to support cycling
  between formats nothing yet configures would be building ahead of the
  subsystem that owns it. One clean format is implemented now
  (filename, line/total, percentage); the cycling mechanism is
  subsystem 08's to add once `Style` exists.
- **No incremental scroll-region rendering optimization.** The original
  shifts existing screen content and draws only the newly-exposed line
  for single-line up/down scrolling. This subsystem always redraws the
  full visible page on any scroll. It's a performance optimization, not
  a correctness concern, and `Terminal::scroll_region` (subsystem 04)
  remains available to add this later without any change to `Viewer`'s
  own interface.
- **Tab width is a fixed constant (8)**, not the original's
  `Style`-configured `iTabWidth` — no config system exists yet. The
  renderer's tab-expansion is written so wiring in a real configured
  width later (subsystem 08/09) is a one-line change, not a redesign.
- **No live file-change detection/reload** (`*` key, `fileHasChanged()`).
  The original re-checks the file's mtime and reloads on demand. Not
  implemented here — a file is loaded once at construction; refreshing
  is a small, self-contained addition for whenever a concrete need
  makes it worth the added state (tracking mtime, deciding when to
  reload mid-session).
- **No external-filter / `d`/`D` reload-through-a-command feature** — a
  `Style`-driven feature (subsystem 08/09), and the original's
  implementation reaches into the shared `App::cViewedFiles` list to
  re-dump sibling viewers sharing the same style, which is entirely
  App/multi-buffer territory (subsystem 06) layered on top of `Style`
  besides.
- **No external-filter reload path** — `App` now captures redirected stdin
  and constructs a viewer over it. The original's external-filter reload
  behaviour remains unimplemented.
- **`Ctrl+F`, `Alt+Z`, `Alt+E`, `h`/`H`, `d`/`D`, `w`/`W`'s storage
  location** (all `Style`-dependent one way or another per the survey)
  and hex mode entirely are not ported here — see the survey notes
  folded into the bullet points above for exactly where each seam is,
  so later subsystems land against a known integration point rather
  than rediscovering it.
- **One correction surfaced by this survey to subsystem 05's docs**:
  `docs/05-keyboard-input.md` claimed there's no `VKALT_0` in the
  original. There isn't a *named* `VKALT_0` constant, but `Alt+0` **is**
  used as this subsystem's bookmark-slot-0 toggle, with scan code
  `0x81` (129 decimal) — matching `kbdtab`'s `'0'` row's alt column
  (`EXT(129)`) exactly. `alt_key()` (subsystem 05) is extended in this
  PR to cover `'0'` alongside `'1'`-`'9'`.
