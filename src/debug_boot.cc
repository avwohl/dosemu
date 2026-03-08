#include "dos_machine.h"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <vector>

class dos_io_debug : public dos_io {
public:
  int disk_fd = -1;
  uint64_t disk_sz = 0;
  std::vector<uint8_t> last_vram;
  int last_cols = 80, last_rows = 25;

  bool load_disk(const char *path) {
    disk_fd = open(path, O_RDONLY);
    if (disk_fd < 0) { perror(path); return false; }
    disk_sz = lseek(disk_fd, 0, SEEK_END);
    lseek(disk_fd, 0, SEEK_SET);
    return true;
  }

  void console_write(uint8_t ch) override { (void)ch; }
  bool console_has_input() override { return false; }
  int console_read() override { return -1; }
  void video_mode_changed(int mode, int cols, int rows) override {
    (void)mode;
    last_cols = cols;
    last_rows = rows;
  }
  void video_refresh(const uint8_t *vram, int cols, int rows) override {
    last_cols = cols;
    last_rows = rows;
    last_vram.assign(vram, vram + cols * rows * 2);
  }
  void video_set_cursor(int row, int col) override { (void)row; (void)col; }

  bool disk_present(int drive) override { return drive == 0 && disk_fd >= 0; }
  size_t disk_read(int drive, uint64_t offset, uint8_t *buf, size_t count) override {
    if (drive != 0 || disk_fd < 0) return 0;
    if (lseek(disk_fd, offset, SEEK_SET) < 0) return 0;
    ssize_t n = ::read(disk_fd, buf, count);
    return n > 0 ? (size_t)n : 0;
  }
  size_t disk_write(int, uint64_t, const uint8_t*, size_t) override { return 0; }
  uint64_t disk_size(int drive) override { return drive == 0 ? disk_sz : 0; }
  void get_time(int &h, int &m, int &s, int &hs) override { h=12;m=0;s=0;hs=0; }
  void get_date(int &y, int &m, int &d, int &w) override { y=2026;m=3;d=8;w=0; }

  void dump_screen() {
    if (last_vram.empty()) return;
    fprintf(stderr, "=== Screen (%dx%d) ===\n", last_cols, last_rows);
    for (int r = 0; r < last_rows; r++) {
      std::string line;
      for (int c = 0; c < last_cols; c++) {
        uint8_t ch = last_vram[(r * last_cols + c) * 2];
        line += (ch >= 0x20 && ch < 0x7F) ? (char)ch : ' ';
      }
      // Trim trailing spaces
      while (!line.empty() && line.back() == ' ') line.pop_back();
      if (!line.empty())
        fprintf(stderr, "%s\n", line.c_str());
      else
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "=== END ===\n");
  }
};

class dos_machine_debug : public dos_machine {
public:
  dos_machine_debug(emu88_mem *m, dos_io *io) : dos_machine(m, io) {}

  void unimplemented_opcode(emu88_uint8 opcode) override {
    if (opcode != 0xF1) {
      fprintf(stderr, "[UNDEF] opcode=0x%02X at %04X:%04X\n",
              opcode, sregs[seg_CS], ip - 1);
    }
    dos_machine::unimplemented_opcode(opcode);
  }
};

int main(int argc, char *argv[]) {
  const char *img = "fd/freedos.img";
  if (argc > 1) img = argv[1];

  dos_io_debug io;
  if (!io.load_disk(img)) return 1;

  emu88_mem mem(0x1000000);  // 16MB: 1MB conventional + 15MB extended
  dos_machine_debug machine(&mem, &io);

  if (!machine.boot(0)) return 1;

  for (int batch = 0; batch < 5000; batch++) {
    if (!machine.run_batch(100000)) break;
    if (machine.is_waiting_for_key()) break;
  }

  io.dump_screen();
  return 0;
}
