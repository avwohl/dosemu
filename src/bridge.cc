//
// bridge.cc — the single translation unit in dosemu that touches dosbox
// internals.
//
// Strict cpmemu-style DOS replacement: dosbox-staging provides the CPU +
// PC hardware + BIOS; dosemu provides DOS.  dosbox's own DOS kernel still
// gets initialised (it's in libdos.a and DOSBOX_Init() wires its section),
// but we override INT 21h at the IVT so dosbox's handler never runs.
//
// Init order (inside run_com):
//   1. DOSBOX_Init()  registers all sections, wires SHELL_Init as startup
//   2. control->Init()  activates modules; dosbox's DOS installs its own
//      INT 21h callback at vector 0x21
//   3. control->SetStartUp(&dosemu_startup)  overrides the shell
//   4. control->StartUp()  calls dosemu_startup, which:
//        a. installs our INT 21h handler, overwriting vector 0x21
//        b. loads the .COM file at 0x0100:0x0100
//        c. sets CS:IP / SS:SP / DS / ES
//        d. DOSBOX_RunMachine()  CPU runs until our AH=4Ch stops it
//

#include "bridge.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "dosbox.h"
#include "control.h"
#include "cross.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "programs.h"
#include "loguru.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

namespace dosemu::bridge {

const char *dosbox_version() {
  return DOSBOX_GetVersion();
}

namespace {

constexpr uint16_t PSP_SEG  = 0x0100;          // arbitrary free segment
constexpr uint16_t COM_ENTRY_OFFSET = 0x0100;  // .COM entry is PSP+256
constexpr uint32_t MAX_COM_SIZE = 0xFF00;      // 64KB segment minus PSP minus stack

std::string  s_program;
int          s_exit_code = 0;
CALLBACK_HandlerObject s_int21_cb;

Bitu dosemu_int21() {
  switch (reg_ah) {
    case 0x02: {  // Write character in DL to stdout
      std::fputc(reg_dl, stdout);
      std::fflush(stdout);
      return CBRET_NONE;
    }
    case 0x09: {  // Write $-terminated string at DS:DX to stdout
      const PhysPt base = SegValue(ds) * 16;
      for (uint16_t off = reg_dx;; ++off) {
        const uint8_t c = mem_readb(base + off);
        if (c == '$') break;
        std::fputc(c, stdout);
      }
      std::fflush(stdout);
      return CBRET_NONE;
    }
    case 0x4C: {  // Exit with code in AL
      s_exit_code = reg_al;
      shutdown_requested = true;
      return CBRET_STOP;
    }
    default:
      std::fprintf(stderr, "dosemu: unimplemented INT 21h AH=%02Xh "
                           "(AL=%02Xh BX=%04Xh CX=%04Xh DX=%04Xh)\n",
                   reg_ah, reg_al, reg_bx, reg_cx, reg_dx);
      s_exit_code = 1;
      shutdown_requested = true;
      return CBRET_STOP;
  }
}

bool load_com_into_guest(const std::string &path) {
  std::FILE *f = std::fopen(path.c_str(), "rb");
  if (!f) {
    std::fprintf(stderr, "dosemu: cannot open %s: %s\n",
                 path.c_str(), std::strerror(errno));
    return false;
  }
  std::fseek(f, 0, SEEK_END);
  long size = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (size < 0 || static_cast<uint32_t>(size) > MAX_COM_SIZE) {
    std::fprintf(stderr, "dosemu: %s is too large for a .COM file (%ld bytes)\n",
                 path.c_str(), size);
    std::fclose(f);
    return false;
  }

  const PhysPt load_addr = PSP_SEG * 16 + COM_ENTRY_OFFSET;
  for (long i = 0; i < size; ++i) {
    uint8_t b;
    if (std::fread(&b, 1, 1, f) != 1) {
      std::fprintf(stderr, "dosemu: read error on %s\n", path.c_str());
      std::fclose(f);
      return false;
    }
    mem_writeb(load_addr + i, b);
  }
  std::fclose(f);
  return true;
}

void dosemu_startup() {
  // Override dosbox's INT 21h callback with ours.  dosbox's DOS already
  // installed its handler during control->Init(); Set_RealVec replaces the
  // IVT entry so our code runs instead.
  s_int21_cb.Install(&dosemu_int21, CB_INT21, "dosemu Int 21");
  s_int21_cb.Set_RealVec(0x21);

  if (!load_com_into_guest(s_program)) {
    s_exit_code = 1;
    shutdown_requested = true;
    return;
  }

  // .COM entry conditions: CS=DS=ES=SS=PSP_SEG, IP=0x0100, SP=0xFFFE.
  SegSet16(cs, PSP_SEG);
  SegSet16(ds, PSP_SEG);
  SegSet16(es, PSP_SEG);
  SegSet16(ss, PSP_SEG);
  reg_eip = COM_ENTRY_OFFSET;
  reg_sp  = 0xFFFE;

  DOSBOX_RunMachine();
}

} // namespace

int run_com(const char *program, bool headless, int verbose) {
  s_program  = program;
  s_exit_code = 0;

  loguru::g_stderr_verbosity = (verbose >= 2) ? loguru::Verbosity_INFO
                              : (verbose >= 1) ? loguru::Verbosity_WARNING
                                               : loguru::Verbosity_ERROR;

  static const char *dummy_argv[] = {"dosemu", nullptr};
  auto cmdline = std::make_unique<CommandLine>(1, dummy_argv);
  control      = std::make_unique<Config>(cmdline.get());

  if (headless) {
    setenv("SDL_VIDEODRIVER", "offscreen", 1);
    setenv("SDL_AUDIODRIVER", "dummy", 1);
  }

  try {
    InitConfigDir();
    DOSBOX_Init();
    control->ParseConfigFiles(GetConfigDir());

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
      std::fprintf(stderr, "dosemu: SDL_Init failed: %s\n", SDL_GetError());
      return -1;
    }

    control->ParseEnv();
    control->Init();
    control->SetStartUp(&dosemu_startup);
    control->StartUp();
  } catch (const std::exception &e) {
    std::fprintf(stderr, "dosemu: bring-up threw: %s\n", e.what());
    return -1;
  } catch (char *msg) {
    std::fprintf(stderr, "dosemu: bring-up failed: %s\n", msg);
    return -1;
  }

  return s_exit_code;
}

} // namespace dosemu::bridge
