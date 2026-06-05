//
// dosbox_compat.h — DOSBox CPU/memory API re-implemented over the emu88 backend.
//
// dosiz emulates DOS itself (INT 21h/31h in bridge.cc); it only needs a CPU +
// memory + PC hardware from the backend. This shim provides the slice of the
// DOSBox CPU API that bridge.cc uses (registers, memory, segments, descriptors,
// callbacks, the run loop) so bridge.cc drives emu88 instead of DOSBox.
//
// Strategy: cpu_regs is the register *interface* bridge.cc reads/writes via the
// reg_* references (exactly as in DOSBox). The real engine is a global emu88
// machine; cpu_regs is synced to/from emu88 at run / interrupt-trap boundaries
// (sync_to_emu / sync_from_emu), so emu88 itself is unmodified.
//
// This header is the register + memory + segment foundation. The descriptor
// tables, CALLBACK_* and DOSBOX_RunMachine layer build on it (next phase).
//
#ifndef DOSIZ_DOSBOX_COMPAT_H
#define DOSIZ_DOSBOX_COMPAT_H

#include <cstdint>

// ---- DOSBox integer typedefs bridge.cc relies on -------------------------
typedef uintptr_t Bitu;
typedef intptr_t  Bits;
typedef uint32_t  PhysPt;
typedef uint32_t  RealPt;
typedef uint8_t   Bit8u;
typedef uint16_t  Bit16u;
typedef uint32_t  Bit32u;
typedef int8_t    Bit8s;
typedef int16_t   Bit16s;
typedef int32_t   Bit32s;

// ---- Register file (DOSBox-compatible layout) ----------------------------
union GenReg32 {
    uint32_t dword[1] = {};
    uint16_t word[2];
    uint8_t  byte[4];
};
struct CPU_Regs {
    GenReg32 regs[8] = {};
    GenReg32 ip      = {};
    uint32_t flags   = 0;
};
extern CPU_Regs cpu_regs;

// x86 register indices (match emu88: AX,CX,DX,BX,SP,BP,SI,DI).
enum { REGI_AX = 0, REGI_CX, REGI_DX, REGI_BX, REGI_SP, REGI_BP, REGI_SI, REGI_DI };
// Little-endian sub-register indices.
inline constexpr int DW_INDEX = 0, W_INDEX = 0, BH_INDEX = 1, BL_INDEX = 0;

inline uint8_t  &reg_al  = cpu_regs.regs[REGI_AX].byte[BL_INDEX];
inline uint8_t  &reg_ah  = cpu_regs.regs[REGI_AX].byte[BH_INDEX];
inline uint16_t &reg_ax  = cpu_regs.regs[REGI_AX].word[W_INDEX];
inline uint32_t &reg_eax = cpu_regs.regs[REGI_AX].dword[DW_INDEX];
inline uint8_t  &reg_bl  = cpu_regs.regs[REGI_BX].byte[BL_INDEX];
inline uint8_t  &reg_bh  = cpu_regs.regs[REGI_BX].byte[BH_INDEX];
inline uint16_t &reg_bx  = cpu_regs.regs[REGI_BX].word[W_INDEX];
inline uint32_t &reg_ebx = cpu_regs.regs[REGI_BX].dword[DW_INDEX];
inline uint8_t  &reg_cl  = cpu_regs.regs[REGI_CX].byte[BL_INDEX];
inline uint8_t  &reg_ch  = cpu_regs.regs[REGI_CX].byte[BH_INDEX];
inline uint16_t &reg_cx  = cpu_regs.regs[REGI_CX].word[W_INDEX];
inline uint32_t &reg_ecx = cpu_regs.regs[REGI_CX].dword[DW_INDEX];
inline uint8_t  &reg_dl  = cpu_regs.regs[REGI_DX].byte[BL_INDEX];
inline uint8_t  &reg_dh  = cpu_regs.regs[REGI_DX].byte[BH_INDEX];
inline uint16_t &reg_dx  = cpu_regs.regs[REGI_DX].word[W_INDEX];
inline uint32_t &reg_edx = cpu_regs.regs[REGI_DX].dword[DW_INDEX];
inline uint16_t &reg_si  = cpu_regs.regs[REGI_SI].word[W_INDEX];
inline uint32_t &reg_esi = cpu_regs.regs[REGI_SI].dword[DW_INDEX];
inline uint16_t &reg_di  = cpu_regs.regs[REGI_DI].word[W_INDEX];
inline uint32_t &reg_edi = cpu_regs.regs[REGI_DI].dword[DW_INDEX];
inline uint16_t &reg_sp  = cpu_regs.regs[REGI_SP].word[W_INDEX];
inline uint32_t &reg_esp = cpu_regs.regs[REGI_SP].dword[DW_INDEX];
inline uint16_t &reg_bp  = cpu_regs.regs[REGI_BP].word[W_INDEX];
inline uint32_t &reg_ebp = cpu_regs.regs[REGI_BP].dword[DW_INDEX];
inline uint16_t &reg_ip  = cpu_regs.ip.word[W_INDEX];
inline uint32_t &reg_eip = cpu_regs.ip.dword[DW_INDEX];
#define reg_flags cpu_regs.flags

// ---- Segment registers ----------------------------------------------------
struct Segments {
    uint16_t val[8]  = {};
    PhysPt   phys[8] = {};
};
extern Segments Segs;
enum SegNames { es = 0, cs, ss, ds, fs, gs };
inline uint16_t SegValue(Bitu s)        { return Segs.val[s]; }
inline PhysPt   SegPhys(Bitu s)         { return Segs.phys[s]; }

// ---- Memory access (backed by emu88_mem) ---------------------------------
uint8_t  mem_readb(PhysPt pt);
uint16_t mem_readw(PhysPt pt);
uint32_t mem_readd(PhysPt pt);
void     mem_writeb(PhysPt pt, uint8_t  val);
void     mem_writew(PhysPt pt, uint16_t val);
void     mem_writed(PhysPt pt, uint32_t val);

// Real-mode helpers DOSBox exposes.
inline PhysPt PhysMake(uint16_t seg, uint16_t off) { return ((PhysPt)seg << 4) + off; }
inline RealPt RealMake(uint16_t seg, uint16_t off) { return ((RealPt)seg << 16) | off; }
inline uint16_t RealSeg(RealPt p) { return (uint16_t)(p >> 16); }
inline uint16_t RealOff(RealPt p) { return (uint16_t)(p & 0xFFFF); }

// ---- Backend bring-up + register sync ------------------------------------
namespace dosiz_compat {
// Create the emu88 machine (RAM size in MB) and bind the shim to it.
void init_machine(uint32_t ram_mb);
void shutdown_machine();
// Sync the cpu_regs / Segs interface to/from the emu88 engine. Called at run
// and interrupt-trap boundaries so bridge.cc always sees a consistent cpu_regs.
void sync_to_emu();     // cpu_regs + Segs -> emu88
void sync_from_emu();   // emu88 -> cpu_regs + Segs
} // namespace dosiz_compat

#endif // DOSIZ_DOSBOX_COMPAT_H
