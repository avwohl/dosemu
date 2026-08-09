#
# dosiz/src/CMakeLists.txt
#
# dosiz runs on the in-tree **emu88** backend (vendored under ../emu88): a
# from-scratch 386 + FPU + protected mode + PC hardware. dosiz emulates DOS
# itself (the INT 21h/31h/67h host in bridge.cc); emu88 is purely the CPU +
# memory engine. A thin DOSBox->emu88 compatibility shim (src/compat) lets the
# original bridge.cc DOS host drive emu88 unchanged. No DOSBox, no glib.
#
# SDL2 is an OPTIONAL dependency: when present, `dosiz --window` opens a VGA
# window (src/display.cc). Without it the build is dependency-free and headless.
#
cmake_minimum_required(VERSION 3.16)
project(dosiz CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

add_compile_options(-Wall -Wextra -Wno-unused-parameter)
add_compile_definitions(_FILE_OFFSET_BITS=64)

# ---------------------------------------------------------------------------
# emu88 backend: CPU core + protected mode + FPU + memory. (The emu88 tree also
# carries a FreeDOS-boot machine and PC hardware; only the CPU/memory core is
# needed by the shim, so we compile that subset.)
#
# emu88 is OWNED BY THE qxDOS REPO (https://github.com/avwohl/qxDOS, emu88/),
# which is where it was written and where its 386 validation suites live. dosiz
# consumes it from a sibling checkout rather than keeping a second copy — the
# two copies had silently drifted 129 lines before this was consolidated. Fix
# emu88 bugs in qxDOS, not here.
#
# Override with -DEMU88_DIR=/path/to/qxDOS/emu88 if your checkout is elsewhere.
# ---------------------------------------------------------------------------
set(EMU88_DIR "${CMAKE_SOURCE_DIR}/../../qxDOS/emu88"
    CACHE PATH "Path to the emu88 CPU core (owned by the qxDOS repo)")
if(NOT EXISTS "${EMU88_DIR}/emu88.cc")
  message(FATAL_ERROR
    "emu88 sources not found at ${EMU88_DIR}\n"
    "dosiz builds against the emu88 core owned by the qxDOS repo. Clone it\n"
    "next to dosiz:\n"
    "    git clone https://github.com/avwohl/qxDOS.git\n"
    "or point dosiz at an existing checkout:\n"
    "    cmake -S src -B build -DEMU88_DIR=/path/to/qxDOS/emu88")
endif()
message(STATUS "Using emu88 from: ${EMU88_DIR}")
set(EMU88_SRC
  ${EMU88_DIR}/emu88.cc
  ${EMU88_DIR}/emu88_pmode.cc
  ${EMU88_DIR}/emu88_fpu.cc
  ${EMU88_DIR}/emu88_mem.cc
  ${EMU88_DIR}/opl.cc            # AdLib/OPL2/OPL3 FM synthesizer (PC sound)
  ${EMU88_DIR}/sound_blaster.cc  # Sound Blaster DSP + 8237 DMA digital audio
)

add_executable(dosiz
  dosiz.cc
  config.cc
  debug_settings.cpp
  bridge.cc                   # the DOS host (INT 21h/31h/67h, PSP, loader, DPMI)
  compat/dosbox_compat.cc     # DOSBox CPU/callback API re-implemented over emu88
  video.cc                    # VGA video state + software renderer (text/mode 13h)
  hardware.cc                 # PC sound hardware: AdLib/OPL + PC speaker (+ SB)
  ${EMU88_SRC}
)

target_include_directories(dosiz PRIVATE
  ${CMAKE_SOURCE_DIR}/compat   # DOSBox-API shim headers bridge.cc includes
  ${EMU88_DIR}
)
target_link_libraries(dosiz PRIVATE m)

# ---------------------------------------------------------------------------
# Optional SDL2 window for `--window`. Headless builds need nothing.
# ---------------------------------------------------------------------------
find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
  find_package(SDL2 QUIET)
endif()
if(SDL2_FOUND)
  message(STATUS "dosiz: SDL2 found -- building --window support")
  target_sources(dosiz PRIVATE display.cc)
  target_compile_definitions(dosiz PRIVATE HAVE_SDL2)
  if(TARGET SDL2::SDL2)
    target_link_libraries(dosiz PRIVATE SDL2::SDL2)
  else()
    target_include_directories(dosiz PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(dosiz PRIVATE ${SDL2_LIBRARIES})
  endif()
else()
  message(STATUS "dosiz: SDL2 not found -- --window will be unavailable")
endif()
