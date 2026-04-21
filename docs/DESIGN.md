# Design Notes

## Why subprocess and not linking?

Three candidate architectures for "dosbox under the hood":

1. **Subprocess** (chosen): `dosemu` generates a `.conf`, forks `dosbox`.
2. **In-process linking**: link against `libcpu.a`, `libdos.a`, … from
   dosbox-staging; replace its `main.cpp` with our own.
3. **Patched fork**: maintain a fork of dosbox-staging that exposes a C
   API for our hooks (the qxDOS approach).

Subprocess wins for Phase 1 because:
- Zero surgery on dosbox-staging — submodule stays clean, `git pull` in
  the submodule is never a merge conflict.
- Cleanest recovery: if dosbox crashes, the CLI handles it.
- Packaging: we can depend on a distro-installed `dosbox-staging` if the
  submodule isn't built locally.
- We keep the option of moving to (2) later for the cpmemu-style syscall
  hooks — most of the code in `config.cc` / `dosbox_conf.cc` is reusable
  verbatim.

The trade-off: interception of INT 21h / INT 31h is only possible in-process
(option 2) or via a guest-side shim (see below). Plain subprocess mode
cannot do cpmemu-exact long-name mapping or EOL translation at the DOS
syscall level.

## The `.cfg` format

Modelled on `cpmemu/examples/example.cfg`. Keys are:

	program      # DOS command to run (required unless given on CLI)
	args         # Whitespace-split arg list
	cd           # Linux chdir before launching (optional)
	drive_X      # Mount Linux path as DOS drive X (any letter)
	default_mode # auto | text | binary
	eol_convert  # true | false
	*.EXT = PATH # Per-pattern file mapping (Phase 3+)
	FILE  = PATH # Per-file mapping (Phase 3+)
	printer, aux_input, aux_output  # Device redirection (Phase 3+)
	machine, cputype, core, memsize # Passed through to dosbox.conf
	headless     # false to open an SDL window (default: true)
	verbose      # 0 | 1 | 2
	dosbox       # Absolute path to dosbox-staging binary

Environment variables inside values are expanded with `$VAR` or `${VAR}`.

## Phase 3 — long-name and EOL translation

Three approaches, still open:

### 3a. Symlink staging directory

- dosemu creates `/tmp/dosemu-XXXX/stage/`.
- For each explicit `FILE = PATH` mapping, symlink
  `/tmp/dosemu-XXXX/stage/FILE → PATH`.
- For the chdir directory, symlink each host file into stage using a
  mapped 8.3 name.
- Mount the staging dir as C:.
- On exit, walk the stage for any new files and copy them back to the
  real host directory.

Pros: pure Linux; no dosbox changes.
Cons: EOL conversion still needs in-process work. Symlink indirection
confuses some DOS tools that stat the parent dir.

### 3b. FUSE overlay

- dosemu mounts a small FUSE filesystem at `/tmp/dosemu-XXXX/stage/`.
- FUSE handles long-name ↔ 8.3 presentation and CR/LF conversion on
  read/write.
- Mount the FUSE dir as C:.

Pros: Clean semantics; read/write translation is easy.
Cons: Requires FUSE installed and user-space daemon running. Adds a
runtime dependency. More moving parts during debug.

### 3c. DOS TSR + INT E0

- Small DOS .COM file loaded via `autoexec`. Installs an INT 21h handler.
- For each intercepted call, issues `INT E0` (a dosbox-custom vector) to
  ask the host (dosemu via dosbox-patched hook) for translation.
- qxDOS's `int_e0_hostio.cpp` is the template.

Pros: Matches cpmemu architecture exactly. Translation happens at the
correct layer.
Cons: Requires a patch to dosbox to register the INT E0 hook, and
writing the .COM in DOS asm. Most invasive.

**Recommendation (post-Phase 1)**: start with 3a for explicit `FILE =
PATH` mappings (which is most of cpmemu's actual usage). Add 3b later
if EOL conversion proves essential. Keep 3c in reserve for a future
major version.

## Phase 4 — DPMI

dosbox-staging ships with a DPMI host. No work needed for DJGPP / DOS4GW
binaries beyond verifying they run through `dosemu`. Watch for:

- Memory sizing: default 16 MB may be low for modern DOS4GW programs.
- `cputype = auto` picks the fastest available; some old DPMI code
  asserts a specific CPU — fall back to `386_prefetch` or `486` if
  needed.

## Phase 5 — headless text output

Currently SDL video driver is `dummy`, so the text-mode framebuffer is
written to memory but never rendered. The program's output reaches the
user only via `INT 21h AH=2/9` writes to `STDOUT`, which dosbox lets
through to the real stdout when no window is open.

Limitations:
- Programs that write directly to B800:0000 video RAM (most DOS
  applications, not just compilers) will produce no output in headless
  mode.
- Interactive programs that poll INT 16h for keys won't see keystrokes
  piped to stdin unless we translate them.

Phase 5 work: optional ANSI rendering of the B800:xxxx framebuffer after
each vertical retrace, pushed to stdout as cursor-addressed escapes.
