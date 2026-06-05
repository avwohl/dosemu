//
// display.cc — SDL2 implementation of the dosiz host window.
//
// Compiled only when SDL2 is available (see src/CMakeLists.txt). Keeps all SDL
// state file-local; exposes the thin interface in display.h.
//
#include "display.h"

#include <SDL.h>
#include <cstdio>
#include <cstring>

namespace dosiz {
namespace display {
namespace {

SDL_Window   *g_window   = nullptr;
SDL_Renderer *g_renderer = nullptr;
SDL_Texture  *g_texture  = nullptr;
int           g_tex_w = 0, g_tex_h = 0;
KeyCallback   g_on_key = nullptr;
MouseCallback g_on_mouse = nullptr;
AudioCallback g_on_audio = nullptr;
SDL_AudioDeviceID g_audio = 0;
JoystickCallback g_on_joystick = nullptr;
SDL_Joystick *g_joystick = nullptr;
bool          g_quit = false;

// SDL scancode -> IBM PC set-1 scancode (the value INT 16h reports in AH).
uint8_t dos_scancode(SDL_Scancode sc) {
  switch (sc) {
  case SDL_SCANCODE_ESCAPE: return 0x01;
  case SDL_SCANCODE_1: return 0x02; case SDL_SCANCODE_2: return 0x03;
  case SDL_SCANCODE_3: return 0x04; case SDL_SCANCODE_4: return 0x05;
  case SDL_SCANCODE_5: return 0x06; case SDL_SCANCODE_6: return 0x07;
  case SDL_SCANCODE_7: return 0x08; case SDL_SCANCODE_8: return 0x09;
  case SDL_SCANCODE_9: return 0x0A; case SDL_SCANCODE_0: return 0x0B;
  case SDL_SCANCODE_MINUS: return 0x0C; case SDL_SCANCODE_EQUALS: return 0x0D;
  case SDL_SCANCODE_BACKSPACE: return 0x0E; case SDL_SCANCODE_TAB: return 0x0F;
  case SDL_SCANCODE_Q: return 0x10; case SDL_SCANCODE_W: return 0x11;
  case SDL_SCANCODE_E: return 0x12; case SDL_SCANCODE_R: return 0x13;
  case SDL_SCANCODE_T: return 0x14; case SDL_SCANCODE_Y: return 0x15;
  case SDL_SCANCODE_U: return 0x16; case SDL_SCANCODE_I: return 0x17;
  case SDL_SCANCODE_O: return 0x18; case SDL_SCANCODE_P: return 0x19;
  case SDL_SCANCODE_LEFTBRACKET: return 0x1A; case SDL_SCANCODE_RIGHTBRACKET: return 0x1B;
  case SDL_SCANCODE_RETURN: return 0x1C; case SDL_SCANCODE_LCTRL: return 0x1D;
  case SDL_SCANCODE_A: return 0x1E; case SDL_SCANCODE_S: return 0x1F;
  case SDL_SCANCODE_D: return 0x20; case SDL_SCANCODE_F: return 0x21;
  case SDL_SCANCODE_G: return 0x22; case SDL_SCANCODE_H: return 0x23;
  case SDL_SCANCODE_J: return 0x24; case SDL_SCANCODE_K: return 0x25;
  case SDL_SCANCODE_L: return 0x26; case SDL_SCANCODE_SEMICOLON: return 0x27;
  case SDL_SCANCODE_APOSTROPHE: return 0x28; case SDL_SCANCODE_GRAVE: return 0x29;
  case SDL_SCANCODE_LSHIFT: return 0x2A; case SDL_SCANCODE_BACKSLASH: return 0x2B;
  case SDL_SCANCODE_Z: return 0x2C; case SDL_SCANCODE_X: return 0x2D;
  case SDL_SCANCODE_C: return 0x2E; case SDL_SCANCODE_V: return 0x2F;
  case SDL_SCANCODE_B: return 0x30; case SDL_SCANCODE_N: return 0x31;
  case SDL_SCANCODE_M: return 0x32; case SDL_SCANCODE_COMMA: return 0x33;
  case SDL_SCANCODE_PERIOD: return 0x34; case SDL_SCANCODE_SLASH: return 0x35;
  case SDL_SCANCODE_RSHIFT: return 0x36; case SDL_SCANCODE_LALT: return 0x38;
  case SDL_SCANCODE_SPACE: return 0x39; case SDL_SCANCODE_CAPSLOCK: return 0x3A;
  case SDL_SCANCODE_F1: return 0x3B; case SDL_SCANCODE_F2: return 0x3C;
  case SDL_SCANCODE_F3: return 0x3D; case SDL_SCANCODE_F4: return 0x3E;
  case SDL_SCANCODE_F5: return 0x3F; case SDL_SCANCODE_F6: return 0x40;
  case SDL_SCANCODE_F7: return 0x41; case SDL_SCANCODE_F8: return 0x42;
  case SDL_SCANCODE_F9: return 0x43; case SDL_SCANCODE_F10: return 0x44;
  case SDL_SCANCODE_HOME: return 0x47; case SDL_SCANCODE_UP: return 0x48;
  case SDL_SCANCODE_PAGEUP: return 0x49; case SDL_SCANCODE_LEFT: return 0x4B;
  case SDL_SCANCODE_RIGHT: return 0x4D; case SDL_SCANCODE_END: return 0x4F;
  case SDL_SCANCODE_DOWN: return 0x50; case SDL_SCANCODE_PAGEDOWN: return 0x51;
  case SDL_SCANCODE_INSERT: return 0x52; case SDL_SCANCODE_DELETE: return 0x53;
  default: return 0x00;
  }
}

// ASCII for the printable keys, honouring shift. Non-printable keys (arrows,
// F-keys, etc.) return 0 — the scancode carries them.
uint8_t dos_ascii(SDL_Keycode k, bool shift) {
  if (k >= SDLK_a && k <= SDLK_z) return (uint8_t)((shift ? 'A' : 'a') + (k - SDLK_a));
  switch (k) {
  case SDLK_RETURN: case SDLK_KP_ENTER: return 0x0D;
  case SDLK_BACKSPACE: return 0x08;
  case SDLK_TAB: return 0x09;
  case SDLK_ESCAPE: return 0x1B;
  case SDLK_SPACE: return ' ';
  }
  if (!shift) {
    if (k >= SDLK_0 && k <= SDLK_9) return (uint8_t)k;
    switch (k) {
    case SDLK_MINUS: return '-'; case SDLK_EQUALS: return '=';
    case SDLK_LEFTBRACKET: return '['; case SDLK_RIGHTBRACKET: return ']';
    case SDLK_BACKSLASH: return '\\'; case SDLK_SEMICOLON: return ';';
    case SDLK_QUOTE: return '\''; case SDLK_BACKQUOTE: return '`';
    case SDLK_COMMA: return ','; case SDLK_PERIOD: return '.'; case SDLK_SLASH: return '/';
    }
  } else {
    switch (k) {
    case SDLK_1: return '!'; case SDLK_2: return '@'; case SDLK_3: return '#';
    case SDLK_4: return '$'; case SDLK_5: return '%'; case SDLK_6: return '^';
    case SDLK_7: return '&'; case SDLK_8: return '*'; case SDLK_9: return '(';
    case SDLK_0: return ')';
    case SDLK_MINUS: return '_'; case SDLK_EQUALS: return '+';
    case SDLK_LEFTBRACKET: return '{'; case SDLK_RIGHTBRACKET: return '}';
    case SDLK_BACKSLASH: return '|'; case SDLK_SEMICOLON: return ':';
    case SDLK_QUOTE: return '"'; case SDLK_BACKQUOTE: return '~';
    case SDLK_COMMA: return '<'; case SDLK_PERIOD: return '>'; case SDLK_SLASH: return '?';
    }
  }
  return 0;
}

void ensure_texture(int w, int h) {
  if (g_texture && g_tex_w == w && g_tex_h == h) return;
  if (g_texture) SDL_DestroyTexture(g_texture);
  g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, w, h);
  g_tex_w = w; g_tex_h = h;
  SDL_RenderSetLogicalSize(g_renderer, w, h);
}

} // namespace

bool open(const char *title, KeyCallback on_key, MouseCallback on_mouse) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::fprintf(stderr, "dosiz: SDL_Init failed: %s\n", SDL_GetError());
    return false;
  }
  // Start at a doubled 640x400 so the window isn't tiny.
  g_window = SDL_CreateWindow(title ? title : "dosiz",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              640, 400, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!g_window) {
    std::fprintf(stderr, "dosiz: SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return false;
  }
  g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
  if (!g_renderer)
    g_renderer = SDL_CreateRenderer(g_window, -1, 0);  // software fallback
  if (!g_renderer) {
    std::fprintf(stderr, "dosiz: SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(g_window); g_window = nullptr; SDL_Quit();
    return false;
  }
  g_on_key = on_key;
  g_on_mouse = on_mouse;
  g_quit = false;
  return true;
}

namespace {
void audio_trampoline(void *, Uint8 *stream, int len) {
  int16_t *out = reinterpret_cast<int16_t *>(stream);
  const int frames = len / (int)(2 * sizeof(int16_t));   // stereo int16
  if (g_on_audio) g_on_audio(out, frames, 44100);
  else std::memset(stream, 0, len);
}
} // namespace

bool open_audio(AudioCallback cb) {
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    std::fprintf(stderr, "dosiz: audio init failed: %s\n", SDL_GetError());
    return false;
  }
  SDL_AudioSpec want = {}, have = {};
  want.freq = 44100;
  want.format = AUDIO_S16SYS;
  want.channels = 2;
  want.samples = 1024;
  want.callback = &audio_trampoline;
  g_on_audio = cb;
  g_audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (!g_audio) {
    std::fprintf(stderr, "dosiz: audio unavailable: %s\n", SDL_GetError());
    g_on_audio = nullptr;
    return false;
  }
  SDL_PauseAudioDevice(g_audio, 0);   // start playback
  return true;
}

