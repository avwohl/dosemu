//
// display.h — thin host-window interface for dosiz --window.
//
// Implemented in display.cc over SDL2, which is an OPTIONAL build dependency:
// display.cc is compiled only when SDL2 is found (HAVE_SDL2). No SDL types leak
// through this header, so bridge.cc can include it unconditionally and guard
// the calls with #ifdef HAVE_SDL2.
//
#ifndef DOSIZ_DISPLAY_H
#define DOSIZ_DISPLAY_H

#include <cstdint>

namespace dosiz {
namespace display {

// Key-press callback: ascii (0 if non-printable) + DOS scancode.
using KeyCallback = void (*)(uint8_t ascii, uint8_t scancode);

bool open(const char *title, KeyCallback on_key);  // create + show a window
void close();

// Blit an ARGB8888 frame (0xAARRGGBB per pixel, row-major, w*h pixels). The
// window scales the logical wxh to its current size.
void present(const uint32_t *argb, int w, int h);

// Pump host events (keyboard -> on_key, window close -> request_quit).
void pump_events();

// True once the user closed the window (or pressed the quit chord).
bool quit_requested();

} // namespace display
} // namespace dosiz

#endif // DOSIZ_DISPLAY_H
