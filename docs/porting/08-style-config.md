# Subsystem 08: style/config system

Source: `original/apps/onscreen/os.hpp` (337 lines — `Item<T>`, `Style`,
`ReservedWord`), `original/apps/onscreen/osstyle.cpp` (1717 lines —
config file parsing/writing, `Style` methods). Needed before syntax
highlighting (subsystem 09); otherwise decoupled from the viewer core
(subsystem 07). Depends on subsystem 01 (string/container primitives)
and reuses subsystem 04's `Color` enum (`src/Color.hpp`) for the palette.

## What the original does

- **`Item<T>`** (`os.hpp:80-244`) — a template wrapping a lazily-allocated
  `T*` plus a `Set<Item<T>>` of "base items". `SetItem(T)` allocates and
  assigns the owned value; `GetUnresolved()` returns the owned value only
  (or null); `GetItem()` returns the owned value if set, otherwise walks
  `iBaseItems` (a `Set`, so insertion order = discovery order) calling
  `GetItem()` on each recursively and returning the first non-null
  result. `AddBaseItem(Item<T>* t)` inserts `t` **before** the current
  head of `iBaseItems` if the set is non-empty, otherwise adds it as the
  only element — so of several base items, the *most recently added* one
  is consulted first. This is the whole inheritance mechanism: no field
  is ever copied between styles, only pointer-linked, so changing a base
  style's field retroactively changes every derived style that hasn't
  overridden it.
- **`Style`** (`os.hpp:273-335`) — one named style (`iStyleName`) with a
  `Set<CString> iBaseStyles` (names, for display/config-writing) plus
  ~34 `Item<T>`-wrapped fields: display colours (`BYTE`, an index into
  a 16-entry DOS palette), tab/wrap/filter booleans, `iTabWidth`,
  `iDisplayMode` (text/hex), `iExternalFilterCmd`/`iEditor`, and the
  syntax-highlighting fields (`iReserved` — a flat, directly-copied-not-
  linked `SortableArray<ReservedWord>`, `iOpenComment`/`iCloseComment`/
  `iEolComment`/`iNumericPrefix` — `Item<Set<CString>>`, `iSymbols`,
  `iString`, `iEscape`, `iCaseSensitive`, `iCaseConvert`,
  `iOpenPreprocessor`/`iClosePreprocessor`, `iCommentColumn`,
  `iLineContinuation`). `AddBaseStyle(Style*)` (`osstyle.cpp:1661-1717`)
  links every field via `AddBaseItem` **except** `iExtensions` (a style's
  file-extension list never inherits — a `.pas` style built on `Default`
  doesn't pull in `Default`'s extensions), and copies the base's
  `iReserved` words in directly with `iInherited = TRUE` (reserved-word
  lookup wants one flat, fast-to-scan array per style, not a chain of
  indirections, so this one field is a real copy, not a link — the
  `iInherited` flag exists purely so `saveConfig` knows not to re-write
  words that came from a base style).
- **Config file format** (`osstyle.cpp:383-1550`) — a hand-rolled,
  brace-delimited text format, *not* INI or any standard syntax. Comments
  are full lines starting with `;`. A hand-rolled tokenizer (`getSymbol`)
  reads either a bare word (alpha- or `.`-led, stopping at whitespace or
  ``=<>(){}.``), one of `(` `)` `*` `{` `}`, or the two-character token
  `=>` — except immediately after `=>`, where `getSymbol(f, 1)` instead
  returns the **entire rest of the current source line**, letting values
  like `ExternalFilter`'s shell command contain spaces. Grammar (informal):
  ```
  Settings
  {
      Key => Value        ; global App-level settings — see "Deferred" below
      ...
  }

  Style Name (.ext1 .ext2)  BaseStyle1 BaseStyle2
  {
      Key => Value
             => Value2     ; repeating "=>" continues the same key
      ...
  }
  ```
  The `Default` style is special: its extension-list parens contain a
  literal `*` instead of a real extension list (`if
  (strcmpi(iStyleName,"Default")==0) fprintf(f,"*")` on save; on load,
  `*` is accepted as a pseudo-extension token like any `.ext`). Loading
  reuses an existing style if the name (case-insensitive) already exists
  — including `Default`, which always exists via the global
  `gDefaultStyle` and is never newly-allocated. Saving backs up the
  previous file to `<name>.bak` first, then writes every style's
  `GetUnresolved()` (i.e. *own*, non-inherited) fields only — inherited
  values are implied by the style's base-style list on the next load,
  not re-written. Malformed input calls `exit(3)` immediately (parsing
  a config error is fatal to the whole program).
