# Changelog

All notable changes to **dosiz** are documented here.

This file starts on 2026-08-26 and looks backwards over the 286 commits that
exist today. There is nothing to anchor it to: **dosiz has never cut a
release.** `git tag` lists six tags and all six are `ci-*` markers from
2026-04-22, put there to make the tag-triggered CI workflow fire; `--version`
has said `0.1.0-dev` throughout. So the sections below are dated phases of the
history, not versions, and no version numbers are invented here.

For the whole of this project's life the record of a change has been the commit
message that made it, and those messages are longer and more specific than any
changelog entry — they carry the traces, the counter-examples and the things
that were deliberately *not* done. This file summarises and points; `git log`
is the detail. Open work is in [`todo.txt`](todo.txt). The founding prompt, in
the owner's own words, is [`docs/original-brief.md`](docs/original-brief.md).

`WIP.md` is the session journal and is history, not a backlog. It was
where the real open list lived until this file and `todo.txt` were written; the
three things in it that were genuinely still open — the AH=3F stale-ECX
question, the Windows 35/37 result, and the never-built keyboard injection —
are in `todo.txt` now, with what could and could not be re-measured on
2026-08-26 written into each one.

## What the arc is

dosiz emulates DOS itself — the INT 21h DOS API, the INT 31h DPMI 0.9 host, the
INT 67h EMS 4.0 + VCPI provider, the PSP, the program loaders — and gets a CPU
from somewhere else. **Which "somewhere else" is the spine of this history.**
The first 251 commits got it from dosbox-staging, first as a subprocess, then
linked in-process through a patched submodule. `4014c3a` (2026-06-05) replaced
that with emu88, a from-scratch 386 core, behind a DOSBox→emu88 compatibility
shim in `src/compat/` that let `bridge.cc` keep driving the same API it always
had. `b8c54f9` then retired the submodule outright, and `8e507aa` (2026-08-09)
stopped keeping a second copy of emu88 in this tree at all: the build now reads
six emu88 sources out of a sibling **qxDOS** checkout, which is where emu88 is
written and where its 386 validation suites live.

Everything else — DJGPP, DPMI ring 3, LE binaries, EMS, graphics, sound, the
42-check regression suite — was built on top of whichever backend was current
at the time, and most of it was built on the DOSBox one and then had to be made
to work again on emu88.

## 2026-08-26: CI can build again

`.github/workflows/ci.yml` had been unable to build this repository since
`8e507aa` (2026-08-09) moved emu88 out to qxDOS. The job checked out this
repository only, with `submodules: recursive` against a tree that has no
`.gitmodules`, so `make` stopped at CMake's `emu88 sources not found`
FATAL_ERROR and the 55 `run:` blocks after it never executed. Nothing noticed:
the workflow fires on `v*`/`ci-*` tags and pull requests, and the newest of the
six tags predates the break by four months. A release would have been the first
thing to find out.

The job now checks qxDOS out into the workspace and passes
`EMU88_DIR="$GITHUB_WORKSPACE/qxDOS/emu88"`. CMake's default is a *sibling* of
the source directory, which a runner cannot produce — `actions/checkout` writes
only inside the workspace — so the path is given explicitly rather than left to
a fallback. The ref is **pinned** (`QXDOS_REF`), not floating: a commit in
qxDOS reaches this build with no version gate at all, and a CI run should say
which emu88 it compiled. The top-level `Makefile` forwards `EMU88_DIR` now, so
`make EMU88_DIR=...` works and CI needs no special-case CMake invocation.

`QXDOS_REF` is the **full 40-character SHA**, and has to be: `actions/checkout`
treats `ref` as a commit only at full length, and takes a short SHA for a branch
or tag name — the first dispatched run failed on exactly that, fetching
`refs/heads/64d8e7d*` and `refs/tags/64d8e7d*` and matching nothing. Replaying
the workflow locally could not have caught it, because that clones qxDOS
directly rather than through the action.

Two things fell out of repairing it:

- The dead DOSBox dependency set is gone. The install step pulled meson, ninja,
  fluidsynth, opusfile, speexdsp, slirp, libpng, ALSA, Xi and glib, and cached
  `dosbox-staging/build`, none of which the emu88 build touches — `dosbox-staging/`
  has zero tracked files and `src/CMakeLists.txt` looks for exactly one optional
  package, SDL2.
