//
// hardware.cc — AdLib/OPL FM + PC speaker + Sound Blaster (DSP/DMA) + a minimal
// 8259 PIC, on the emu88 backend.
//
// The compat shim's EmuCpu routes guest I/O here. Audio devices render PCM that
// the host pulls via audio_render(); the Sound Blaster also raises a block-end
// IRQ onto the PIC, which the CPU run loop drains (take_pending_irq_vector()).
//
#include "hardware.h"
#include "../emu88/opl.h"
#include "../emu88/pc_speaker.h"
#include "../emu88/sound_blaster.h"
#include "../emu88/emu88_mem.h"
#include "compat/dosbox_compat.h"   // dosiz_compat::machine_mem()

#include <algorithm>
#include <atomic>
#include <vector>

namespace dosiz {
namespace hardware {
namespace {

OPL          *g_opl = nullptr;   // AdLib (OPL2) at 0x388/0x389
PCSpeaker    *g_spk = nullptr;   // PC speaker (PIT ch2 + port 0x61)
SoundBlaster *g_sb  = nullptr;   // Sound Blaster (0x220 base, IRQ5, DMA1/5)

// PIT channel 2 (PC-speaker frequency) + the 0x61 gate.
uint16_t pit2_reload = 0;
int      pit2_access = 3;        // access mode latched from a 0x43 control word
int      pit2_phase  = 0;        // 0 = next byte low, 1 = next high (mode 3)
uint8_t  port61      = 0;

// Minimal 8259 PIC: just the interrupt-mask registers (a write to 0x21/0xA1)
// and a bitmask of raised-but-undelivered hardware IRQs. The SB raises IRQs
// from the audio thread, so hw_irq_pending is volatile. dosiz's own timer tick
// is delivered directly (not through this PIC), so the IMRs only gate device
// IRQs — they default to all-masked and the guest unmasks what it services.
uint8_t           pic_imr_master = 0xFF;   // port 0x21
uint8_t           pic_imr_slave  = 0xFF;   // port 0xA1
// Set from the SDL audio thread (SB raise_irq) and cleared from the CPU thread
// (take_pending_irq_vector). Atomic so the set|clear read-modify-writes can't
// race and drop an IRQ when a host audio device is driving the SB.
std::atomic<unsigned> hw_irq_pending{0};   // bit i set => IRQ i awaiting delivery

// A host audio device is pulling audio_render(). Written once in run_program
// before the run loop and read only from audio_tick() — both on the CPU/main
// thread (audio_tick is a fire_ticks handler), so this needs no synchronisation.
bool g_host_audio = false;

// Joystick / game port. The emulated game port exists by default (centred,
// no buttons) so INT 15h AH=84h succeeds; SDL overrides with a real stick.
int      joy_axis[4]    = {0, 0, 0, 0};  // pot positions, -32768..32767 (0 = centre)
unsigned joy_btn        = 0;             // bit j set => button j pressed
bool     joy_present    = true;
int      joy_oneshot[4] = {0, 0, 0, 0};  // game-port monostable countdowns

// Map a pot position to a resistive "count": the value INT 15h AH=84h returns
// and the number of high reads the game-port monostable yields. Centre ~ 506.
int axis_count(int a) {
  double norm = (joy_axis[a] + 32768) / 65535.0;   // 0..1
  if (norm < 0) norm = 0; if (norm > 1) norm = 1;
  return 6 + (int)(norm * 1000.0);
}

int  spk_freq() { return pit2_reload ? (int)(1193182u / pit2_reload) : 0; }
bool spk_gate() { return (port61 & 0x03) == 0x03; }  // gate + data-enable

bool irq_masked(int irq) {
  if (irq < 8) return (pic_imr_master & (1u << irq)) != 0;
  // IRQ 8-15 on the slave: masked if the slave bit OR the cascade (IRQ2) is set.
  return (pic_imr_slave & (1u << (irq - 8))) != 0 || (pic_imr_master & 0x04) != 0;
}

// IRQ -> real-mode vector: IRQ 0-7 -> INT 08h-0Fh, IRQ 8-15 -> INT 70h-77h.
int irq_vector(int irq) { return irq < 8 ? 0x08 + irq : 0x70 + (irq - 8); }

} // namespace

void init() {
  if (!g_opl) g_opl = new OPL(false);
  if (!g_spk) g_spk = new PCSpeaker();
  if (!g_sb)  g_sb  = new SoundBlaster(0x220, 5, 1, 5);
  g_sb->mem_read  = [](uint32_t a) -> uint8_t {
    emu88_mem *m = dosiz_compat::machine_mem();
    return m ? m->fetch_mem(a) : 0;
  };
  g_sb->raise_irq = [](int irq) {
    hw_irq_pending.fetch_or(1u << (irq & 15), std::memory_order_release);
  };
  g_opl->reset();
  g_spk->reset();
  g_sb->reset();
  pit2_reload = 0; pit2_access = 3; pit2_phase = 0; port61 = 0;
  pic_imr_master = 0xFF; pic_imr_slave = 0xFF;
  hw_irq_pending.store(0, std::memory_order_relaxed);
}

void shutdown() {
  delete g_opl; g_opl = nullptr;
  delete g_spk; g_spk = nullptr;
  delete g_sb;  g_sb  = nullptr;
}

void set_host_audio(bool on) { g_host_audio = on; }

void port_out(uint16_t port, uint8_t val) {
  // AdLib / OPL FM.
  if (g_opl && (port == 0x388 || port == 0x389)) { g_opl->write_port(port - 0x388, val); return; }

  // Sound Blaster register file (0x220-0x22F) and its 8237 DMA channels.
  if (g_sb) {
    if (port >= 0x220 && port <= 0x22F) { g_sb->write_port(port - 0x220, val); return; }
    if (port <= 0x0F || (port >= 0x80 && port <= 0x8F) || (port >= 0xC0 && port <= 0xDF)) {
      g_sb->dma_write(port, val); return;
    }
  }

  // Game port: a write fires the four axis monostables. Each axis then reads
  // back high for axis_count(a) reads (proportional to the pot position).
  if (port == 0x201) {
    for (int a = 0; a < 4; a++) joy_oneshot[a] = axis_count(a);
    return;
  }

  switch (port) {
  case 0x20:  // PIC master command (EOI etc.) — accepted; priorities unmodeled.
  case 0xA0:  // PIC slave command
    return;
  case 0x21:  pic_imr_master = val; return;   // PIC master IMR
  case 0xA1:  pic_imr_slave  = val; return;   // PIC slave IMR
  case 0x43:  // PIT control word
    if ((val >> 6) == 2) { pit2_access = (val >> 4) & 3; pit2_phase = 0; }
    return;
  case 0x42:  // PIT channel 2 reload
    if (pit2_access == 1)       pit2_reload = val;                          // lobyte only
    else if (pit2_access == 2)  pit2_reload = (uint16_t)(val << 8);         // hibyte only
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
  if (g_sb) {
    if (port >= 0x220 && port <= 0x22F) { *out = g_sb->read_port(port - 0x220); return true; }
    if (port <= 0x0F || (port >= 0x80 && port <= 0x8F) || (port >= 0xC0 && port <= 0xDF)) {
      *out = g_sb->dma_read(port); return true;
    }
  }
  // Game port: bits 0-3 = axis monostables (high while counting, then drop),
  // bits 4-7 = buttons (0 = pressed, 1 = released). 0xFF when no game port.
  if (port == 0x201) {
    if (!joy_present) { *out = 0xFF; return true; }
    uint8_t b = 0;
    for (int a = 0; a < 4; a++)
      if (joy_oneshot[a] > 0) { b |= (uint8_t)(1 << a); joy_oneshot[a]--; }
    for (int j = 0; j < 4; j++)
      if (!(joy_btn & (1u << j))) b |= (uint8_t)(1 << (4 + j));  // released => high
    *out = b;
    return true;
  }

  switch (port) {
  case 0x21:  *out = pic_imr_master; return true;
  case 0xA1:  *out = pic_imr_slave;  return true;
  case 0x61:  *out = port61;         return true;
  default:    return false;
  }
}

void set_joystick(const int *axes, unsigned buttons, bool present) {
  for (int a = 0; a < 4; a++) joy_axis[a] = axes[a];
  joy_btn     = buttons;
  joy_present = present;
}

bool     joystick_present()         { return joy_present; }
unsigned joystick_buttons()         { return joy_btn; }
int      joystick_axis_count(int a) { return (a >= 0 && a < 4) ? axis_count(a) : 0; }

int take_pending_irq_vector() {
  unsigned pending = hw_irq_pending.load(std::memory_order_acquire);
  if (!pending) return -1;
  for (int irq = 0; irq < 16; irq++) {
    if ((pending & (1u << irq)) && !irq_masked(irq)) {
      hw_irq_pending.fetch_and(~(1u << irq), std::memory_order_acq_rel);
      return irq_vector(irq);
    }
  }
  return -1;
}

void audio_render(int16_t *out, int frames, int rate) {
  static std::vector<int32_t> mix;
  const size_t n = (size_t)frames * 2;
  if (mix.size() < n) mix.resize(n);
  std::fill(mix.begin(), mix.begin() + n, 0);
  if (g_opl) g_opl->render(mix.data(), frames, rate);
  if (g_spk) { g_spk->set_tone(spk_freq(), spk_gate()); g_spk->render(mix.data(), frames, rate); }
  if (g_sb)  g_sb->render(mix.data(), frames, rate);
  for (size_t i = 0; i < n; i++) {
    int32_t s = mix[i];
    if (s >  32767) s =  32767;
    if (s < -32768) s = -32768;
    out[i] = (int16_t)s;
  }
}

void audio_tick() {
  // Headless only: when no host audio device is pulling, advance an active SB
  // DMA transfer so its block-end IRQ still fires. Rendering into a discard
  // buffer consumes guest PCM via the DMA and raises the IRQ at block end.
  if (g_host_audio || !g_sb || !g_sb->is_playing()) return;
  static std::vector<int16_t> discard;
  const int rate = 44100, frames = rate / 64;   // ~one tick's worth
  if ((int)discard.size() < frames * 2) discard.resize((size_t)frames * 2);
  audio_render(discard.data(), frames, rate);
}

} // namespace hardware
} // namespace dosiz
