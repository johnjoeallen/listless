# Using Listless

## Browser

Arrow keys, Home, End, Page Up, and Page Down move the directory selection.
Enter opens a file or enters a directory. `q` or Escape quits.

The browser shows one directory at a time in a column-major grid. It is not a
dual-pane file manager; copy, move, rename, delete, and create-directory
actions are not available yet.

## Viewer

| Action | Keys |
|---|---|
| Line down/up | Down / Up, Enter, `j` / `k` |
| Page down/up | Page Down / Page Up, Space / `b` / `B`, Ctrl+F / Ctrl+B |
| Half page down/up | `d` / `u`, Ctrl+D / Ctrl+U |
| Start/end (text) | Home / End, `g` / `G` |
| Text/hex view | `h` / `H` |
| Go to line (text) | `:` |
| Go to offset (hex) | `g` / `G` |
| Search forward | `/`, `s`, `S` |
| Repeat search | `a` / `A`, `n` / `N` |
| Set/jump bookmark | Alt+`0`–`9` / `0`–`9` |
| Close | `q`, `Q`, Escape |

In hex view, `g` and `G` open the offset prompt.

!!! danger "**Upcoming feature**"
    A fresh backward-search prompt (`?`) is not available yet.
