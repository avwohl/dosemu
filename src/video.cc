//
// video.cc — dosiz VGA video state + software renderer.
//
// See video.h. Reads the guest's VGA memory through the emu88 backend
// (mem_readb / the emu88_mem flags) and composites an ARGB frame. The 8x16
// font and the default DAC palette are taken from emu88 so colours match the
// rest of the project.
//
#include "video.h"
#include "compat/dosbox_compat.h"
#include "emu88_mem.h"

#include <cstdio>
#include <cstring>
#include <vector>

// emu88's text-mode font: vga_font_8x16[codepoint][row], MSB = leftmost pixel.
#include "vga_font_8x16.h"

namespace dosiz {
namespace video {
namespace {

struct VgaState {
  int     mode = 0x03;            // current INT 10h mode number
  uint8_t dac[256][3] = {};       // DAC palette, 6-bit per channel
  uint8_t dac_widx = 0;           // 0x3C8 write index
  uint8_t dac_comp = 0;           // which of R/G/B is next on 0x3C9
  uint8_t dac_ridx = 0;           // 0x3C7 read index
  uint8_t dac_rcomp = 0;
  uint8_t pel_mask = 0xFF;        // 0x3C6
};
VgaState g;

// ---- VESA / VBE state ------------------------------------------------------
constexpr uint32_t SVGA_LFB_PHYS    = 0xE0000000u;   // LFB aperture (PhysBasePtr)
constexpr uint32_t SVGA_VRAM_TOTAL  = 8u * 1024 * 1024;  // advertised VRAM

const VesaMode kVesaModes[] = {
  {0x100, 640, 400, 8}, {0x101, 640, 480, 8}, {0x103, 800, 600, 8},
  {0x105,1024, 768, 8}, {0x107,1280,1024, 8},
  {0x10D, 320, 200,15}, {0x110, 640, 480,15}, {0x113, 800, 600,15},
  {0x116,1024, 768,15}, {0x119,1280,1024,15},
  {0x10E, 320, 200,16}, {0x111, 640, 480,16}, {0x114, 800, 600,16},
  {0x117,1024, 768,16}, {0x11A,1280,1024,16},
  {0x10F, 320, 200,24}, {0x112, 640, 480,24}, {0x115, 800, 600,24},
  {0x118,1024, 768,24}, {0x11B,1280,1024,24},
  {0x140, 320, 200,32}, {0x141, 640, 480,32}, {0x142, 800, 600,32},
  {0x143,1024, 768,32}, {0x144,1280,1024,32},
};
constexpr int kNumVesaModes = (int)(sizeof(kVesaModes) / sizeof(kVesaModes[0]));

struct VesaState {
  bool     active = false;
  uint16_t mode = 0;
  int      xres = 0, yres = 0, bpp = 0;
  int      stride = 0;            // bytes per scan line
  uint32_t display_start = 0;     // byte offset (4F07 pan)
};
VesaState vs;

void init_default_palette(uint8_t dac[][3]) {
  static const uint8_t cga16[16][3] = {
    { 0, 0, 0}, { 0, 0,42}, { 0,42, 0}, { 0,42,42},
    {42, 0, 0}, {42, 0,42}, {42,21, 0}, {42,42,42},
    {21,21,21}, {21,21,63}, {21,63,21}, {21,63,63},
    {63,21,21}, {63,21,63}, {63,63,21}, {63,63,63}
  };
  for (int i = 0; i < 16; i++)
    for (int c = 0; c < 3; c++) dac[i][c] = cga16[i][c];
  for (int i = 0; i < 16; i++)
    for (int c = 0; c < 3; c++) dac[16 + i][c] = cga16[i][c] / 2;
  int idx = 32;
  for (int r = 0; r < 6; r++)
    for (int gg = 0; gg < 6; gg++)
      for (int b = 0; b < 6; b++)
        if (idx < 248) {
          dac[idx][0] = r * 12 + 3;
          dac[idx][1] = gg * 12 + 3;
          dac[idx][2] = b * 12 + 3;
          idx++;
        }
  for (int i = 0; i < 8; i++) {
    uint8_t v = (uint8_t)(i * 9);
    dac[248 + i][0] = dac[248 + i][1] = dac[248 + i][2] = v;
  }
}

// 6-bit DAC channel -> 8-bit (replicate the high bits for a full-range scale).
inline uint32_t dac_to_argb(const uint8_t c[3]) {
  uint8_t r = (uint8_t)((c[0] << 2) | (c[0] >> 4));
  uint8_t gg = (uint8_t)((c[1] << 2) | (c[1] >> 4));
  uint8_t b = (uint8_t)((c[2] << 2) | (c[2] >> 4));
  return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)gg << 8) | b;
}

bool is_text_mode() { return g.mode != 0x13; }

// Composite the SVGA framebuffer (emu88_mem::svga_vram) at the active VESA
// depth into the ARGB output buffer.
void render_vesa(uint32_t *out, int w, int h) {
  emu88_mem *mem = dosiz_compat::machine_mem();
  const uint8_t *fb = mem ? mem->svga_base() : nullptr;
  if (!fb) { std::memset(out, 0, (size_t)w * h * 4); return; }
  const int bypp = vesa_bpp_bytes(vs.bpp);
  const uint32_t vram = mem->svga_vram_size;
  for (int y = 0; y < h; y++) {
    const uint32_t base = vs.display_start + (uint32_t)y * (uint32_t)vs.stride;
    uint32_t *line = out + (size_t)y * w;
    for (int x = 0; x < w; x++) {
      const uint32_t o = base + (uint32_t)x * bypp;
      if (o + bypp > vram) { line[x] = 0xFF000000u; continue; }
      const uint8_t *px = fb + o;
      uint32_t argb;
      switch (vs.bpp) {
      case 8:
        argb = dac_to_argb(g.dac[px[0]]);
        break;
      case 15: {  // RGB555
        uint16_t v = (uint16_t)(px[0] | (px[1] << 8));
        uint8_t r = (v >> 10) & 0x1F, gg = (v >> 5) & 0x1F, b = v & 0x1F;
        argb = 0xFF000000u | ((uint32_t)((r << 3) | (r >> 2)) << 16)
             | ((uint32_t)((gg << 3) | (gg >> 2)) << 8) | ((b << 3) | (b >> 2));
        break;
      }
      case 16: {  // RGB565
        uint16_t v = (uint16_t)(px[0] | (px[1] << 8));
        uint8_t r = (v >> 11) & 0x1F, gg = (v >> 5) & 0x3F, b = v & 0x1F;
        argb = 0xFF000000u | ((uint32_t)((r << 3) | (r >> 2)) << 16)
             | ((uint32_t)((gg << 2) | (gg >> 4)) << 8) | ((b << 3) | (b >> 2));
        break;
      }
      default:    // 24bpp / 32bpp: little-endian B,G,R[,X]
        argb = 0xFF000000u | ((uint32_t)px[2] << 16) | ((uint32_t)px[1] << 8) | px[0];
        break;
      }
      line[x] = argb;
    }
  }
}

} // namespace

