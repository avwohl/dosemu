# Credits and Third-Party Components

## emu88 (CPU backend)

The 386 + FPU + protected-mode CPU, memory, and PC hardware are provided by
**emu88** (`emu88/`), a from-scratch x86 interpreter vendored into dosiz and
shared with the sibling [qxDOS](https://github.com/avwohl/qxDOS) project.

## dosbox-staging (derived code)

dosiz no longer embeds or links
[dosbox-staging](https://github.com/dosbox-staging/dosbox-staging)
(GPLv2-or-later). However, parts of dosiz are **derived from** it:

- The DOSBox→emu88 CPU compatibility shim in `src/compat/` — the descriptor-
  table classes (`Descriptor` / `DescriptorTable` / `GDTDescriptorTable`,
  including the `S_Descriptor` / `G_Descriptor` bitfield layout) and the
  native-call callback mechanism (the `FE 38` callback opcode, the per-type
  callback-stub byte sequences, and the `CALLBACK_RunRealInt` / `RunRealFar` /
  `DOSBOX_RunMachine` run-loop semantics) follow dosbox-staging's
  `src/cpu/callback.cpp` and `include/cpu.h`.
- The INT 67h LIM EMS 4.0 + VCPI provider in `src/bridge.cc` follows the
  structure, constants, and status codes of dosbox-staging's `src/ints/ems.cpp`.

dosiz's own code — the emu88 CPU core, the DPMI 0.9 host (INT 31h sub-functions,
mode-switch primitives, RM callbacks, PM exception dispatch), the LE loader, and
the INT 21h handlers in `src/bridge.cc` — is not derived from dosbox-staging.

dosiz is licensed GPLv3, which is compatible with dosbox-staging's
GPLv2-or-later.

## FreeDOS

When dosiz uses FreeDOS binaries or source, attribution and source
availability obligations from FreeDOS apply. See https://freedos.org.

## Inspiration

- [cpmemu](https://github.com/avwohl/cpmemu) — the translation-layer approach
  and much of the `.cfg` / file-mapping design.
- [qxDOS](https://github.com/avwohl/qxDOS) — iOS/Mac DOS emulator; source of the
  vendored emu88 CPU core and the host-I/O bridge pattern.
- [tnylpo](https://github.com/SvenMb/gbrein_tnylpo) — prior art for CP/M
  syscall translation.
