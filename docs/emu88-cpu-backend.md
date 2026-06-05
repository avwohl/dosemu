# emu88: the dosiz CPU backend

dosiz runs on **emu88**, a from-scratch C++20 386 interpreter, for its CPU +
memory + PC hardware. (Historically dosiz linked **dosbox-staging**; the
migration to emu88 is complete and DOSBox is no longer a build dependency.
dosiz drives emu88 through a thin DOSBox→emu88 compatibility shim in
`src/compat/`, so the DOS host in `src/bridge.cc` runs unchanged.) emu88 is
vendored in-tree under `emu88/` and shared with the sibling
**[qxDOS](https://github.com/avwohl/qxDOS)** project, which originated it.

This note records why emu88 is a good fit for dosiz and the CPU behaviors dosiz
depends on. emu88 validation (qxDOS commit `f2392f0`, 2026-06-04):

## Why this is worth it for dosiz specifically

dosiz is a DPMI 0.9 host + LE loader + INT 21h shim — it leans hard on **32-bit
protected mode, paging, LDT descriptors, mode switches, far transfers, and PM
exception (#GP) interception**. emu88 is now validated exactly where that matters:

- **test386.asm full-system suite: PASS** — real → protected → paging → V86,
  GDT/LDT, call gates, TSS task switching, reaching POST `0xFF` with the EE
  arithmetic output matching the reference byte-for-byte.
- **SingleStepTests/80386 (1.76M per-instruction cases): 99.983%** — every
  architecturally *defined* behavior is correct.

emu88 is original code (not derived from dosbox-staging), licensed **GPL v3** in
qxDOS (it ships under `qxDOS/emu88/`). Adopting it removes the *dosbox-staging
codebase* dependency for the processor core, but emu88 is itself copyleft
(GPL v3) — so this is a dependency change, **not** a way out of GPL. emu88 is the
same author's code, so its license for dosiz is the author's to set; confirm the
intended terms before relying on it.

## CPU behaviors dosiz depends on — all fixed/validated (2026-06-04)

These were the most recent emu88 corrections and they are squarely in dosiz's path:

- **Segment-limit faults attribute to the correct vector** — `#SS` (12) only for
  real SS accesses, `#GP` (13) for everything else (DS/ES/FS/GS/CS), via an
  explicit effective-segment index rather than scanning segment-register *values*
  (which mis-fired when two selectors shared a value). A DPMI host that traps PM
  exceptions needs the right vector + error frame; this now matches hardware.
- **Instruction fetch crossing the CS limit** raises `#GP` with the return IP at
  the faulting instruction's start, and the instruction is fully aborted (no
  partial side effects).
- **Far/segment-load operands** — `LES/LDS/LSS/LFS/LGS`, far `CALL`/`JMP`
  (direct and indirect), `BOUND`: the second word/dword of the operand wraps the
  offset correctly in 16-bit address mode and faults cleanly otherwise. RM
  callbacks and DPMI mode-switch stubs exercise far transfers heavily.
- **String ops** publish the effective segment so boundary faults are `#GP`, and
  32-bit-address (`a32`) string ops are handled correctly.
- **DIV/IDIV** overflow-boundary microcode and real-mode interrupt dispatch are
  correct.

## How to wire it in

emu88 is a plain class you step one instruction at a time. The CPU core is
self-contained in four files; pull only these (the `dos_*.cc` layer is qxDOS's
own DOS/BIOS/DPMI host — dosiz keeps its own `bridge.cc` host instead):

```
emu88/emu88.cc        emu88/emu88.h         (core: decode/execute, real+PM)
emu88/emu88_pmode.cc                         (protected mode, gates, paging, IDT/GDT)
emu88/emu88_fpu.cc                           (x87 FPU)
emu88/emu88_mem.cc    emu88/emu88_mem.h      (physical memory + A20)
```

Model: construct an `emu88_mem`, construct an `emu88` over it, set
`cpu_type = CPU_386`, set up state, then call `execute()` in a loop.

Reference integrations to copy from (these *are* working harnesses):
- `../qxDOS/tests/test386_run.cc` — a minimal full machine: loads a ROM, models
  POST/`0xE9` ports, runs `execute()` to HLT. Closest to "be the CPU under a host."
- `../qxDOS/tests/sst386.cc` — loads/compares full register + RAM state around a
  single `execute()`; good reference for save/restore of CPU state.

Useful public API (`emu88.h`): `execute()`, `reset()`, `halted`,
`protected_mode()`, `paging_enabled()`, `v86_mode()`, `code_32()`/`stack_32()`,
`translate_linear(linear, write)`, `load_segment()`, `get_eflags()/set_eflags()`,
`raise_exception()` / `do_interrupt_pm()`.

## Concrete limitations to know before you commit to it

1. **Physical memory is a flat array, and out-of-range accesses WRAP, they do not
   fault.** `emu88_mem` defaults to **1MB** (constructor takes a size; practical
   ceiling ~16MB). `mask_addr()` returns `addr % mem_size` for anything past the
   end. Paging *is* supported (`translate_linear`, `paging_enabled`), so a DJGPP /
   DOS4G linear address space is mapped onto that physical array — but **size the
   `emu88_mem` to whatever extended/DPMI memory you hand out**, or high linear
   pages will silently alias low physical memory. This is the single most likely
   surprise for a DJGPP workload.
2. **`emu88_mem` carries qxDOS-specific VGA Mode-X plane state** (`vga_planes`,
   `vga_planar`, …). Harmless, but it means `emu88_mem` is lightly coupled to
   qxDOS; the `emu88` CPU class itself is clean. You may want to subclass/trim
   `emu88_mem` for dosiz (it's `virtual`, so overriding `fetch_mem*/store_mem*`
   for MMIO/host traps is straightforward — that's how qxDOS hooks INT/IO).

## Known residuals — these are NOT bugs, don't chase them

The 11 remaining SingleStepTests failures are all *officially-undefined* or
*environment-specific*; none affect correct DJGPP/Watcom/DOS4G execution:

- **IMUL undefined `SF`/`PF`/`AF` flags** (`0FAF`/`69`/`6B`): modelled to ~96%;
  the result and `CF`/`OF` are 100% exact. The residual is gate-level 386
  multiplier-array state. Compilers don't branch on IMUL's undefined flags.
- **`IN` from peripheral ports**: the "expected" value is whatever device sat on
  the 386EX capture bench — environment data, not CPU behavior.
- **Self-modifying `REP` within the prefetch window**: a `REP STOS` overwriting
  its own already-prefetched trailing byte; emu88 has no cycle-accurate prefetch
  queue. The architectural result (memory/`ECX`/`EDI`) is correct.

## Test-coverage caveat

The free SingleStepTests/80386 corpus is **real-mode per-instruction only** —
there is no public *protected-mode* per-instruction corpus. PM/paging/V86
instruction-level confidence therefore rests on **test386.asm** (thorough for
mode transitions and the privileged path, but not exhaustive per-opcode in PM).
**When dosiz switches its CPU to emu88, re-run dosiz's own 23 DPMI fixtures and
the LE-loader fixtures** — those exercise the exact PM/DPMI paths that the public
corpora don't, and are the real acceptance test for this backend.