- The step named "Smoke (CAT.COM read path, auto mode expands LF to CRLF)"
  asserted behaviour `7463708` deliberately removed 131 commits earlier, so it
  had been failing on its own terms the whole time — invisibly, because the job
  died at configure long before reaching it. Disk reads are binary whatever
  `default_mode` says; only stdin is cooked, because expanding `\n` to `\r\n`
  inflates the byte count and breaks anything seeking relative to EOF (DJGPP
  `diff` does `lseek(-filesize, SEEK_CUR)`). The step now asserts that, and
  `README.md`'s matching sentence — which said the same false thing — says it
  too.

Measured, not assumed: a fresh `git clone` plus a qxDOS checkout inside it
builds with `make EMU88_DIR=$PWD/qxDOS/emu88`, produces `dosiz 0.1.0-dev
(backend: emu88)`, and the corrected smoke assertion passes against it.

## 2026-08: one copy of emu88, and five LE/PM client fixes

- **dosiz builds against qxDOS's emu88 instead of keeping its own copy**
  (`8e507aa`). `src/CMakeLists.txt` compiles `emu88.cc`, `emu88_pmode.cc`,
  `emu88_fpu.cc`, `emu88_mem.cc`, `opl.cc` and `sound_blaster.cc` straight out
  of `${EMU88_DIR}`, defaulting to `../../qxDOS/emu88` and failing configure
  with instructions when it is not there. The reason is in the commit message
  and in `CLAUDE.md`: the two copies had **silently drifted 129 lines**, and
  dosiz held four CPU fixes that had never gone back — `FUCOMPP`, the
  `pending_seg_idx` segment-cache selection, the unmasked linear address for
  the VESA LFB aperture, and `#UD` instead of a fatal halt. All four were
  ported into qxDOS first (qxDOS `e89af9c`) and then the copy was deleted.
  **Two consequences that are still open and are in `todo.txt`:** a qxDOS
  checkout is now a hard build dependency and CI has never been taught that, so
  CI has not been able to configure since this commit; and nothing anywhere
  records which qxDOS commit dosiz is known to work against.
- **Five LE/PM defects found bringing up freedos_micro_python's `MP.EXE`**
  (`ae8f107`, `f3c47dc`, `1c3b543`, `b047d18`, `570d5c4`). `CPU_JMP` discarded
  `use32`, truncating every LE entry EIP to 16 bits — dosiz could not run its
  own `tests/LE_MIN.EXE`. LE entry put the flat data selector in `ES` instead
  of a PSP alias, so a client reading `[es:0x80]` for its command tail read the
  IVT. `AH=0x1A` Set-DTA truncated the DTA pointer to 16 bits, as did three
  sibling `DS:DX` sites. `le_load_objects` rejected `virt_size == 0` objects.
  And a `stack_obj == 0` client had its stack carved out of the auto-data
  object, so the stack grew down through the client's own BSS — in `MP.EXE`
  that is the MicroPython heap, and the symptom was `2 in [1, 2, 3]` raising
  `TypeError: 'int' object isn't an iterator`. The reasoning is worth reading
  in `570d5c4` and in `WIP.md`'s "Fixed 2026-08-07" section. **Said plainly:
  none of the five has a fixture or a regression gate.** `tests/` holds the two
  reproduction *sources* added by `47fa13b` (`mp_in_operator.py`,
  `le_small_jt.c`) and neither a built `MP.EXE` nor a built `JT.EXE`, and
  neither file is named by `run.sh` or `ci.yml`. The fix claim rests on the
  commit messages and cannot be re-run here. That is in `todo.txt`.

## 2026-06: emu88 replaces DOSBox, and the hardware is rebuilt on it

The migration is 25 commits on 2026-06-05 and is best read in order from
`4014c3a`.

- **The DOSBox→emu88 CPU compatibility shim** (`05ae52a`, `2691106`).
  `src/compat/` re-implements the slice of the DOSBox CPU API that `bridge.cc`
  drives — registers, segments, `Descriptor`/`DescriptorTable` over emu88's
  `gdtr`/`idtr`/`ldtr`, the control-register calls, and the callback subsystem
  including the `FE 38` native-call opcode and `DOSBOX_RunMachine`. The DOS
  host above it did not have to change. The descriptor classes and the callback
  mechanism are **derived from dosbox-staging** and that is recorded in
  `docs/CREDITS.md`.
