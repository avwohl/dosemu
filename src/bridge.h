//
// bridge.h — thin C++ wrapper around the in-process dosbox-staging library.
//
// Isolates every #include of dosbox internals to bridge.cc so dosemu.cc
// stays readable.  As strict DOS replacement lands, the INT 21h callback
// registration will live here.
//

#ifndef DOSEMU_BRIDGE_H
#define DOSEMU_BRIDGE_H

namespace dosemu::bridge {

const char *dosbox_version();

// Full in-process bring-up of dosbox-staging: registers sections, parses
// config, initialises SDL, activates modules, overrides SHELL_Init with our
// own startup hook, installs an INT 21h handler that dispatches to
// host-side C++, loads `program` as a .COM file, and runs the CPU until it
// exits via INT 21h AH=4Ch.  Returns the DOS exit code (AL) on success, or
// -1 on startup failure.
int run_com(const char *program, bool headless, int verbose);

} // namespace dosemu::bridge

#endif
