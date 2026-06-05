# dosiz project conventions

## Workflow

- When you reach a summary/end-of-turn for a unit of work, commit and push
  to origin/main without asking first. Don't wait for explicit "commit" or
  "push" instructions.

## Backend

- dosiz emulates DOS itself — the INT 21h / INT 31h (DPMI) / INT 67h (EMS+VCPI)
  host, PSP, program loader, and DPMI mode-switching all live in `src/bridge.cc`.
  The CPU + memory + PC hardware come from the in-tree **emu88** 386 core
  (`emu88/`, vendored, shared with qxDOS), driven through a thin DOSBox→emu88
  compatibility shim in `src/compat/`.
- There is **no DOSBox, SDL2, or glib dependency**. The build is plain CMake over
  emu88 + dosiz: `make` (a thin wrapper) or
  `cmake -S src -B build && cmake --build build -j`.
- The `Descriptor`/`DescriptorTable` classes and the native-call callback
  mechanism in `src/compat/`, plus the LIM EMS 4.0 / VCPI provider in
  `src/bridge.cc`, are **derived from dosbox-staging (GPLv2-or-later)**. Keep that
  attribution accurate in `docs/CREDITS.md`.
- emu88 (`emu88/*.cc/.h`) is a vendored copy shared with the sibling qxDOS
  project. A CPU fix here should be ported back to qxDOS.