void init() {
  std::memset(&g, 0, sizeof(g));
  g.mode = 0x03;
  g.pel_mask = 0xFF;
  init_default_palette(g.dac);
  set_mode(0x03);
}

int current_mode() { return g.mode; }

void set_mode(int mode) {
  g.mode = mode;
  vs.active = false;             // a legacy mode set leaves VESA
  emu88_mem *mem = dosiz_compat::machine_mem();
  if (!mem) return;
  // dosiz only drives legacy VGA: turn off any VESA/SVGA window and Mode-X
  // planar routing so the page frame is a plain linear buffer.
  mem->svga_active = false;
  mem->vga_planar  = false;
  mem->vga_map_mask = 0x0F;
  mem->vga_read_map = 0;
  if (mode == 0x13) {
    // 320x200x256 chain-4: clear the linear page frame at 0xA0000 and reset
    // to the default palette (real BIOS reloads the 256-colour DAC here).
    for (uint32_t i = 0; i < 320u * 200u; i++) mem_writeb(0xA0000u + i, 0);
    init_default_palette(g.dac);
  } else {
    // Text mode: clear to spaces on a black background (attr 0x07).
    for (int i = 0; i < 80 * 25; i++) {
      mem_writeb(0xB8000u + i * 2 + 0, ' ');
      mem_writeb(0xB8000u + i * 2 + 1, 0x07);
    }
    init_default_palette(g.dac);
  }
}

