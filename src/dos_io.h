#ifndef DOS_IO_H
#define DOS_IO_H

#include <cstdint>
#include <cstddef>

// Abstract I/O interface for the DOS machine.
// Platform code (iOS, macOS, CLI) implements this.

class dos_io {
public:
  virtual ~dos_io() = default;

  // Console I/O
  virtual void console_write(uint8_t ch) = 0;
  virtual bool console_has_input() = 0;
  virtual int console_read() = 0;  // returns -1 if no input

  // Video display
  virtual void video_mode_changed(int mode, int cols, int rows) = 0;
  // vram points to character/attribute pairs (2 bytes per cell)
  virtual void video_refresh(const uint8_t *vram, int cols, int rows) = 0;
  virtual void video_set_cursor(int row, int col) = 0;

  // Disk I/O (drive: 0=A, 1=B, 0x80=C, 0x81=D, 0xE0=CD-ROM)
  virtual bool disk_present(int drive) = 0;
  virtual size_t disk_read(int drive, uint64_t offset,
                           uint8_t *buf, size_t count) = 0;
  virtual size_t disk_write(int drive, uint64_t offset,
                            const uint8_t *buf, size_t count) = 0;
  virtual uint64_t disk_size(int drive) = 0;
  virtual int disk_sector_size(int drive) { (void)drive; return 512; }

  // Time
  virtual void get_time(int &hour, int &min, int &sec, int &hundredths) = 0;
  virtual void get_date(int &year, int &month, int &day, int &weekday) = 0;

  // Speaker (optional)
  virtual void speaker_beep(int freq_hz, int duration_ms) {
    (void)freq_hz; (void)duration_ms;
  }

  // Mouse input from host (optional)
  // Returns current mouse state: x (0-639), y (0-199), buttons (bit0=L, bit1=R, bit2=M)
  virtual void mouse_get_state(int &x, int &y, int &buttons) {
    x = 0; y = 0; buttons = 0;
  }
  virtual bool mouse_present() { return false; }
};

#endif // DOS_IO_H
