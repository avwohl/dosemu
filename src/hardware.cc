//
// hardware.cc — AdLib/OPL FM + PC speaker (Sound Blaster added later).
//
#include "hardware.h"
#include "../emu88/opl.h"
#include "../emu88/pc_speaker.h"

#include <algorithm>
#include <vector>

namespace dosiz {
namespace hardware {
namespace {

OPL       *g_opl = nullptr;     // AdLib (OPL2) at 0x388/0x389
PCSpeaker *g_spk = nullptr;     // PC speaker (PIT ch2 + port 0x61)

// PIT channel 2 (drives the PC speaker frequency) + the 0x61 gate.
uint16_t pit2_reload = 0;
int      pit2_access = 3;       // access mode latched from a 0x43 control word
int      pit2_phase  = 0;       // 0 = next byte is low, 1 = next is high (mode 3)
uint8_t  port61      = 0;

int  spk_freq() { return pit2_reload ? (int)(1193182u / pit2_reload) : 0; }
bool spk_gate() { return (port61 & 0x03) == 0x03; }  // gate + data-enable

} // namespace

void init() {
  if (!g_opl) g_opl = new OPL(false);
  if (!g_spk) g_spk = new PCSpeaker();
  g_opl->reset();
  g_spk->reset();
  pit2_reload = 0; pit2_access = 3; pit2_phase = 0; port61 = 0;
}

void shutdown() {
  delete g_opl; g_opl = nullptr;
  delete g_spk; g_spk = nullptr;
}

void port_out(uint16_t port, uint8_t val) {
  if (g_opl && (port == 0x388 || port == 0x389)) { g_opl->write_port(port - 0x388, val); return; }
  switch (port) {
  case 0x43:  // PIT control word
    if ((val >> 6) == 2) { pit2_access = (val >> 4) & 3; pit2_phase = 0; }
    return;
  case 0x42:  // PIT channel 2 reload
    if (pit2_access == 1)       pit2_reload = val;                         // lobyte only
    else if (pit2_access == 2)  pit2_reload = (uint16_t)(val << 8);        // hibyte only
    else if (pit2_phase == 0) { pit2_reload = (uint16_t)((pit2_reload & 0xFF00) | val); pit2_phase = 1; }
    else                      { pit2_reload = (uint16_t)((pit2_reload & 0x00FF) | (val << 8)); pit2_phase = 0; }
    return;
  case 0x61:  // speaker control (bit0 = timer-2 gate, bit1 = data enable)
    port61 = val;
    return;
  default:
    return;
  }
}

bool port_in(uint16_t port, uint8_t *out) {
  if (g_opl && (port == 0x388 || port == 0x389)) { *out = g_opl->read_port(port - 0x388); return true; }
  if (port == 0x61) { *out = port61; return true; }
  return false;
}

void audio_render(int16_t *out, int frames, int rate) {
  static std::vector<int32_t> mix;
  const size_t n = (size_t)frames * 2;
  if (mix.size() < n) mix.resize(n);
  std::fill(mix.begin(), mix.begin() + n, 0);
  if (g_opl) g_opl->render(mix.data(), frames, rate);
  if (g_spk) { g_spk->set_tone(spk_freq(), spk_gate()); g_spk->render(mix.data(), frames, rate); }
  for (size_t i = 0; i < n; i++) {
    int32_t s = mix[i];
    if (s >  32767) s =  32767;
    if (s < -32768) s = -32768;
    out[i] = (int16_t)s;
  }
}

} // namespace hardware
} // namespace dosiz