void frame_dims(int *w, int *h) {
  if (vs.active)           { *w = vs.xres;  *h = vs.yres; }
  else if (is_text_mode()) { *w = 80 * 8;   *h = 25 * 16; }   // 640x400
  else                     { *w = 320;      *h = 200; }
}

int             vesa_bpp_bytes(int bpp) { return bpp <= 8 ? 1 : bpp <= 16 ? 2 : bpp <= 24 ? 3 : 4; }
int             vesa_mode_count()       { return kNumVesaModes; }
const VesaMode *vesa_mode_at(int i)     { return (i >= 0 && i < kNumVesaModes) ? &kVesaModes[i] : nullptr; }
bool            vesa_active()           { return vs.active; }
uint16_t        vesa_mode_number()      { return vs.active ? vs.mode : 0; }
uint32_t        vesa_vram_total()       { return SVGA_VRAM_TOTAL; }
uint32_t        vesa_lfb_phys()         { return SVGA_LFB_PHYS; }
void            vesa_set_scanline_bytes(int bytes) { if (bytes > 0) vs.stride = bytes; }
void            vesa_set_display_start(uint32_t off) { vs.display_start = off; }
void            vesa_set_window(uint32_t off) { emu88_mem *m = dosiz_compat::machine_mem(); if (m) m->svga_window_off = off; }
uint32_t        vesa_get_window() { emu88_mem *m = dosiz_compat::machine_mem(); return m ? m->svga_window_off : 0; }

const VesaMode *vesa_find(uint16_t num) {
  num &= 0x3FFF;
  for (int i = 0; i < kNumVesaModes; i++)
    if (kVesaModes[i].num == num) return &kVesaModes[i];
  return nullptr;
}

void vesa_get(int *w, int *h, int *bpp, int *stride, uint32_t *ds) {
  if (w) *w = vs.xres; if (h) *h = vs.yres; if (bpp) *bpp = vs.bpp;
  if (stride) *stride = vs.stride; if (ds) *ds = vs.display_start;
}

bool vesa_set_mode(uint16_t modenum) {
  const bool no_clear = (modenum & 0x8000) != 0;   // bit15: preserve framebuffer
  const VesaMode *m = vesa_find(modenum);
  if (!m) return false;
  emu88_mem *mem = dosiz_compat::machine_mem();
  if (!mem) return false;
  mem->svga_ensure(SVGA_VRAM_TOTAL);               // allocate the full advertised VRAM
  if (!no_clear && mem->svga_base()) std::memset(mem->svga_base(), 0, mem->svga_vram_size);
  vs.active = true;
  vs.mode = m->num;
  vs.xres = m->w; vs.yres = m->h; vs.bpp = m->bpp;
  vs.stride = (int)((uint32_t)m->w * vesa_bpp_bytes(m->bpp));
  vs.display_start = 0;
  mem->svga_active = true;
  mem->svga_window_off = 0;
  mem->svga_lfb_phys = SVGA_LFB_PHYS;
  mem->vga_planar = false;
  // Leave g.mode at the prior legacy mode: render() checks vs.active first, and
  // AH=0F (legacy get-mode) shouldn't return a truncated VESA mode number.
  if (m->bpp == 8) init_default_palette(g.dac);
  return true;
}

