# Subsystem 02: search primitives

Source: `original/func/strsrch.cpp`, `original/include/jac.h`
(`SearchExpression`), `original/class/dir.cpp` (`FileExp2RegExp`,
`FileExp2RegExpArray`), `original/apps/onscreen/os.man` (documented
wildcard syntax, section 2.1.1). Depends on
[01-string-container-primitives.md](01-string-container-primitives.md);
resolves the non-functional `RegExp` blocker noted in
[architecture.md](architecture.md#known-broken-dead-code-in-original)
for filename glob matching.

## Literal search: `strsrch`/`SearchExpression`

`original/func/strsrch.cpp` implements Boyer-Moore-Horspool search: a
`SearchExpression` precomputes a 256-entry "last occurrence" table from
the search pattern (folded to uppercase up front if the search is
case-insensitive), then `strsrch()` scans the haystack right-to-left
within each candidate window, shifting by the bad-character table on a
mismatch. This is a correct, standard algorithm and is ported directly in
behaviour (not line-by-line) as `listless::HorspoolSearcher`
(`src/Search.hpp`/`.cpp`): construct once per pattern, `find()` repeatedly
for "find next" style usage, with an explicit `start` offset rather than
the original's implicit whole-string-only scan.

Not ported: `strnrcmp`/`strnrcmpi` (small helpers internal to the
right-to-left comparison in the original's specific implementation
shape) — `HorspoolSearcher::find()` does the equivalent right-to-left
compare inline.

## Glob matching: `FileExp2RegExp`

The original translates a DOS/OS2/Win32-style wildcard into an ERE string
and hands it to `RegExp` — which is a non-functional stub in the shipped
source (see architecture.md). `docs/../src/Glob.{hpp,cpp}` implements
direct wildcard matching instead, with no regex engine involved at all.

The **documented** wildcard syntax (`os.man` section 2.1.1) is fully
supported:

| Syntax | Meaning |
|---|---|
| `*` | zero or more of any character |
| `?` | exactly one character |
| `[az]` | one character from the given set |
| `[a-z]` | one character from the given range |
| any other character, including `.` | matches literally |

`listless::glob_match()` implements this directly (classic two-pointer
wildcard matching with backtracking on `*`, extended so a bracket
expression is treated as one token). `listless::glob_match_any()` splits
a `;`-separated pattern list (as used for file-manager filespecs like
`*.cpp;*.hpp`, via the original's `FileExp2RegExpArray`) and matches if
any part matches.

**Deliberately not reproduced:** `FileExp2RegExp` (`class/dir.cpp`) also
supports an undocumented `+` suffix after a bracket expression meaning
"repeat this character class N times" or "N-M times" (e.g. `[abc]+3`,
`[abc]+1-3`), translating to an ERE `\{min,max\}` quantifier. This syntax
appears nowhere in `os.man`'s wildcard documentation — it looks like an
implementation detail of the regex translator that was never a documented
user-facing feature. Not reproduced; if a real use case turns up later,
it can be added to `glob_match()`'s token parser without changing the
existing supported syntax.

**Case sensitivity:** both `glob_match()`/`glob_match_any()` and
`HorspoolSearcher` default to case-sensitive, matching Linux's
case-sensitive filesystem convention (the original picked
case-sensitive/insensitive `RegExp` construction per platform — case
sensitive on Unix-like targets, insensitive on OS/2/DOS/Win32). Both take
an explicit `case_sensitive` parameter so a later Windows/macOS platform
layer (case-insensitive filesystems) can opt out per call site rather
than via a global toggle.

## Out of scope for this subsystem

In-file regex search mode (`-search regexp`, as opposed to the default
plain/literal search) is not implemented here. It belongs with the file
viewer core (subsystem 07, issue #7), where `std::regex` is the
straightforward backend — there is no reason to build a regex wrapper
ahead of that concrete call site.
