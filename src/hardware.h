//
// hardware.h — optional PC sound hardware on the emu88 backend.
//
// Owns the AdLib/OPL FM synthesizer, the PC speaker, and the Sound Blaster
// (DSP + DMA digital audio). The CPU's port_out/port_in (the compat shim's
// EmuCpu) routes the device I/O ports here; the host audio thread pulls
// rendered PCM via audio_render(). With no host audio the devices are inert
// (nothing pulls), except that the run loop advances an active SB transfer so
// its block-end IRQ still fires headlessly.
//
#ifndef DOSIZ_HARDWARE_H
#define DOSIZ_HARDWARE_H

#include <cstdint>

namespace dosiz {
namespace hardware {

void init();      // create + reset the devices
void shutdown();

// I/O ports. port_out handles the device ports (AdLib 0x388/9, PIT ch2 0x42/3,
// speaker 0x61, the 8259 PIC 0x20/0x21/0xA0/0xA1, the Sound Blaster 0x220-0x22F
// and its 8237 DMA channels); port_in returns true + *out if it owns the port.
void port_out(uint16_t port, uint8_t val);
bool port_in(uint16_t port, uint8_t *out);

// Mix all devices into `out` (interleaved stereo int16) at `rate` Hz. Called on
// the host audio thread. Safe to call when no program is making sound (silent).
void audio_render(int16_t *out, int frames, int rate);

// Hardware IRQ delivery. Devices (the Sound Blaster) raise IRQs onto an 8259
// PIC model from the audio thread; the CPU run loop drains them. Returns the
// real-mode interrupt vector (0x08-0x0F for IRQ 0-7, 0x70-0x77 for IRQ 8-15) of
// the highest-priority pending *unmasked* IRQ and clears it, or -1 if none. The
// caller must hold IF=1 (the PIC does not gate on IF).
int take_pending_irq_vector();

// Tell the layer whether a host audio device is pulling audio_render() (window
// mode). When false (headless), the run loop drives an active SB transfer via
// audio_tick() so DMA progresses and the block-end IRQ fires.
void set_host_audio(bool on);

// Called from the ~60 Hz timer tick. When no host audio is pulling and an SB
// DMA transfer is active, renders a slice into a discard buffer to advance the
// transfer (and raise its IRQ at block end). No-op otherwise.
void audio_tick();

// Joystick / analog game port (0x201) + INT 15h AH=84h. The host (SDL) pushes
// the current stick state via set_joystick(); the game port and the BIOS read
// it back. axes[4] are the four pot positions (-32768..32767, 0 = centred);
// buttons has bit j set when button j is pressed. The compat shim's EmuCpu
// routes port 0x201 here; bridge.cc's INT 15h AH=84h uses the accessors.
void     set_joystick(const int *axes, unsigned buttons, bool present);
bool     joystick_present();
unsigned joystick_buttons();          // bit j set => button j pressed
int      joystick_axis_count(int a);  // resistive "count" for axis a (0..3)

} // namespace hardware
} // namespace dosiz

#endif // DOSIZ_HARDWARE_H
