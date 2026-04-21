# dosemu

An MS-DOS emulator that runs DOS programs by linking the
[dosbox-staging](https://github.com/dosbox-staging/dosbox-staging) CPU and
PC-hardware emulator in-process and trapping DOS INT 21h calls to C++
implementations running on the host. Same design as
[cpmemu](https://github.com/avwohl/cpmemu), which does the equivalent for
CP/M BDOS.

**Status:** basic .COM and .EXE programs run.

	dosemu tests/HELLO.COM           → prints dosemu-hello-ok
	dosemu tests/HELLO.EXE           → prints dosemu-hello-exe-ok
	dosemu tests/SYSCALLS.COM        → dosver=6.22 / alloc=0x2000 / int21-set-ok
	dosemu tests/WRITE.COM ci-tail   → creates WROTE.TXT in CWD

dosbox-staging is linked in-process for CPU + PC hardware. DOS INT 21h is
handled entirely by C++ host code. Currently implemented:

	01  stdin char+echo    0E  set drive          3E  close handle
	02  putchar            0B  stdin ready?       3F  read handle
	07  stdin char no-echo 19  get drive          40  write handle
	08  stdin char no-echo 1A  set DTA            41  unlink
	09  print string       25  set int vector     42  seek handle
	0A  buffered input     30  get DOS version    44  ioctl (basic)
	39  mkdir              35  get int vector     47  get cwd
	3A  rmdir              3B  chdir              48  allocate (bump)
	3C  create handle      3D  open handle        49  free (no-op)
	4A  resize (stub)      4C  exit               4E  findfirst
	4F  findnext

PSP command tail at offset 80h is populated from argv. Drive mounts and
per-file / per-pattern mappings come from a `.cfg` file:

	program        = PROG.EXE
	args           = /q /v
	drive_C        = /home/me/dos
	drive_D        = /mnt/sources
	default_mode   = text           # all files: CRLF<->LF
	HELLO.TXT      = /real/path/hello_long_name.txt text
	*.BAS          = text           # mode-only wildcard override

Text mode strips CR on write and expands LF to CRLF on read so files
live on the host in Unix format. No subprocess, no dosbox shell, no
generated dosbox.conf.

## Building

Prerequisites (Debian/Ubuntu):

	sudo apt install build-essential cmake ninja-build meson pkg-config \
	  libsdl2-dev libsdl2-net-dev libpng-dev libopusfile-dev \
	  libspeexdsp-dev libfluidsynth-dev libslirp-dev libasound2-dev \
	  libxi-dev libglib2.0-dev patch

Then:

	make               # applies the dosbox-staging patch, builds dosbox
	                   # libs, builds dosemu

	build/dosemu --version   # should report the linked dosbox-staging version
	build/dosemu tests/HELLO.COM

`make distclean` resets the dosbox-staging submodule to its upstream state
and clears all build artifacts.

## Why

Most DOS emulators use native FAT disk images. When developing with a DOS
compiler that means shuffling files in and out of the disk image for every
build. dosemu makes the DOS program see host files directly, so you can:

- Run a DOS C compiler as if it were a native CLI tool
- Use long filenames on the host while presenting 8.3 names to DOS
- Skip the SDL window entirely for text-only programs
- Redirect DOS printer / AUX I/O to host files
- Drive graphical DOS programs with `--window`

Because the syscall layer is native C++, dosemu is intended to run on
Linux, macOS, Windows, iOS, iPadOS, and Android — the same platform set
cpmemu already covers.

## Architecture

	dosemu binary
		dosbox-staging CPU + PC hardware (linked as library)
		host-side DOS: INT 21h handler → C++ file / memory / process calls
		.cfg parser (cpmemu-style)

No subprocess, no generated dosbox.conf. The guest sees a DOS; the host
implements what that DOS does.

## Usage

	dosemu [options] PROGRAM.EXE [args...]
	dosemu config.cfg

Options:

	--help              Show usage
	--version           Print version
	--window            Open an SDL window (default: headless)
	--machine=NAME      PC machine type (default: svga_s3)
	--cpu=NAME          CPU type (default: auto)
	--memsize=N         DOS memory in MB (default: 16)
	--verbose, -v       Trace DOS syscalls

## Example .cfg

See `examples/example.cfg` for a documented sample. Minimal:

	program  = PROG.EXE
	args     = /q /v
	drive_C  = ${HOME}/dos
	drive_D  = /mnt/sources
	memsize  = 16
	cputype  = 486

## License

GPLv3. dosbox-staging is GPLv2-or-later (compatible). Third-party
attributions in `docs/CREDITS.md`.

## Related Projects

- [cpmemu](https://github.com/avwohl/cpmemu) — CP/M 2.2 emulator; the
  translation-layer template and the origin of the `.cfg` format
- [qxDOS](https://github.com/avwohl/qxDOS) — iOS/Mac DOS emulator, also
  built on dosbox-staging; source of the `dosbox_bridge` pattern