- **DPMI and DJGPP brought back up on emu88** (`4cea011`, `9a8c3f7`,
  `5370509`), plus the pieces that turned out to be missing once DOSBox was not
  supplying them: a default IVT of IRET stubs (`67f7b21`) and real-mode INT 1Ah
  with a running BIOS tick counter (`dd02ac9`).
- **INT 67h LIM EMS 4.0 + VCPI on emu88** (`b253b7f`, `a652065`), structurally
  derived from dosbox-staging's `ems.cpp` and credited as such.
- **`--window`** (`e864bef`), then **VESA VBE 2.0** (`2564908`), the **mouse**
  via INT 33h (`7bb9cf9`), **AdLib/OPL and PC speaker** (`1a28945`), **Sound
  Blaster DSP/DMA with 8259 IRQ delivery** (`4a2062b`) and the **joystick** at
  port 0x201 and INT 15h AH=84h (`a0f222b`). `ADLIB`, `SPEAKER`, `SBDMA` and
  `JOYTEST` are gates in `run.sh` and all four pass headlessly, on a build with
  no SDL2 in it; `MOUSETEST.COM` renders a cursor into a `DOSIZ_FRAME_DUMP` PPM
  but is gated by nothing. Which is why README's claim that audio, the mouse
  and the joystick "are not yet wired on the emu88 backend" is listed in
  `todo.txt` as false.
- **Three emu88 CPU fixes made from this repo** — `6a7a103` (`#UD` for
  undefined `0F` opcodes instead of aborting), `5e61742` (the VESA LFB aperture
  reachable from PM clients) and `9aa78cf` (`FUCOMPP`, which is what GNU `seq`
  needed). These are the commits that later had to be ported into qxDOS; today
  `CLAUDE.md` says to fix emu88 in qxDOS and validate there.
- **The dosbox-staging submodule was retired** (`b8c54f9`) and the docs
  refreshed (`ec0bded`). Note that the refresh did not reach everything: the
  "in-tree / vendored emu88" wording survives in `src/CMakeLists.txt`,
  `docs/CREDITS.md` and `patches/README.md`, and `DEBUGGING.md` still has a
  whole section about patching the submodule. Both are in `todo.txt`.

## 2026-05: Windows, a real GUI application, and a packet driver

- **Windows 11 builds via MSYS2 / MinGW-w64** (`bcab44a`), and then the fixes
  that a real Windows run turned up: guest files opened `O_BINARY` (`ea3417c`),
  which took the DJGPP suite there from 0 to 11 of 14, and PM-aware DOS path
  arguments plus a Windows `dos_to_host` (`b6c609c`), which took it to 35 of
  37. `EMS_PROBE` and `DJ_SIGNAL` were still failing when that was written.
  **That measurement is against the DOSBox build and cannot be rechecked**:
  both pass on Linux/emu88 now, the CPU backend has been replaced entirely
  since, and there is no Windows machine here. It is in `todo.txt` as unknown
  rather than as two known failures.
- **Smalltalk-80 runs end-to-end** — a real DJGPP-cross application driven to a
  fully rendered Xerox Smalltalk-80 v2 desktop with a live INT 33h mouse
  (`cbcacc1`, `9d2dd08`), which is also what produced `DOSIZ_SCREENSHOT_SECS`
  (`b860fbc`) and the DPMI gate-width fix in `1b42bec`.
- **A native virtual Crynwr packet driver at INT 60h** (`a102726`, `5f10459`,
  `91eed4a`), forwarding send and receive to dosbox-staging's SLIRP backend so
  guest clients like mTCP and MicroPython could use the network without
  `NE2000.COM`. **This is now dead and noisy**: the emu88 shim's
  `ETHERNET_OpenConnection` returns `nullptr`, the driver still installs itself
  unconditionally on every run, and it prints two lines to stderr before any
  program's own output. In `todo.txt`.
- **LE loader work for PMODE/W-wrapped binaries** (`92bb991`) and flat
  selectors plus an INT 0x80 handler at PM entry (`5b728d9`).

## 2026-04-22 to 04-24: DJGPP, ring 3, and a suite that can fail

This is where the project stopped being a demo. The DPMI host was moved to
ring-3 entry by default (`1fca261` … `6371618`), which is what DJGPP actually
requires, and a long cascade of DPMI defects was worked out one fault at a time
— `AX=0300` scratch stacks, `AX=0203` CWSDPMI-style exception dispatch, LDT
starter-set bases, `PM_CB_STACK` D-bit, nested-exec save and restore of the
parent's CR0, LDT and PM exception handlers. `WIP.md` is the log of that hunt
and is worth reading for how the faults were narrowed.