- **Live colour cycling** — `F2`-`F7` cycle individual colour fields
  (fore/back/selected-fore/selected-back/etc.) through the 16-colour
  palette while viewing, and `Ctrl+S` persists the current in-memory
  style set back to the config file. Both are keyboard-loop features in
  `osview.cpp`/`fileman.cpp`'s `Activate()`, not part of `Style` itself
  — `Style` only needs to expose "set this field to the next colour" and
  "save now", which the keyboard loop calls into.
- **Global `Settings` block** — separate from `Style` entirely: FileManager
  UI colours (disk-bar/status-line colours) and app-wide toggles (sound,
  keep-files-loaded, regex-vs-plain search default), stored in a global
  `setupInfo` struct, not on any `Style`. Out of scope for this subsystem
  (see "Deferred" below).

## What's ported here

- **`Item<T>`** (`src/Style.hpp`) — a direct port of the lazy-fallback
  mechanism using `std::optional<T>` instead of a manually
  `new`/`delete`d `T*`, and `std::vector<Item<T>*>` instead of
  `Set<Item<T>>` (a style is only ever linked as a base once in practice;
  a `Set`'s dedup semantics aren't load-bearing here). `add_base_item()`
  prepends, exactly matching the original's "most recently added base
  wins ties" resolution order (`Item.MostRecentlyAddedBaseWinsTies` in
  `tests/core/style_test.cpp` pins this down).
