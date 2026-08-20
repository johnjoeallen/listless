# Getting started

## Build

Listless currently has a Linux ncurses backend.

```sh
cmake -S . -B build
cmake --build build --parallel
```

## Open something

```sh
./build/lss                 # browse the current directory
./build/lss path/to/file    # open a file
./build/lss path/to/dir     # browse a directory
```

Piped input opens in the viewer. See [usage](usage.md) and
[syntax highlighting](syntax-highlighting.md).

!!! danger "**Not implemented**"
    Windows and macOS backends are planned; Linux is the current runtime platform.