bool open_joystick(JoystickCallback cb) {
  if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) return false;
  g_on_joystick = cb;
  if (SDL_NumJoysticks() > 0) {
    g_joystick = SDL_JoystickOpen(0);
    SDL_JoystickEventState(SDL_ENABLE);
  }
  return g_joystick != nullptr;
}

namespace {
// Read the open stick and push its state to the guest game port / BIOS.
void poll_joystick() {
  if (!g_on_joystick) return;
  if (!g_joystick) { return; }   // no host stick: leave the emulated default
  int axes[4] = {0, 0, 0, 0};
  const int naxes = SDL_JoystickNumAxes(g_joystick);
  for (int a = 0; a < 4 && a < naxes; a++) axes[a] = SDL_JoystickGetAxis(g_joystick, a);
  unsigned buttons = 0;
  const int nbtn = SDL_JoystickNumButtons(g_joystick);
  for (int b = 0; b < 4 && b < nbtn; b++)
    if (SDL_JoystickGetButton(g_joystick, b)) buttons |= (1u << b);
  g_on_joystick(axes, buttons, true);
}
} // namespace

void close() {
  if (g_joystick) { SDL_JoystickClose(g_joystick); g_joystick = nullptr; g_on_joystick = nullptr; }
  if (g_audio)    { SDL_CloseAudioDevice(g_audio); g_audio = 0; g_on_audio = nullptr; }
  if (g_texture)  { SDL_DestroyTexture(g_texture);   g_texture = nullptr; }
  if (g_renderer) { SDL_DestroyRenderer(g_renderer); g_renderer = nullptr; }
  if (g_window)   { SDL_DestroyWindow(g_window);     g_window = nullptr; }
  SDL_Quit();
}

