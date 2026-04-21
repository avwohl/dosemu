//
// bridge.cc — the single translation unit in dosemu that touches dosbox
// internals.
//
// Strict cpmemu-style DOS replacement is built up here incrementally:
//   1. link against dosbox-staging libs (done)
//   2. register section hierarchy via DOSBOX_Init()           <-- now
//   3. activate CPU/memory/hardware modules (no DOS, no shell)
//   4. install INT 21h callback
//   5. load .EXE/.COM host-side and run
//
// All dosbox headers stay confined to this file.
//

#include "bridge.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "dosbox.h"
#include "control.h"
#include "programs.h"

#include <cstdio>
#include <memory>
#include <vector>

namespace dosemu::bridge {

const char *dosbox_version() {
  return DOSBOX_GetVersion();
}

bool register_sections() {
  // DOSBOX_Init() reads from the global `control`; it must exist first.
  // CommandLine is the argv holder Config consumes on construction.
  static const char *dummy_argv[] = {"dosemu", nullptr};
  auto cmdline = std::make_unique<CommandLine>(1, dummy_argv);
  control      = std::make_unique<Config>(cmdline.get());

  try {
    DOSBOX_Init();
  } catch (const std::exception &e) {
    std::fprintf(stderr, "dosemu: DOSBOX_Init threw: %s\n", e.what());
    return false;
  }
  return true;
}

} // namespace dosemu::bridge
