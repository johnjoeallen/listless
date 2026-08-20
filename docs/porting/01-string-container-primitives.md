# Subsystem 01: string/container primitives

Source: `original/include/{cstring,array,dlist,slist,set,refptr,date,time,
tracer,memman,conditio}.hpp`, `original/class/{cstring,date,time,tracer,
conditio}.cpp`, `original/func/memcheck.cpp`. This is a custom, pre-STL
C++ library predating `std::`; every other subsystem depends on it, so it
is documented and ported first.

## `CString` (`include/cstring.hpp`, `class/cstring.cpp`)

A ref-uncounted, manually managed `char*` buffer. Provides `length()`,
`pos`/`posr`/`posi` (find / reverse-find / case-insensitive find), and an
implicit `operator const char*()`.

Non-obvious behaviour to preserve or deliberately drop:

- **1-indexed `operator[]`.** `s[1]` is the first character throughout the
  codebase (e.g. `dir.cpp`'s `path[2] == ':'` drive-letter check). A
  `std::string`-based port should decide explicitly whether to keep
  1-indexing at call sites or normalize to 0-indexing everywhere — mixing
  the two silently is the likely source of off-by-one bugs during the
  port.
- `operator==(RegExp&)` bakes regex matching into string equality. This
  goes away once `RegExp` is replaced (see subsystem 02) — regex/glob
  matching should become an explicit function call, not an overloaded
  `==`.

**Target:** `std::string`.

## `SubCString`

A lazily-materialized substring/reference proxy
(`SubCString(s, index, count)`) that can be assigned back into its source
string (used for in-place path manipulation in `class/dir.cpp`, e.g.
`SubCString(tmp, 1, 2)` to read/replace a drive prefix).

**Target:** `std::string_view` for the read-only case. The assign-back
case has no direct `std::` equivalent and should become an explicit
`std::string::replace` call at each site — do not port the proxy type.

## `Array<T>` / `SortableArray<T>` (`include/array.hpp`)

A value array of `T*` that owns and deletes its elements, with fixed or
`dynamic` growth (realloc doubling via `iExpandSize`) and a hand-rolled
quicksort.

Bugs/hazards, not behaviour to preserve:

- `QSort`'s pivot/partition bookkeeping uses `static` locals — not
  reentrant, not thread-safe, and no recursion-depth guard (stack
  overflow risk on adversarial or already-sorted large inputs).
- `dynamic` arrays auto-extend on `operator[]` access when
  `index == iSize` — a surprising side-effecting `[]`. Any C++20 port
  should make growth explicit (`push_back`/`resize`), not implicit in a
  read/write operator.

**Target:** `std::vector<T>` + `std::sort`.

## `SList<T>` / `DList<T>` / `Set<T>` (`include/{slist,dlist,set}.hpp`)

Singly-linked list, doubly-linked list, and a "set" respectively. All
three own `T*` payloads and delete them on destruction unless constructed
with `shouldDelete = FALSE`.

- `Set<T>` is actually an **insertion-ordered unique list**, not a
  tree/hash set: `Add` does a linear scan for an existing equal element
  before appending, making bulk construction O(n²). `AddBefore`/
  `AddAfter` also do linear scans by pointer identity.
- `App::cViewedFiles` (the open-file-buffer list) is an `SList<Viewer>` —
  this is the one call site most worth checking carefully when choosing
  a replacement container, since buffer order and identity matter to the
  UI (digit-key 1-9 switching).

**Target:** `std::list`/`std::vector` for `SList`/`DList` depending on
whether random access or mid-list insertion dominates at each call site;
`std::vector` with dedup-on-insert, or `std::set`/`std::unordered_set` if
true (non-insertion) ordering turns out not to matter, for `Set<T>`. Pick
per call site rather than a single blanket replacement — check whether
each use relies on insertion order.

## `RefPtr<T>` / `Storage<T>` (`include/refptr.hpp`)

An intrusive-ish shared pointer. Not thread-safe (plain `UINT` refcount,
no atomics). `Storage::operator=` looks broken: it calls `ReleaseRef()`
before reading the source's `GetPointer()`, but increments `this`'s
refcount rather than the source `Storage`'s — the assignment does not
correctly share ownership.

`RCString`/`RSubCString` (`include/cstring.hpp`) are typedef'd against
this but no use site was found in the application files surveyed
(`os.cpp`, `fileman.cpp`, `osview.cpp`, `osstyle.cpp`, `globals.cpp`,
`osmisc.cpp`). **Before porting this type, grep the full `/original` tree
for any remaining use; if none exists, skip it entirely** rather than
debugging a 1990s reference-counting bug that may be dead code.

**Target (if needed):** `std::shared_ptr<T>`.

## `Date` / `Time` (`include/{date,time}.hpp`, `class/{date,time}.cpp`)

Hand-rolled Julian-day date and integer-seconds-of-day time, each with
regex-based string parsing (e.g. a `ddmmyyFormat`-style pattern) and
`CString` conversion operators. Parsing currently depends on `RegExp`,
which is non-functional in the shipped source (see
[architecture.md](architecture.md#known-broken-dead-code-in-original)).

**Target:** `std::chrono::year_month_day` / `std::chrono::sys_seconds`,
formatted and parsed via `std::format`/`std::chrono` I/O or
`std::from_chars` — parsing should not reintroduce a regex dependency.

## Debug/diagnostic support: `tracer`, `conditio`, `memcheck`, `memman`

- **`tracer`** (`include/tracer.hpp`, `class/tracer.cpp`) — debug-build
  file logger (`trace.log`) plus a manual call-stack dump on fatal error,
  via the `MStackCall` macro pushing/popping a global linked list at
  function entry/exit. **Target:** `<source_location>` plus a real
  logging library, or rely on debugger-provided stack traces; do not port
  the manual push/pop call-stack list.
- **`conditio`** (`include/conditio.hpp`, `class/conditio.cpp`) —
  assertion macros `MFailIf`/`MFail`/`MEnsure`/`MUnimplemented`, logging
  via `tracer` then calling `MTerminate()` (dumps stack, breaks into
  debugger, `exit(EXIT_FAILURE)`). No-ops outside `DEBUG` builds **except
  `MEnsure`, which compiles to a bare `while (Condition)` in release
  builds** — i.e., it silently hangs instead of asserting if the
  condition is ever false in a release build. This is a genuine bug to
  fix during the port (with a regression test), per the project's
  "preserve behaviour, but fix genuine bugs" principle — the fix is to
  make `MEnsure` assert/throw unconditionally, never loop.
  **Target:** `assert()` for internal invariants, exceptions or
  `std::expected` at real API boundaries.
- **`func/memcheck.cpp`** — global `operator new`/`delete` overrides
  backed by a fixed `void* allocs[4096]` slot table, active only under
  `DEBUG` — a crude allocation tracker/leak catcher capped at 4096
  concurrent allocations. **Target:** none needed — ASan/UBSan (already
  planned as a CI quality gate) supersedes this entirely; do not port it.
- **`include/memman.hpp`** — vestigial and non-compiling as shipped (a
  `tepmlate <class T>` typo in an otherwise-empty `RefPtr<T>` template);
  the real `RefPtr` lives in `refptr.hpp`. **Do not port; confirm zero
  includes elsewhere in `/original`, then treat as historical dead code
  only.**

## Not covered by this doc

`getopt.hpp`/`getopt.cpp` (custom argv parser) and `jac.h`'s
`strstri`/`strrchri`/`strchri`/`SearchExpression` (Boyer-Moore-Horspool
literal search) are self-contained utilities, not part of the core
container library — `SearchExpression` belongs with subsystem 02 (search
primitives), and `getopt` is low priority (lowest-risk, most easily
replaced or kept as-is) and can be documented alongside `App::Init` when
subsystem 6+ revisits argument parsing.
