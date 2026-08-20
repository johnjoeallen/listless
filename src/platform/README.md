# Platform interfaces

Thin, platform-specific implementations behind interfaces declared in the
parent `/src` directory. No `#ifdef`-per-platform logic in shared source
files — each platform directory provides a complete implementation of the
same interface, and CMake selects which one to build.

- `linux/` — the first target platform; implementations land alongside
  the subsystems that need them (directory enumeration, console I/O,
  keyboard input, timing — see `/docs/architecture.md`).
- `windows/` — permanent target, implemented after the Linux port proves
  each interface out.
- `macos/` — permanent target, implemented after the Linux port proves
  each interface out.

Each subdirectory is empty until its first subsystem issue lands.
