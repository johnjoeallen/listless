# Subsystem 03: directory enumeration

Source: `original/include/dir.hpp`, `original/class/dir.cpp`, plus the
platform-specific `Directory::fill()` implementations
`original/class/{OS2,win32,Dos}/dir.cpp`. Depends on
[01-string-container-primitives.md](01-string-container-primitives.md)
and [02-search-primitives.md](02-search-primitives.md) (glob filtering).
This is Listless's core "ls" behaviour.

## What the original provides

- **`Dirent`** — one directory entry: name, full path, size, DOS-style
  `FDATE`/`FTIME`, `attrib_t` mode bitmask, with `isDirectory()`/
  `isReadOnly()` derived from the mode bitmask via `ISDIR`/`FILE_READONLY`
  macros, and case-insensitive-or-sensitive `operator<`/`>`/`==` depending
  on platform (`__UNIX__` branch is case-sensitive; OS/2/DOS/Win32 use
  `stricmp`).
- **`Directory`** — `fill(fileSpec = "*")` populates a `SortableArray<Dirent>`
  for one directory level via the platform-specific backend
  (`DosFindFirst`/`FindFirstFile`/`_dos_findfirst` — **there is no
  `__UNIX__` backend in `/original` at all**, so this is new platform work
  on Linux, not a port); `sort(cmp)` with an optional custom comparator.
- **`FileTreeWalker`** — recursive directory walk with virtual
  `found()`/`searching()`/`dirFinished()` hooks. **Zero use sites found
  anywhere in `/original`** (checked both `apps/onscreen` and `class`) —
  it's dead/unused infrastructure in the shipped source, not a real
  feature. **Not ported in this subsystem.** If a real recursive-walk need
  appears later (e.g. recursive copy/delete in subsystem 11, or a
  recursive search feature), it can be added then against a concrete call
  site, as a set of callbacks rather than a virtual-inheritance hierarchy
  (more idiomatic modern C++, and this codebase has no other subclasses
  of it to justify the extension-point design).
- **Path helpers** — `mergePath`, `splitPath`, `nativePathName`/
  `unixPathName`/`dosPathName`, `changeDir`, `isDir`, `queryCurrentDir`,
  `queryCurrentDisk`, `expandDir`, `conv2NativePathSep`, `pathSeparator`.
  Built around DOS/OS2/Win32 drive letters (`C:`) and backslash/forward-
  slash normalization — concepts that don't exist on Linux.

## What's ported here

- **`DirEntry`** (`src/Directory.hpp`) — name, full `std::filesystem::path`,
  size, `std::filesystem::file_time_type` (last write time — `std::chrono`
  directly, no separate `Date`/`Time` wrapper needed, per the subsystem 01
  decision to defer `Date`/`Time` until a concrete need appears), and
  `is_directory`/`is_read_only` booleans computed directly from
  `std::filesystem::directory_entry`'s status rather than reconstructed
  from a DOS-style attribute bitmask.
- **`Directory`** (`src/Directory.hpp`/`.cpp`) — `fill(pattern = "*")`
  lists one directory level via `std::filesystem::directory_iterator`,
  filtered through subsystem 02's `glob_match()`; `size()`/`operator[]`;
  `sort()` with a default comparator (case-sensitive name order, matching
  the original's `__UNIX__` branch) and an optional custom one, as a hook
  for later sort-by-extension/date/size (file manager, subsystem 06).
- **`split_path_and_pattern()`** — given a filespec argument like
  `/home/user/*.cpp`, `*.cpp`, or a bare directory path, returns
  `(directory, pattern)`: if the whole input names an existing directory,
  the pattern is `"*"`; otherwise the last path component is treated as
  the pattern and the rest as the directory (defaulting to `.` if there
  is no directory component). This is the Linux-relevant subset of the
  original's `splitPath()` — with no drive letters to parse.

## What's deliberately not ported

- **`FileTreeWalker`** — see above; dead code, no use sites.
- **Drive-letter logic** in `splitPath`/`changeDir`/`queryCurrentDir`/
  `queryCurrentDisk` — meaningless on Linux. `std::filesystem::path`'s own
  operations (`operator/`, `parent_path()`, `filename()`,
  `lexically_normal()`) replace `mergePath`/`nativePathName`/
  `unixPathName`/`dosPathName`/`conv2NativePathSep` directly; there is no
  wrapper for these, callers use `std::filesystem` directly.
- **`expandDir()`** — recursively resolves a wildcard *directory* path
  component by component (e.g. resolving a wildcard partway through a
  path, not just the final filename). This is obscure, undocumented in
  `os.man`'s wildcard section, and has no found call site outside
  `splitPath`'s own fallback branch. Not ported; `split_path_and_pattern()`
  only splits the final path component as a pattern, nothing upstream of
  it.
- **`isDir()`/`changeDir()`/`queryCurrentDir()`** as named wrappers — thin
  one-line calls over `std::filesystem::is_directory()`/
  `std::filesystem::current_path()`. Callers use `std::filesystem`
  directly rather than a Listless-specific indirection over it.

## Platform note

`Directory::fill()`'s `std::filesystem::directory_iterator`-based
implementation lives directly in `src/Directory.cpp`, not under
`/src/platform/linux` — `std::filesystem` is already portable across Linux,
Windows, and macOS, so there is no platform seam here to isolate. The
`/src/platform` directories are for genuinely OS-specific APIs (console I/O,
keyboard input — subsystems 04/05), not for functionality the standard
library already abstracts.
