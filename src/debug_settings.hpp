/*
 * debug_settings.hpp -- env-var snapshot for dosemu.
 *
 * Constructor reads every DOSEMU_* env var once at static-init (before
 * main() runs).  Call sites access flags through `g_debug.<field>`
 * instead of repeatedly calling getenv().  Modeled on DebugSettings
 * from the iospharo VM.
 */
#pragma once

namespace dosemu {

struct DebugSettings {
  DebugSettings();
  void reload();

  // Runtime behavior switches
  bool dpmi_ring3;       // DOSEMU_DPMI_RING3
  bool force_dpmi;       // DOSEMU_FORCE_DPMI
  bool no_dpmi;          // DOSEMU_NO_DPMI
  bool le_as_mz;         // DOSEMU_LE_AS_MZ

  // Trace flags
  bool trace;            // DOSEMU_TRACE
  bool dpmi_trace;       // DOSEMU_DPMI_TRACE
  bool exc_trace;        // DOSEMU_EXC_TRACE
  bool ldt_trace;        // DOSEMU_LDT_TRACE
  bool simrm_trace;      // DOSEMU_SIMRM_TRACE
  bool stackwatch;       // DOSEMU_STACKWATCH
  bool int4b_trace;      // DOSEMU_4B_TRACE
  bool int4c_trace;      // DOSEMU_4C_TRACE
  bool open_trace;       // DOSEMU_OPEN_TRACE
  bool write_trace;      // DOSEMU_WRITE_TRACE
  bool cpu_trace;        // DOSEMU_CPU_TRACE  (read here to pick core=normal;
                         // the actual per-instruction trace lives in the
                         // submodule and reads getenv() directly.)

  // Strings (nullptr if unset or empty).
  const char* path;      // DOSEMU_PATH
};

extern DebugSettings g_debug;

}  // namespace dosemu