- **`Style`** (`src/Style.hpp`/`.cpp`) — the same ~34 fields, typed with
  `Color` (subsystem 04's enum, not a raw `BYTE`), `bool`, `int`,
  `std::string`, `char`, and `std::vector<std::string>` (in place of
  `Set<CString>` for extension/comment/numeric-prefix lists — order-
  preserving is what every call site actually needs; true set semantics
  weren't load-bearing). `add_base_style()` is a direct port of
  `AddBaseStyle`: links every field except `extensions`, copies the
  base's `reserved` words in as `inherited = true`.
- **Config load/save** (`src/Style.cpp`'s `load_config()`/`save_config()`)
  — a from-scratch recursive-descent-ish parser/writer producing the
  *same brace-delimited shape* the original used (same section-key
  spellings, `=>`, multi-line continuation, `Style Name (ext...)
  BaseStyle...`), so a config file remains readable to anyone who knows
  the original format, but **not** byte-identical to `saveConfig`'s
  output (no `.bak`-file rotation, no tab-aligned columns, no header
  comment block). One real behavioural deviation: **malformed input is
  skipped, not fatal** — an unrecognised key or an invalid value for a
  known key (e.g. `ForeGndColor => Fuchsia`) is silently ignored rather
  than calling `exit(3)`, so one bad line in a hand-edited config doesn't
  take down the whole program. `docs/03-directory-enumeration.md`-style
  "narrowed" framing applies here too: see `LoadConfig.InvalidValueIsSkippedNotFatal`
  in `tests/core/style_test.cpp`.
- **`StyleSet`** — new (no direct original equivalent; the original used
  a bare global `SList<Style> styles` plus a separate `gDefaultStyle*`).
  Always holds a `Default` style from construction, and owns every style
  behind `std::unique_ptr<Style>` — required because `Item<T>::
  add_base_item()` stores raw pointers into another style's members, so
  a `Style` must never move once linked; `StyleSet` is the seam that
  guarantees that.
- **`cycle_color(Color)`** — the underlying "next colour in the palette,
  wrapping White back to Black" primitive `F2`-`F7` would call. Exposed
  as a free function rather than a `Style` method since it operates on
  whichever single `Item<Color>` field the keyboard loop is currently
  pointed at; the loop calls `field.set(cycle_color(*field.get()))`.
## Directory-based styles (issue #32)

Rather than one config file, `App` assembles its `StyleSet` from two
directories, each loaded via `load_config_dir()` in
increasing precedence (a later source overrides or extends an earlier
one by reusing a style's name):

1. **`system_styles_dir()`** — the package-installed, read-only styles
   directory. Baked in at build time from `CMAKE_INSTALL_PREFIX` via the
   `LISTLESS_SYSTEM_STYLES_DIR` compile definition (`src/CMakeLists.txt`);
   defaults to `/usr/local/share/listless/styles` if that definition is
   absent. Populated from this repo's `style/syntax/*.conf` by the root
   `CMakeLists.txt`'s `install(DIRECTORY style/syntax/ ...)` rule --
   `cmake --install` (and eventually a `.deb`) drops each file there
   individually, rather than one shared file competing with a user's own
   edits. Ships styles for C/C++ (`cpp.conf`), Python (`python.conf`),
   Shell (`shell.conf`), Java (`java.conf`), Pascal (`pascal.conf`),
   Perl (`perl.conf`), PHP (`php.conf`), classic BASIC (`basic.conf`),
   and BASCAL (`bcl.conf` -- see
   <https://johnjoeallen.github.io/bascal/manual.html>), plus the shared
   `common.conf` base they all inherit from.
2. **`default_styles_dir()`** — `$XDG_CONFIG_HOME/listless/styles/syntax/`
   (falling back to `~/.config/listless/styles/syntax/`), a directory of the
   user's own `*.conf` files. Highest precedence, since it's the newest
   and most specific source.

The former `$XDG_CONFIG_HOME/listless/style.conf` path is no longer loaded.
Move its `Style` blocks into one or more files in `default_styles_dir()` to
migrate an existing configuration.

### Contextual syntax rules

Styles can add structural highlighting without a language-specific lexer:

- `BeforeDelimiter` highlights text before a delimiter; use
  `BeforeDelimiterRequiresSpace => On` when the delimiter must be followed
  by whitespace or end-of-line to qualify.
- `LineStartPrefix` highlights a marker after indentation; use
  `LineStartPrefixRequiresSpace => On` for markers such as YAML's `-`.
  `LineStartDataColor` colours scalar data that follows a qualifying marker.
- `PrefixToken` highlights a token beginning with one of its configured
  characters, such as YAML anchors, aliases, tags, and directives.
- `BlockTextStart` starts an indented plain-text block when one of its
  markers is used as a mapping or sequence value. Optional numeric markers
  such as `|2` establish the minimum content indentation.

`BeforeDelimiterColor`, `LineStartPrefixColor`, and `PrefixTokenColor`
default to `ReservedColor`; `BlockTextColor` and `LineStartDataColor`
default to `StringColor`.

`load_config_dir(StyleSet&, dir)` loads every `*.conf` file directly
inside `dir` (not recursive) in filename order, but **not** via a naive
single pass per file -- it pre-registers every style name defined
anywhere in `dir` first (mirroring `load_config()`'s own token
traversal, so a field value that happens to contain the word "Style"
can't be misread as a new style header), *then* parses each file's
fields and `BaseStyle` links. That means a style in one file can serve
as a `BaseStyle` for a style in another file regardless of which file
happens to load first -- see this repo's `style/syntax/common.conf`
(a shared base with no extensions of its own) and `cpp.conf`/
`python.conf`/`shell.conf` (each declaring `Common` as a `BaseStyle`,
despite `common.conf` sorting after all three alphabetically), and
`LoadConfigDir.BaseStyleInAnotherFileResolvesRegardlessOfLoadOrder` in
`tests/core/style_test.cpp`, which pins this down by disabling the
pre-registration pass and confirming the test then fails.

## What's deliberately narrowed or deferred

- **The `Settings` block** (FileManager UI colours, sound, search-mode
  default) is not implemented — it's an `App`-level config concern, not
  `Style`'s, and there's no `App`/main-loop subsystem yet to own it (see
  issue #24). `load_config()`/`save_config()` only handle `Style Name
  (...) ... { ... }` blocks; a `Settings { ... }` block in a hand-edited
  file is silently skipped (the `Style` keyword check simply doesn't
  match it) rather than parsed.
- **`F2`-`F7` colour-cycling and `Ctrl+S` keybindings** are not wired up
  — that requires an interactive keyboard loop, which is `App`/main-loop
  territory (issue #24), not this subsystem's. `cycle_color()` is the
  primitive that keybinding would call; wiring it to an actual key is a
  small, self-contained addition once that loop exists.
- **No `.bak` rotation on save** — the original renames the previous
  config to `<name>.bak` before writing. Not reproduced; `save_config()`
  overwrites in place. Losing a config to a bad write is a low-
  consequence, easily-regenerated failure (the file is derived state,
  not user data), so the extra complexity isn't justified yet.
- **No header comment block or tab-aligned columns on save** — cosmetic;
  `save_config()` writes one short header comment and simple `\tKey =>
  Value\n` lines instead of the original's hand-tuned per-key tab counts.
- **`iFilterEnabled` and `iSyntaxHighlightEnabled`** exist as `Style`
  fields (ported, since `AddBaseStyle` links them) but have **no config
  file key** in the original either — `styleSectionTable` never mentions
  them, so they're presumably set programmatically elsewhere in
  `osview.cpp`. Ported as plain fields; not wired into `load_config()`/
  `save_config()`'s field table, matching the original's own omission.
- **`AddBaseStyle`'s multi-match loop**: the original iterates *every*
  registered style checking for a name match when resolving a base-style
  token, so in principle two identically-named styles would both get
  linked. `StyleSet::find()` returns the first case-insensitive match
  only, assuming names are unique in practice (nothing in the original
  enforces uniqueness either, but nothing exercises the duplicate case).