void set_dac(int index, uint8_t r, uint8_t gg, uint8_t b) {
  if (index < 0 || index > 255) return;
  g.dac[index][0] = r & 0x3F;
  g.dac[index][1] = gg & 0x3F;
  g.dac[index][2] = b & 0x3F;
}

void port_out(uint16_t port, uint8_t val) {
  switch (port) {
  case 0x3C6: g.pel_mask = val; break;
  case 0x3C7: g.dac_ridx = val; g.dac_rcomp = 0; break;
  case 0x3C8: g.dac_widx = val; g.dac_comp = 0; break;
  case 0x3C9:
    g.dac[g.dac_widx][g.dac_comp] = val & 0x3F;
    if (++g.dac_comp >= 3) { g.dac_comp = 0; g.dac_widx++; }
    break;
  default: break;
  }
}

uint8_t port_in(uint16_t port) {
  switch (port) {
  case 0x3C6: return g.pel_mask;
  case 0x3C8: return g.dac_widx;
  case 0x3C9: {
    uint8_t v = g.dac[g.dac_ridx][g.dac_rcomp];
    if (++g.dac_rcomp >= 3) { g.dac_rcomp = 0; g.dac_ridx++; }
    return v;
  }
  default: return 0xFF;
  }
}

void render(uint32_t *out, int *w, int *h) {
  int fw, fh;
  frame_dims(&fw, &fh);
  *w = fw; *h = fh;

  if (vs.active) { render_vesa(out, fw, fh); return; }

  if (is_text_mode()) {
    // Cursor position for the active page lives in BDA 0x40:0x50 (row<<8|col).
    uint16_t cpos = mem_readw(0x450u);
    int cur_row = (cpos >> 8) & 0xFF, cur_col = cpos & 0xFF;
    for (int row = 0; row < 25; row++) {
      for (int col = 0; col < 80; col++) {
        const uint32_t cell = 0xB8000u + (uint32_t)(row * 80 + col) * 2u;
        uint8_t ch   = mem_readb(cell);
        uint8_t attr = mem_readb(cell + 1);
        uint32_t fg = dac_to_argb(g.dac[attr & 0x0F]);
        uint32_t bg = dac_to_argb(g.dac[(attr >> 4) & 0x07]);
        const uint8_t *glyph = vga_font_8x16[ch];
        for (int gy = 0; gy < 16; gy++) {
          uint8_t bits = glyph[gy];
          uint32_t *line = out + (row * 16 + gy) * fw + col * 8;
          for (int gx = 0; gx < 8; gx++)
            line[gx] = (bits & (0x80 >> gx)) ? fg : bg;
        }
        // Block cursor: invert the bottom rows of the cell it sits on.
        if (row == cur_row && col == cur_col) {
          for (int gy = 13; gy < 16; gy++) {
            uint32_t *line = out + (row * 16 + gy) * fw + col * 8;
            for (int gx = 0; gx < 8; gx++) line[gx] = fg;
          }
        }
      }
    }
    return;
  }

  // Mode 13h: 320x200, one palette index per byte at 0xA0000.
  for (int y = 0; y < 200; y++)
    for (int x = 0; x < 320; x++) {
      uint8_t idx = mem_readb(0xA0000u + (uint32_t)(y * 320 + x));
      out[y * 320 + x] = dac_to_argb(g.dac[idx]);
    }
}

bool dump_ppm(const char *path) {
  int w, h;
  frame_dims(&w, &h);
  std::vector<uint32_t> buf((size_t)w * h, 0);
  render(buf.data(), &w, &h);
  FILE *f = std::fopen(path, "wb");
  if (!f) return false;
  std::fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int i = 0; i < w * h; i++) {
    uint32_t p = buf[i];
    uint8_t rgb[3] = { (uint8_t)(p >> 16), (uint8_t)(p >> 8), (uint8_t)p };
    std::fwrite(rgb, 1, 3, f);
  }
  std::fclose(f);
  return true;
}

} // namespace video
} // namespace dosiz
