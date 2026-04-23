# Patches this repo carries / has drafted

## `sdlmain-expose-setup.patch`

Applied to the dosbox-staging submodule at build time by the top-level
Makefile.  Required so dosemu can link against libdosbox.a and call the
entry points that upstream marks `static`.

## `djgpp-libc-c1loadef-stack-smash.patch`

Not applied anywhere -- we don't rebuild DJGPP libc as part of the dosemu
build.  This is a drafted fix for a genuine stack-buffer overflow in
`src/libc/crt0/c1loadef.c` of DJGPP libc 2.05 (djlsr205.zip), discovered
while chasing why gcc 12.2 `cpp.exe --version` crashes under dosemu with
a recursive #MF fault when the standard delorie `djgpp.env` is in place.

Root cause: `__crt0_load_environment_file` sizes its stack buffer to the
raw djgpp.env file size, then expands `%VAR%` references into it with no
upper bound.  Typical djgpp.env lines like
`C_INCLUDE_PATH=%/>;C_INCLUDE_PATH%%DJDIR%/include` recursively reference
already-expanded variables, so the output outgrows the buffer.  Whether
the overflow causes a crash depends on what's above `buf` in the caller's
stack frame -- delorie-built cpp.exe happens to have a return address
there and pops `;%DJ` bytes from djgpp.env on return.

See `../WIP.md` (section "The delorie cpp.exe SIGFPE") for the full
investigation log, including the exact 2683-byte trigger threshold,
evidence ruling out dosemu (our own 2.3 MB DJGPP-built C++ binary
`tests/BIGTEST.EXE` runs the full djgpp.env clean), and byte-level
disassembly of the failure point.

## Submission status

DJGPP libc hasn't seen a release since October 2015 (`djlsr205.zip`
timestamp on delorie.com).  There's no git repository, no GitHub PR
queue, and no active bug tracker.  Traditional submission paths:

1. **`djgpp@delorie.com` mailing list** -- the channel DJ Delorie
   watches, subscribe via <https://www.delorie.com/djgpp/>.
2. **Fork `djlsr` on GitHub** and publish as a community patch.  There
   is precedent: `PC-98/djlsr` (2020) is a pre-existing fork used for
   PC-98 hacking; none appear to be "the" maintained line.
3. **Carry the patch inline** in downstream distributions (FreeDOS
   packagers, andrewwutw/build-djgpp, etc.).

Given the 10-year dormancy, an upstream release absorbing this patch
is unlikely soon.  The patch is drafted here so anyone rebuilding
DJGPP libc -- or investigating a similar crash in another DPMI host --
has it ready.
