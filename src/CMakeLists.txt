#
# dosiz/src/CMakeLists.txt
#
# dosiz now runs on the in-tree **emu88** backend (vendored under ../emu88):
# a from-scratch 386 + FPU + DPMI host + PC hardware, booting a real DOS
# (FreeDOS/MS-DOS) from a disk image. No DOSBox, no SDL2/glib — emu88 is
# self-contained, so the build is just emu88's sources plus dosiz's CLI.
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
# emu88 backend (vendored CPU + BIOS + DPMI + dos_machine + PC hardware).
# ---------------------------------------------------------------------------
set(EMU88_DIR ${CMAKE_SOURCE_DIR}/../emu88)
file(GLOB EMU88_SRC ${EMU88_DIR}/*.cc)

add_executable(dosiz
  dosiz.cc
  config.cc
  debug_settings.cpp
  emu_bridge.cc        # emu88-backed implementation of dosiz::bridge
  ${EMU88_SRC}
)

target_include_directories(dosiz PRIVATE ${EMU88_DIR})
target_link_libraries(dosiz PRIVATE m)
