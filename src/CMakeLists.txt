#
# dosiz/src/CMakeLists.txt
#
# dosiz runs on the in-tree **emu88** backend (vendored under ../emu88): a
# from-scratch 386 + FPU + protected mode + PC hardware. dosiz emulates DOS
# itself (the INT 21h/31h host in bridge.cc); emu88 is purely the CPU + memory
# + hardware engine. A thin DOSBox->emu88 compatibility shim (src/compat) lets
# the original bridge.cc DOS host drive emu88 unchanged. No DOSBox, no SDL2/glib.
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
# emu88 backend: CPU core + protected mode + FPU + memory. (The vendored tree
# also carries a FreeDOS-boot machine and PC hardware; only the CPU/memory core
# is needed by the shim, so we compile that subset.)
# ---------------------------------------------------------------------------
set(EMU88_DIR ${CMAKE_SOURCE_DIR}/../emu88)
set(EMU88_SRC
  ${EMU88_DIR}/emu88.cc
  ${EMU88_DIR}/emu88_pmode.cc
  ${EMU88_DIR}/emu88_fpu.cc
  ${EMU88_DIR}/emu88_mem.cc
)

add_executable(dosiz
  dosiz.cc
  config.cc
  debug_settings.cpp
  bridge.cc                   # the DOS host (INT 21h/31h emulation, PSP, loader, DPMI)
  compat/dosbox_compat.cc     # DOSBox CPU/callback API re-implemented over emu88
  ${EMU88_SRC}
)

target_include_directories(dosiz PRIVATE
  ${CMAKE_SOURCE_DIR}/compat   # DOSBox-API shim headers bridge.cc includes
  ${EMU88_DIR}
)
target_link_libraries(dosiz PRIVATE m)
