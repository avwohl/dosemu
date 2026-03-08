# DOSEmu

An 8088 IBM PC emulator for iOS and macOS (Catalyst). Runs FreeDOS and other
DOS-compatible operating systems on your iPhone, iPad, or Mac.

## Features

- **8088 CPU emulator** written in C++ — executes real DOS binaries
- **CGA, MDA, and Hercules** display adapters (configurable, including dual CGA+MDA)
- **INT 33h mouse driver** with touch-to-mouse mapping (tap = left click, long press = right click, drag = mouse move with button held)
- **Keyboard input** with Ctrl key toolbar, Esc, Tab, arrow keys, copy/paste
- **Floppy, hard disk, and CD-ROM ISO** image support
- **Disk image catalog** — browse and download disk images from GitHub releases
- **Download from URL** — load floppy, HDD, or ISO images from any URL
- **Named configuration profiles** — save/load machine setups with different hardware options
- **Speed control** — Full speed, IBM PC 4.77 MHz, IBM AT 8 MHz, or Turbo 25 MHz
- **Catalog versioning** — automatic re-download when the catalog is updated; warns before overwriting user-modified catalog disks

## Requirements

- Xcode 15 or later
- iOS 15.0+ / macOS (via Catalyst)
- Python 3 with Pillow (only for icon generation)

## Building

### From Xcode

Open `DOSEmu.xcodeproj` in Xcode, select your target device, and build.

### From the command line

```bash
# iOS Simulator
xcodebuild -project DOSEmu.xcodeproj \
  -scheme DOSEmu \
  -destination 'platform=iOS Simulator,name=iPhone 16' \
  -configuration Debug \
  SYMROOT="$(pwd)/build" \
  build

# macOS (Catalyst)
xcodebuild -project DOSEmu.xcodeproj \
  -scheme DOSEmu \
  -destination 'platform=macOS,variant=Mac Catalyst' \
  -configuration Debug \
  SYMROOT="$(pwd)/build" \
  build
```

**Important:** Always pass `SYMROOT="$(pwd)/build"` (or set it in Xcode build
settings) so the build output goes into `build/` instead of overwriting the
`DOSEmu/` source directory.

### Regenerating the app icon

```bash
pip3 install Pillow
python3 scripts/gen_icon.py
```

This renders a retro DOS prompt icon at 1024px and scales it to all sizes
required by the iOS and macOS App Store.

## Project Structure

```
DOSEmu.xcodeproj/          Xcode project
DOSEmu/                    iOS/macOS app source
  DOSEmuApp.swift           App entry point
  DOSEmu-Bridging-Header.h  Imports DOSEmulator.h into Swift
  Assets.xcassets/           App icon and asset catalog
  Bridge/
    DOSEmulator.h            Obj-C bridge header (enums, delegate, API)
    DOSEmulator.mm           Obj-C++ bridge — connects Swift UI to C++ core
  Views/
    ContentView.swift        Main settings UI, config profiles, disk management
    EmulatorViewModel.swift  View model — emulator lifecycle, catalog, downloads
    MachineConfig.swift      Named config profiles with Codable persistence
    TerminalView.swift       CGA terminal display with touch/mouse/keyboard input
src/                       C++ emulator core
  emu88.h / emu88.cc        8088 CPU — fetch/decode/execute, all instructions
  emu88_mem.h / emu88_mem.cc Memory subsystem (up to 16 MB address space)
  emu88_types.h              Register and flag type definitions
  emu88_trace.h              Instruction tracing/disassembly for debugging
  dos_machine.h / dos_machine.cc  Machine — ties CPU, memory, BIOS, and I/O together
  dos_bios.cc                BIOS interrupts (INT 10h video, INT 13h disk,
                              INT 16h keyboard, INT 21h DOS, INT 33h mouse, etc.)
  dos_io.h                   Abstract I/O interface (disk, video, console, mouse)
  main_cli.cc                Standalone CLI harness (for testing without iOS)
  test_emu88.cc              CPU instruction tests
  debug_boot.cc              Boot debugging utility
scripts/
  gen_icon.py                Generates app icon PNGs for all App Store sizes
release_assets/
  disks.xml                  Disk image catalog (served from GitHub releases)
```

## Architecture

```
┌─────────────────────────────────────┐
│  SwiftUI (ContentView, TerminalView)│
│  EmulatorViewModel, ConfigManager   │
└──────────────┬──────────────────────┘
               │ Obj-C bridge
┌──────────────▼──────────────────────┐
│  DOSEmulator.mm  (dos_io_ios)       │
│  Disk I/O, video callbacks, mouse   │
└──────────────┬──────────────────────┘
               │ C++
┌──────────────▼──────────────────────┐
│  dos_machine    →  emu88 (8088 CPU) │
│  dos_bios       →  emu88_mem        │
│  dos_io (abstract interface)        │
└─────────────────────────────────────┘
```

The C++ core (`src/`) is platform-independent. `dos_io` is an abstract
interface that the iOS bridge (`dos_io_ios` in `DOSEmulator.mm`) implements
for disk access, video refresh callbacks, time/date, and mouse state.

## Disk Image Catalog

The app fetches `disks.xml` from the latest GitHub release. Each entry has a
type (`floppy`, `hdd`, or `iso`), filename, description, size, license, and
optional SHA256 hash for integrity verification.

When the catalog version number changes, all previously downloaded images are
deleted and must be re-downloaded. If the emulator writes to a catalog disk at
runtime, a warning is shown since those changes may be lost on the next catalog
update. This can be toggled off in Preferences.

To add disk images to the catalog, edit `release_assets/disks.xml` and upload
the files as GitHub release assets.

## Configuration Profiles

Machine configurations are saved as named profiles and persisted in
UserDefaults. Each profile stores:

- CPU speed mode
- Display adapter (CGA, MDA, Hercules, or dual CGA+MDA)
- Mouse, PC speaker, sound card (None/AdLib/Sound Blaster), CD-ROM toggles
- Floppy A/B, HDD C filenames
- Boot drive

Use the profile picker at the top of the settings screen to switch between
configurations, or Save As to create variants.

## Getting Disk Images

The easiest way to get started is with [FreeDOS](https://www.freedos.org), a
free, open-source DOS-compatible operating system:

1. Open the app and scroll to the **Disk Catalog** section
2. Download a FreeDOS floppy image and tap **Use as A:**
3. Tap **Start Emulator**

You can also load images from a URL or use the file picker to load `.img` or
`.iso` files from your device.

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE).
