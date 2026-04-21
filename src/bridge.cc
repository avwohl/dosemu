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
//        b. builds a minimal PSP with the command tail at PSP:80h
//        c. loads the .COM file at PSP:0100h
//        d. sets CS:IP / SS:SP / DS / ES
//        e. DOSBOX_RunMachine()  CPU runs until AH=4Ch stops it
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

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dosemu::bridge {

const char *dosbox_version() {
  return DOSBOX_GetVersion();
}

namespace {

constexpr uint16_t PSP_SEG          = 0x0100;
constexpr uint16_t COM_ENTRY_OFFSET = 0x0100;
constexpr uint32_t MAX_COM_SIZE     = 0xFF00;

// --- Run-time state, reset by run_com() ------------------------------------

std::string                 s_program;
std::vector<std::string>    s_args;
int                         s_exit_code = 0;

// DOS file-handle table.  Handles 0..2 are permanently bound to the host
// stdin/stdout/stderr file descriptors.  User opens get the next free slot
// starting at 5 (DOS conventionally reserves 0..4 for stdin/out/err/aux/prn).
constexpr int FIRST_FILE_HANDLE = 5;
constexpr int MAX_HANDLES       = 20;
std::map<uint16_t, int>     s_handles;  // DOS handle -> host fd

// Drive table: letter -> host directory.  Populated (currently minimally)
// from the CLI working directory.  cfg.drives from .cfg files will feed in
// additional entries in a later iteration.
std::map<char, std::string> s_drives;
char                        s_current_drive = 'C';

// --- Host <-> guest helpers -----------------------------------------------

// Read a NUL-terminated DOS path from guest memory at seg:off.
std::string read_dos_string(uint16_t seg, uint16_t off, size_t max = 260) {
  const PhysPt base = seg * 16;
  std::string s;
  for (size_t i = 0; i < max; ++i) {
    const uint8_t c = mem_readb(base + off + i);
    if (c == 0) break;
    s += static_cast<char>(c);
  }
  return s;
}

// Translate a DOS path (drive-letter + backslashes) to a host path.
// For this iteration, all drives resolve under a single base directory.
std::string dos_to_host(const std::string &dos_path) {
  std::string path = dos_path;
  char drive = s_current_drive;

  if (path.size() >= 2 && path[1] == ':') {
    drive = static_cast<char>(std::toupper(static_cast<unsigned char>(path[0])));
    path  = path.substr(2);
  }

  std::replace(path.begin(), path.end(), '\\', '/');

  auto it = s_drives.find(drive);
  if (it == s_drives.end()) {
    // Unknown drive -> treat path as relative to CWD.
    return path;
  }
  if (path.empty()) return it->second;
  if (path.front() == '/') return it->second + path;
  return it->second + "/" + path;
}

// Allocate the next free DOS handle for a given host fd.
int allocate_handle(int fd) {
  for (int h = FIRST_FILE_HANDLE; h < MAX_HANDLES; ++h) {
    if (s_handles.find(h) == s_handles.end()) {
      s_handles[h] = fd;
      return h;
    }
  }
  return -1;
}

// --- INT 21h dispatcher ----------------------------------------------------

void set_cf(bool val) {
  // Build the FLAGS word dosbox will pop on IRET by flipping bit 0 on the
  // stacked copy at SS:SP+4 (IP:CS:FLAGS).  Uses the dosbox helper.
  CALLBACK_SCF(val);
}

void return_error(uint16_t dos_err) {
  reg_ax = dos_err;
  set_cf(true);
}

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

    case 0x3C: {  // Create file; CX=attr, DS:DX=path.  Returns handle in AX.
      const std::string dos_path  = read_dos_string(SegValue(ds), reg_dx);
      const std::string host_path = dos_to_host(dos_path);
      int fd = ::open(host_path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
      if (fd < 0) { return_error(0x05); break; }  // 0x05 = access denied
      int h = allocate_handle(fd);
      if (h < 0) { ::close(fd); return_error(0x04); break; }  // 0x04 = too many open files
      reg_ax = static_cast<uint16_t>(h);
      set_cf(false);
      return CBRET_NONE;
    }

    case 0x3D: {  // Open file; AL=mode, DS:DX=path.  Returns handle in AX.
      const std::string dos_path  = read_dos_string(SegValue(ds), reg_dx);
      const std::string host_path = dos_to_host(dos_path);
      int flags = O_RDONLY;
      switch (reg_al & 0x07) {
        case 0: flags = O_RDONLY; break;
        case 1: flags = O_WRONLY; break;
        case 2: flags = O_RDWR;   break;
      }
      int fd = ::open(host_path.c_str(), flags);
      if (fd < 0) { return_error(0x02); break; }  // 0x02 = file not found
      int h = allocate_handle(fd);
      if (h < 0) { ::close(fd); return_error(0x04); break; }
      reg_ax = static_cast<uint16_t>(h);
      set_cf(false);
      return CBRET_NONE;
    }

    case 0x3E: {  // Close handle in BX.
      const auto it = s_handles.find(reg_bx);
      if (it == s_handles.end()) { return_error(0x06); break; }  // invalid handle
      ::close(it->second);
      s_handles.erase(it);
      set_cf(false);
      return CBRET_NONE;
    }

    case 0x3F: {  // Read from handle BX, CX bytes, into DS:DX.
      int fd = -1;
      if (reg_bx == 0) fd = STDIN_FILENO;
      else {
        auto it = s_handles.find(reg_bx);
        if (it == s_handles.end()) { return_error(0x06); break; }
        fd = it->second;
      }
      std::vector<uint8_t> buf(reg_cx);
      ssize_t n = ::read(fd, buf.data(), reg_cx);
      if (n < 0) { return_error(0x05); break; }
      const PhysPt dst = SegValue(ds) * 16 + reg_dx;
      for (ssize_t i = 0; i < n; ++i) mem_writeb(dst + i, buf[i]);
      reg_ax = static_cast<uint16_t>(n);
      set_cf(false);
      return CBRET_NONE;
    }

    case 0x40: {  // Write to handle BX, CX bytes, from DS:DX.
      int fd = -1;
      if      (reg_bx == 1) fd = STDOUT_FILENO;
      else if (reg_bx == 2) fd = STDERR_FILENO;
      else {
        auto it = s_handles.find(reg_bx);
        if (it == s_handles.end()) { return_error(0x06); break; }
        fd = it->second;
      }
      std::vector<uint8_t> buf(reg_cx);
      const PhysPt src = SegValue(ds) * 16 + reg_dx;
      for (uint16_t i = 0; i < reg_cx; ++i) buf[i] = mem_readb(src + i);
      ssize_t n = ::write(fd, buf.data(), reg_cx);
      if (n < 0) { return_error(0x05); break; }
      reg_ax = static_cast<uint16_t>(n);
      set_cf(false);
      return CBRET_NONE;
    }

    case 0x42: {  // Seek handle BX; CX:DX = offset, AL = whence (0,1,2).
      auto it = s_handles.find(reg_bx);
      if (it == s_handles.end()) { return_error(0x06); break; }
      int whence = SEEK_SET;
      if (reg_al == 1) whence = SEEK_CUR;
      if (reg_al == 2) whence = SEEK_END;
      off_t off = (static_cast<int32_t>(reg_cx) << 16) | reg_dx;
      off_t pos = ::lseek(it->second, off, whence);
      if (pos < 0) { return_error(0x19); break; }  // seek error
      reg_ax = static_cast<uint16_t>(pos & 0xFFFF);
      reg_dx = static_cast<uint16_t>((pos >> 16) & 0xFFFF);
      set_cf(false);
      return CBRET_NONE;
    }

    case 0x4C: {  // Exit with code AL
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
  return CBRET_NONE;
}

// --- Program load ----------------------------------------------------------

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

// Build a minimal PSP at PSP_SEG:0000.  Only the command tail at offset 80h
// is populated meaningfully; everything else is zeroed.  Real programs
// querying the PSP will find a legal-looking but mostly empty structure.
void build_psp(const std::vector<std::string> &args) {
  const PhysPt psp = PSP_SEG * 16;
  for (int i = 0; i < 256; ++i) mem_writeb(psp + i, 0);

  // INT 20h at offset 0x00 (CD 20) — programs sometimes jump here.
  mem_writeb(psp + 0x00, 0xCD);
  mem_writeb(psp + 0x01, 0x20);

  // Command tail: " arg1 arg2 ..." + 0x0D
  std::string tail;
  for (const auto &a : args) {
    tail += ' ';
    tail += a;
  }
  if (tail.size() > 126) tail.resize(126);
  mem_writeb(psp + 0x80, static_cast<uint8_t>(tail.size()));
  for (size_t i = 0; i < tail.size(); ++i)
    mem_writeb(psp + 0x81 + i, static_cast<uint8_t>(tail[i]));
  mem_writeb(psp + 0x81 + tail.size(), 0x0D);
}

void dosemu_startup() {
  // Override dosbox's INT 21h callback.  The CALLBACK_HandlerObject is a
  // local: its destructor runs when this function returns, which is still
  // inside control->StartUp() -- dosbox's callback tables are alive and the
  // Uninstall path works cleanly.  A global-lifetime object would destruct
  // after control.reset() and crash on a freed callback table.
  CALLBACK_HandlerObject int21_cb;
  int21_cb.Install(&dosemu_int21, CB_INT21, "dosemu Int 21");
  int21_cb.Set_RealVec(0x21);

  build_psp(s_args);

  if (!load_com_into_guest(s_program)) {
    s_exit_code = 1;
    shutdown_requested = true;
    return;
  }

  // .COM entry: CS=DS=ES=SS=PSP_SEG, IP=100h, SP=FFFEh, AX=0
  SegSet16(cs, PSP_SEG);
  SegSet16(ds, PSP_SEG);
  SegSet16(es, PSP_SEG);
  SegSet16(ss, PSP_SEG);
  reg_eip = COM_ENTRY_OFFSET;
  reg_sp  = 0xFFFE;
  reg_ax  = 0;

  DOSBOX_RunMachine();
}

} // namespace

int run_com(const char *program, const char *const *args, size_t nargs,
            bool headless, int verbose) {
  s_program   = program;
  s_args.clear();
  for (size_t i = 0; i < nargs; ++i) s_args.emplace_back(args[i]);
  s_exit_code = 0;
  s_handles.clear();

  // Default drive mount: C: = process CWD.  cfg.drives from .cfg files will
  // override / extend this in a later iteration.
  char cwd[4096];
  if (getcwd(cwd, sizeof(cwd))) s_drives['C'] = cwd;
  else                          s_drives['C'] = ".";
  s_current_drive = 'C';

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
