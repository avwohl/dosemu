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

// Minimal in-process dosbox bring-up.  Instantiates CommandLine + Config and
// calls DOSBOX_Init() to register the section hierarchy.  Does NOT yet
// activate any modules (no CPU, memory, or device init).  Returns true on
// success; logs to stderr on failure.  Intended as a per-session smoke test
// while the full init sequence is being built up.
bool register_sections();

} // namespace dosemu::bridge

#endif
