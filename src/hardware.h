//
// hardware.h — optional PC sound hardware on the emu88 backend.
//
// Owns the AdLib/OPL FM synthesizer, the PC speaker, and (later) the Sound
// Blaster. The CPU's port_out/port_in (the compat shim's EmuCpu) routes the
// device I/O ports here; the host audio thread pulls rendered PCM via
// audio_render(). With no host audio the devices are inert (nothing pulls).
//
#ifndef DOSIZ_HARDWARE_H
#define DOSIZ_HARDWARE_H

#include <cstdint>

namespace dosiz {
namespace hardware {

void init();      // create + reset the devices
void shutdown();

// I/O ports. port_out handles the device ports (AdLib 0x388/9, PIT ch2 0x42/3,
// speaker 0x61, SB later); port_in returns true + *out if it owns the port.
void port_out(uint16_t port, uint8_t val);
bool port_in(uint16_t port, uint8_t *out);

// Mix all devices into `out` (interleaved stereo int16) at `rate` Hz. Called on
// the host audio thread. Safe to call when no program is making sound (silent).
void audio_render(int16_t *out, int frames, int rate);

} // namespace hardware
} // namespace dosiz

#endif // DOSIZ_HARDWARE_H
