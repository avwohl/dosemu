//
// video.h — dosiz VGA video state + software renderer (text + mode 13h).
//
// dosiz emulates the video BIOS itself (INT 10h in bridge.cc); this module
// holds the video mode + DAC palette and turns the guest's VGA memory
// (0xB8000 text / 0xA0000 mode 13h, in emu88_mem) into an RGBA frame for the
// display module to blit. No SDL here — pure logic, always compiled.
//
#ifndef DOSIZ_VIDEO_H
#define DOSIZ_VIDEO_H

#include <cstdint>

namespace dosiz {
namespace video {

// Reset to mode 3 (80x25 text) with the default VGA palette.
void init();

// INT 10h AH=00: select a video mode and program the emu88 VGA memory flags
// (planar/SVGA off, page frame cleared). Supports the common text modes and
// mode 0x13 (320x200x256); other modes fall back to text.
void set_mode(int mode);
int  current_mode();

// Pixel dimensions of the current mode's rendered frame.
void frame_dims(int *w, int *h);

// VGA DAC palette. Guest programs set it via I/O ports 0x3C6-0x3C9 (captured
// from the CPU port_out path) or INT 10h AH=10 (set_dac). Values are 6-bit.
void    port_out(uint16_t port, uint8_t val);
uint8_t port_in(uint16_t port);
void    set_dac(int index, uint8_t r, uint8_t g, uint8_t b);

// Render the current frame into `out` as ARGB8888 (one uint32 per pixel,
// 0xAARRGGBB). `out` must hold at least frame_dims()'s w*h pixels. Returns the
// dims via *w,*h. Always succeeds for supported modes.
void render(uint32_t *out, int *w, int *h);

// Render the current frame and write it to `path` as a binary PPM (P6). For
// verifying output without a window. Returns false on file error.
bool dump_ppm(const char *path);

// ---- VESA / VBE (SVGA) -----------------------------------------------------
// The INT 10h AH=4F handler (bridge.cc) drives these; the renderer composites
// the SVGA framebuffer (emu88_mem::svga_vram) at the active mode's depth.
struct VesaMode { uint16_t num; uint16_t w; uint16_t h; uint8_t bpp; };

int              vesa_mode_count();
const VesaMode  *vesa_mode_at(int i);
const VesaMode  *vesa_find(uint16_t num);          // strips the LFB/no-clear bits
bool             vesa_active();
uint16_t         vesa_mode_number();               // active mode (0 if none)
void             vesa_get(int *w, int *h, int *bpp, int *stride, uint32_t *display_start);
void             vesa_set_scanline_bytes(int bytes);
void             vesa_set_display_start(uint32_t byte_off);
void             vesa_set_window(uint32_t byte_off);   // 4F05 bank window
uint32_t         vesa_get_window();
uint32_t         vesa_vram_total();                // advertised VRAM (bytes)
uint32_t         vesa_lfb_phys();                  // LFB aperture physical base
int              vesa_bpp_bytes(int bpp);
bool             vesa_set_mode(uint16_t modenum);  // activate: VRAM + emu88 flags + state

} // namespace video
} // namespace dosiz

#endif // DOSIZ_VIDEO_H