Two things from this period are load-bearing today:

- **`tests/djgpp/run.sh`, the regression suite** (`ba8130f`, then grown a
  fixture at a time over the following two days). It is 42 checks and it is
  what a person runs: EMS, VCPI and HMA probes; the audio, joystick and LFB
  gates; eleven DJGPP fixtures; FreeCOM and nested spawn; and real third-party
  DJGPP tools — `grep`, `diff`, `sed`, `sort`, `wc`, `gawk`, `gzip`, `ls`,
  `find`, `patch`, `tar`, `bc`, `m4`, `flex`, `seq`, `factor`, `basename`,
  `make`, and a 2.3 MB C++ binary with STL and regex. Measured on 2026-08-26
  against qxDOS `7352fc5`: **42 passed, 0 failed.** It is reachable from
  neither the `Makefile` nor `README.md`, and `make test` runs `ctest`, which
  finds no tests and exits 0 — in `todo.txt`.
- **`AH=3F` disk reads are binary** (`7463708`). Real DOS does no CR/LF
  translation on disk-file reads and dosiz stopped doing it. The `.cfg` text
  mode still strips CR on the *write* side. `README.md` and one `ci.yml` step
  have both said the opposite for the 131 commits since; both are in
  `todo.txt`.

Also from this stretch: the LFN truename call, `dup`/`dup2`, alloc strategy,
`findfirst`/`findnext` surviving DTA swaps, the DBCS lead-byte table, the Open
Watcom compile-link-run pipeline (`8ce31ab`, gated on an install at `~/ow` that
is not present here), `DEBUGGING.md` (`2508c28`), the DJGPP libc `c1loadef`
stack-smash patch and its verification script (`15a0339`, `5306990`), and
`docs/c-toolchain-guide.md` (`18f837d`). The project was renamed from `dosemu`
to `dosiz` at `90169f2`.

## 2026-04-21: from a dosbox subprocess to a DOS host

The first day is 112 of the 286 commits on main, and it covers the whole idea.
It starts with a subprocess-style dosbox launcher (`a2bc693`), replaces that
with in-process linkage (`e51b3d3`), overrides dosbox's `SHELL_Init` so dosiz's
own startup hook takes the place of the DOS shell (`634eb6e`, `9da0f77`), and
then runs its first DOS program with the INT 21h call served host-side
(`49d5848`).

From there, in order: file I/O and the PSP command tail; the MZ `.EXE` loader;
`.cfg`-driven drive mounts and text-mode translation; per-pattern and per-file
mappings; `findfirst`/`findnext` with 8.3 mangling; the MCB chain allocator
replacing the bump allocator; unknown `AH` soft-failing with `CF=1` instead of
exiting; `AH=4Bh` Load and Execute with three-level nesting; the **whole DPMI
0.9 host** built in numbered stages from a detection stub through the
real-to-protected switch, 16- and 32-bit PM entry, interrupt reflection, LDT
management, real-mode callbacks and PM exception dispatch; and the **LE/LX
loader**, from recognising the format through page-copy loading, per-object LDT
descriptors, internal-reference fixups, selector-bearing fixups and a catch-all
exception handler, to `LE_MIN.EXE` running to a clean exit.

FreeDOS `xcopy.exe` copying a file (`cd680a7`) and a real DOS-hosted Open
Watcom binary (`2e86545`) are the two "it runs real software" markers from that
day. **Neither is reproducible from a clean checkout today**: `xcopy.exe` came
out of the dosbox-staging build tree, which no longer exists here, and the
`mTCP-FTP.EXE` result README cites alongside it was never committed. Both are
in `todo.txt`.

## A note on what this file does not say

It does not say the repository's documentation is accurate. As of 2026-08-26
`README.md`, `DEBUGGING.md`, `docs/CREDITS.md`, `docs/c-toolchain-guide.md`,
`docs/emu88-cpu-backend.md`, `examples/example.cfg`, `patches/README.md` and
`src/CMakeLists.txt`'s header comment each contain at least one statement that
is measurably false — a dead flag documented as working, a submodule that was
retired, an emu88 that is described as living in this tree, a text-mode
behaviour that was removed 131 commits ago. Each one is written up in
`todo.txt` with what was measured. They are listed there rather than fixed here
because a changelog that quietly corrected them would leave no record that they
were ever wrong.
