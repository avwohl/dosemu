//
// emu_bridge.cc — dosiz bridge over the emu88 backend (replaces the DOSBox
// bridge). Implements the same dosiz::bridge interface (bridge.h) so dosiz.cc
// is unchanged.
//
// emu88 provides the CPU + BIOS + DPMI host + PC hardware in C++, and runs a
// real DOS kernel by booting FreeDOS/MS-DOS from a disk image. dosiz therefore:
//   * boots emu88 with a FreeDOS hard-disk image,
//   * maps the host stdio onto the guest console,
//   * (Phase 2) presents the host working directory as a DOS drive and
//     auto-runs cfg.program, then captures the INT 21h AH=4Ch exit code.
//
// This Phase-1 file stands up the emu88 machine + host I/O and the build; the
// host-filesystem mapping and program injection are the next phase (marked
// TODO(hostfs) / TODO(exec) below).
//

#include "bridge.h"

#include "../emu88/dos_machine.h"
#include "../emu88/dos_io.h"
#include "../emu88/emu88_mem.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <memory>
#include <string>

namespace dosiz::bridge {

namespace {

// A raw disk image backing a DOS drive (the FreeDOS boot image, and later a
// host-directory-backed FAT image).
struct DiskImage {
    int      fd   = -1;
    uint64_t size = 0;
    bool     writable = false;
    bool open(const std::string &path, bool rw) {
        fd = ::open(path.c_str(), rw ? O_RDWR : O_RDONLY);
        if (fd < 0 && rw) fd = ::open(path.c_str(), O_RDONLY);  // fall back read-only
        if (fd < 0) return false;
        writable = rw && fd >= 0;
        off_t end = ::lseek(fd, 0, SEEK_END);
        size = end > 0 ? (uint64_t)end : 0;
        return true;
    }
    ~DiskImage() { if (fd >= 0) ::close(fd); }
};

// dos_io implementation for a headless command-line host: guest console maps to
// the process stdio, drive 0x80 (C:) maps to the FreeDOS image.
class CliDosIO : public dos_io {
public:
    DiskImage hdd;          // drive 0x80
    int verbose = 0;

    // ---- console ----
    void console_write(uint8_t ch) override {
        ::fputc(ch, stdout);
    }
    bool console_has_input() override {
        // Non-blocking peek at host stdin.
        if (!stdin_nonblock_) {
            int fl = ::fcntl(0, F_GETFL, 0);
            ::fcntl(0, F_SETFL, fl | O_NONBLOCK);
            stdin_nonblock_ = true;
        }
        if (have_peek_) return true;
        unsigned char c;
        ssize_t n = ::read(0, &c, 1);
        if (n == 1) { peek_ = c; have_peek_ = true; return true; }
        return false;
    }
    int console_read() override {
        if (have_peek_) { have_peek_ = false; return peek_; }
        if (console_has_input()) { have_peek_ = false; return peek_; }
        return -1;
    }

    // ---- video (headless: ignore) ----
    void video_mode_changed(int, int, int) override {}
    void video_refresh(const uint8_t *, int, int) override {}
    void video_set_cursor(int, int) override {}

    // ---- disk (drive 0x80 = the FreeDOS image) ----
    bool disk_present(int drive) override { return drive == 0x80 && hdd.fd >= 0; }
    uint64_t disk_size(int drive) override { return drive == 0x80 ? hdd.size : 0; }
    size_t disk_read(int drive, uint64_t off, uint8_t *buf, size_t n) override {
        if (drive != 0x80 || hdd.fd < 0) return 0;
        ssize_t r = ::pread(hdd.fd, buf, n, (off_t)off);
        return r > 0 ? (size_t)r : 0;
    }
    size_t disk_write(int drive, uint64_t off, const uint8_t *buf, size_t n) override {
        if (drive != 0x80 || hdd.fd < 0 || !hdd.writable) return 0;
        ssize_t w = ::pwrite(hdd.fd, buf, n, (off_t)off);
        return w > 0 ? (size_t)w : 0;
    }

    // ---- time ----
    void get_time(int &h, int &m, int &s, int &hund) override {
        time_t t = ::time(nullptr); struct tm lt; localtime_r(&t, &lt);
        h = lt.tm_hour; m = lt.tm_min; s = lt.tm_sec; hund = 0;
    }
    void get_date(int &y, int &mo, int &d, int &wd) override {
        time_t t = ::time(nullptr); struct tm lt; localtime_r(&t, &lt);
        y = lt.tm_year + 1900; mo = lt.tm_mon + 1; d = lt.tm_mday; wd = lt.tm_wday;
    }

private:
    bool stdin_nonblock_ = false;
    bool have_peek_ = false;
    unsigned char peek_ = 0;
};

// Locate a FreeDOS boot image: cfg.drives 'C' host path if it points at an
// image, else $DOSIZ_FREEDOS, else a couple of conventional locations.
std::string find_boot_image(const Config &cfg) {
    if (const char *e = ::getenv("DOSIZ_FREEDOS")) return e;
    (void)cfg;
    const char *candidates[] = {
        "freedos_hd.img", "fd/freedos_hd.img",
        "../qxDOS/fd/freedos_hd.img", "../qxDOS/fd/freedos_starter.img",
    };
    for (const char *c : candidates)
        if (::access(c, R_OK) == 0) return c;
    return {};
}

} // namespace

const char *dosbox_version() { return "emu88 (in-process 386 + FreeDOS)"; }

int run_program(const Config &cfg) {
    std::string boot = find_boot_image(cfg);
    if (boot.empty()) {
        std::fprintf(stderr,
            "dosiz: no FreeDOS image found (set $DOSIZ_FREEDOS or place freedos_hd.img)\n");
        return -1;
    }

    auto io = std::make_unique<CliDosIO>();
    io->verbose = cfg.verbose;
    if (!io->hdd.open(boot, /*rw=*/true)) {
        std::fprintf(stderr, "dosiz: cannot open boot image %s\n", boot.c_str());
        return -1;
    }

    uint32_t ram_mb = (uint32_t)cfg.memsize_mb;
    if (ram_mb < 16)  ram_mb = 16;     // DPMI / DJGPP headroom
    if (ram_mb > 256) ram_mb = 256;
    auto mem = std::make_unique<emu88_mem>(ram_mb * 1024u * 1024u);

    auto machine = std::make_unique<dos_machine>(mem.get(), io.get());
    dos_machine::Config mc;
    mc.cpu     = emu88::CPU_386;
    mc.display = dos_machine::DISPLAY_VGA;
    mc.mouse_enabled   = false;        // headless / compiler defaults: no hardware
    mc.speaker_enabled = false;
    mc.sound_card      = 0;
    machine->configure(mc);

    if (!machine->boot(0x80)) {
        std::fprintf(stderr, "dosiz: FreeDOS boot failed (%s)\n", boot.c_str());
        return -1;
    }

    // TODO(hostfs): present the host working directory / cfg.drives as a DOS
    //   drive (synthesised FAT image via dos_io::disk_*), so the guest sees the
    //   program and its files.
    // TODO(exec): inject cfg.program + an AUTOEXEC that runs it, and capture the
    //   INT 21h AH=4Ch exit code to return from here. For now we boot to the
    //   FreeDOS shell so the backend swap can be exercised end to end.
    for (;;) {
        bool alive = machine->run_batch(20000);
        if (!alive && !machine->halted) break;   // permanent halt
        if (machine->halted && /* idle HLT */ true) {
            // The shell is idle; without a program to run, stop after boot.
            // (Phase 2 replaces this with exit-code capture.)
            break;
        }
    }
    return 0;
}

} // namespace dosiz::bridge