void present(const uint32_t *argb, int w, int h) {
  if (!g_renderer || w <= 0 || h <= 0) return;
  ensure_texture(w, h);
  SDL_UpdateTexture(g_texture, nullptr, argb, w * (int)sizeof(uint32_t));
  SDL_RenderClear(g_renderer);
  SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);
  SDL_RenderPresent(g_renderer);
}

void pump_events() {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) { g_quit = true; }
    else if (e.type == SDL_WINDOWEVENT &&
             e.window.event == SDL_WINDOWEVENT_CLOSE) { g_quit = true; }
    else if (e.type == SDL_KEYDOWN && g_on_key) {
      const bool shift = (e.key.keysym.mod & KMOD_SHIFT) != 0;
      const bool ctrl  = (e.key.keysym.mod & KMOD_CTRL) != 0;
      // Ctrl+Q / window close quits.
      if (ctrl && e.key.keysym.sym == SDLK_q) { g_quit = true; continue; }
      uint8_t sc = dos_scancode(e.key.keysym.scancode);
      uint8_t ch = dos_ascii(e.key.keysym.sym, shift);
      if (ctrl && e.key.keysym.sym >= SDLK_a && e.key.keysym.sym <= SDLK_z)
        ch = (uint8_t)(e.key.keysym.sym - SDLK_a + 1);  // Ctrl-A..Z = 0x01..0x1A
      if (sc || ch) g_on_key(ch, sc);
    }
    else if ((e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN ||
              e.type == SDL_MOUSEBUTTONUP) && g_on_mouse) {
      int mx, my;
      const uint32_t st = SDL_GetMouseState(&mx, &my);
      const int buttons = ((st & SDL_BUTTON_LMASK) ? 1 : 0)
                        | ((st & SDL_BUTTON_RMASK) ? 2 : 0)
                        | ((st & SDL_BUTTON_MMASK) ? 4 : 0);
      float lx = (float)mx, ly = (float)my;
      SDL_RenderWindowToLogical(g_renderer, mx, my, &lx, &ly);  // window -> guest pixels
      g_on_mouse((int)lx, (int)ly, buttons);
    }
  }
  poll_joystick();   // push current stick state to the guest game port
}

bool quit_requested() { return g_quit; }

} // namespace display
} // namespace dosiz
