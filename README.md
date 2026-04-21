# dosemu

An MS-DOS emulator for Linux that runs DOS programs by translating them
through [dosbox-staging](https://github.com/dosbox-staging/dosbox-staging),
with cpmemu-style convenience features on top: a `.cfg` config file,
drive-to-Linux-directory mounting via a single CLI flag, headless mode by
default so command-line DOS tools behave like ordinary Linux binaries, and
(in later phases) transparent long-name ↔ 8.3 mapping and CR/LF
translation.

**Status:** Phase 1/2 complete — `dosemu PROG.EXE` runs a DOS program in
dosbox with the current directory mounted as `C:`, no window.

## Why

Most DOS emulators use native FAT disk images. When developing with a DOS
compiler that means shuffling files in and out of the disk image for every
build. dosemu makes the DOS program see Linux files directly, so you can:

- Run a DOS C compiler as if it were a Linux CLI tool
- Use long filenames on the host while presenting 8.3 names to DOS
- Skip the SDL window entirely for text-only programs
- Redirect DOS printer / AUX I/O to Linux files
- Drive graphical DOS programs with `--window`

## Architecture

```
argv / .cfg ─▶ dosemu CLI
                   │
                   ├─ writes /tmp/dosemu-XXXXXX.conf
                   │    (mounts host dirs as DOS drives, autoexec runs PROG)
                   │
                   └─ fork + execvp("dosbox-staging/build/dosbox -conf ... -exit")
                        SDL_VIDEODRIVER=dummy by default (no window)
                        SDL_AUDIODRIVER=dummy (no audio init)
```

## Building

Requires: C++20 compiler, CMake 3.25+, meson, ninja, and dosbox-staging's
own build dependencies (SDL2, libpng, libopusfile, libspeexdsp,
libsdl2-net, libfluidsynth, libslirp, libasound2, libxi).

```sh
# Clone with submodule
git clone --recurse-submodules https://github.com/avwohl/dosemu.git
cd dosemu

# Install host dependencies (Debian / Ubuntu)
sudo apt install build-essential cmake ninja-build meson pkg-config ccache \
  libsdl2-dev libsdl2-net-dev libpng-dev libopusfile-dev libspeexdsp-dev \
  libfluidsynth-dev libslirp-dev libasound2-dev libxi-dev

# Build dosbox-staging (takes a few minutes)
meson setup dosbox-staging/build --buildtype=release dosbox-staging
ninja -C dosbox-staging/build

# Build dosemu
cmake -S src -B build
cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure
```

## Usage

```sh
dosemu [options] PROGRAM.EXE [args...]     # run a single program
dosemu config.cfg                           # load a .cfg file
dosemu --window PROGRAM.EXE                # graphical DOS program
```

Options:

	--help			Show usage
	--version		Print version
	--window		Open a dosbox SDL window (default: headless)
	--machine=NAME		dosbox machine= (default: svga_s3)
	--cpu=NAME		dosbox cputype= (default: auto)
	--memsize=N		DOS memory in MB (default: 16)
	--dosbox=PATH		Path to dosbox binary (auto-detected by default)
	--keep-conf		Keep the generated dosbox.conf for inspection
	--verbose, -v		Print the dosbox command line

Environment:

	DOSEMU_DOSBOX		Path to the dosbox-staging binary

## Example .cfg

See `examples/example.cfg` for a documented sample. Minimal:

```
program  = PROG.EXE
args     = /q /v
drive_C  = ${HOME}/dos
drive_D  = /mnt/sources
memsize  = 16
cputype  = 486
```

## License

GPLv3. dosbox-staging is GPLv2-or-later (compatible). Third-party
attributions in `docs/CREDITS.md`.

## Related Projects

- [cpmemu](https://github.com/avwohl/cpmemu) — CP/M 2.2 emulator; same
  translation-layer philosophy and the origin of the `.cfg` format
- [qxDOS](https://github.com/avwohl/qxDOS) — iOS/Mac DOS emulator, also
  built on dosbox-staging
