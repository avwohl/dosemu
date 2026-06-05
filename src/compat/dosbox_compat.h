//
// dosbox_compat.h — DOSBox CPU/memory/callback API re-implemented over emu88.
//
// dosiz emulates DOS itself (INT 21h/31h in bridge.cc); it only needs a CPU +
// memory + PC hardware from the backend. This shim provides the slice of the
// DOSBox CPU API that bridge.cc uses (registers, memory, segments, descriptors,
// callbacks, the run loop) so bridge.cc drives emu88 instead of DOSBox.
//
// Strategy: cpu_regs / Segs / cpu are the register *interface* bridge.cc reads
// and writes (exactly as in DOSBox). The real engine is a global emu88 machine.
// The interface is synced to/from emu88 at run / interrupt-trap boundaries
// (sync_to_emu / sync_from_emu), so emu88 itself is unmodified. Callbacks are
// dispatched by recognising DOSBox's native-call opcode (FE 38 lo hi) at the
// instruction stream, then letting emu88 execute the trailing IRET/RETF bytes.
//
#ifndef DOSIZ_DOSBOX_COMPAT_H
#define DOSIZ_DOSBOX_COMPAT_H

#include <cstdint>
#include <ctime>
#include <functional>

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

// real_*: real-mode seg:off access (always seg<<4 + off, like DOSBox).
inline uint16_t real_readw(uint16_t seg, uint16_t off)  { return mem_readw(PhysMake(seg, off)); }
inline void     real_writew(uint16_t seg, uint16_t off, uint16_t val) { mem_writew(PhysMake(seg, off), val); }

// ---- Descriptors (lifted from DOSBox; backed by mem_readd/mem_writed) -----
class Descriptor {
public:
    Descriptor() : saved{{0, 0}} {}

    void Load(PhysPt address) {
        saved.fill[0] = mem_readd(address);
        saved.fill[1] = mem_readd(address + 4);
    }
    void Save(PhysPt address) {
        mem_writed(address,     saved.fill[0]);
        mem_writed(address + 4, saved.fill[1]);
    }

    PhysPt GetBase() const {
        return (PhysPt)((saved.seg.base_24_31 << 24) |
                        (saved.seg.base_16_23 << 16) |
                        saved.seg.base_0_15);
    }
    uint32_t GetLimit() const {
        uint32_t limit = (saved.seg.limit_16_19 << 16) | saved.seg.limit_0_15;
        if (saved.seg.g) return (limit << 12) | 0xFFF;
        return limit;
    }
    uint32_t GetOffset() const {
        return (saved.gate.offset_16_31 << 16) | saved.gate.offset_0_15;
    }
    uint16_t GetSelector() const { return (uint16_t)saved.gate.selector; }
    uint8_t  Type() const        { return (uint8_t)saved.seg.type; }
    uint8_t  Conforming() const  { return (uint8_t)(saved.seg.type & 8); }
    uint8_t  DPL() const         { return (uint8_t)saved.seg.dpl; }
    uint8_t  Big() const         { return (uint8_t)saved.seg.big; }

    struct S_Descriptor {
        uint32_t limit_0_15  : 16;
        uint32_t base_0_15   : 16;
        uint32_t base_16_23  : 8;
        uint32_t type        : 5;
        uint32_t dpl         : 2;
        uint32_t p           : 1;
        uint32_t limit_16_19 : 4;
        uint32_t avl         : 1;
        uint32_t r           : 1;
        uint32_t big         : 1;
        uint32_t g           : 1;
        uint32_t base_24_31  : 8;
    };
    struct G_Descriptor {
        uint32_t offset_0_15  : 16;
        uint32_t selector     : 16;
        uint32_t paramcount   : 5;
        uint32_t reserved     : 3;
        uint32_t type         : 5;
        uint32_t dpl          : 2;
        uint32_t p            : 1;
        uint32_t offset_16_31 : 16;
    };
    union {
        uint32_t     fill[2];
        S_Descriptor seg;
        G_Descriptor gate;
    } saved;
};

// Descriptor-table proxies over emu88's GDTR / IDTR / LDTR. Reads/writes go
// straight through to the engine, so the cpu.gdt / cpu.idt the DPMI host pokes
// stay coherent with the running CPU without an extra copy step.
class ShimDescTable {        // proxies emu88 IDTR
public:
    PhysPt GetBase() const;
    Bitu   GetLimit() const;
    void   SetBase(PhysPt b);
    void   SetLimit(Bitu l);
    bool   GetDescriptor(Bitu selector, Descriptor &desc) const;
};
class ShimGdtTable {         // proxies emu88 GDTR + LDTR (TI bit selects LDT)
public:
    PhysPt GetBase() const;
    Bitu   GetLimit() const;
    void   SetBase(PhysPt b);
    void   SetLimit(Bitu l);
    bool   GetDescriptor(Bitu selector, Descriptor &desc) const;
    bool   SetDescriptor(Bitu selector, Descriptor &desc) const;
};

// ---- CPU state block (subset of DOSBox CPUBlock used by bridge.cc) --------
struct CPUBlock {
    Bitu cpl   = 0;
    Bitu mpl   = 0;
    Bitu cr0   = 0;
    bool pmode = false;
    ShimGdtTable gdt;
    ShimDescTable idt;
    struct { Bitu mask = 0xFFFF, notmask = 0xFFFF0000u; bool big = false; } stack;
    struct { bool big = false; } code;
};
extern CPUBlock cpu;

// ---- CPU control surface bridge.cc drives ---------------------------------
void SegSet16(Bitu seg, uint16_t val);                 // real-mode segment load
bool CPU_SetSegGeneral(SegNames seg, Bitu value);      // mode-aware segment load
void CPU_SET_CRX(Bitu cr, Bitu value);
void CPU_LGDT(Bitu limit, Bitu base);
void CPU_LIDT(Bitu limit, Bitu base);
Bitu CPU_SIDT_base();
Bitu CPU_SIDT_limit();
bool CPU_LLDT(Bitu selector);
bool CPU_LTR(Bitu selector);
void CPU_JMP(bool use32, Bitu selector, Bitu offset, Bitu oldeip);
void CPU_IRET(bool use32, Bitu oldeip);

// ---- Callbacks (DOSBox native-call mechanism over emu88) ------------------
typedef Bitu (*CallBack_Handler)();

enum {
    CB_RETN, CB_RETF, CB_RETF8, CB_RETF_STI, CB_RETF_CLI,
    CB_IRET, CB_IRETD, CB_IRET_STI, CB_IRET_EOI_PIC1,
    CB_IRQ0, CB_IRQ1, CB_IRQ9, CB_IRQ12, CB_IRQ12_RET, CB_IRQ6_PCJR,
    CB_MOUSE, CB_INT29, CB_INT16, CB_HOOKABLE, CB_TDE_IRET,
    CB_IPXESR, CB_IPXESR_RET, CB_INT21, CB_INT13, CB_VESA_WAIT, CB_VESA_PM
};
enum { CBRET_NONE = 0, CBRET_STOP = 1 };

#define CB_MAX     250
#define CB_SIZE    32
#define CB_SEG     0xF000
#define CB_SOFFSET 0x1000

inline RealPt CALLBACK_RealPointer(uint8_t cb) {
    return RealMake(CB_SEG, (uint16_t)(CB_SOFFSET + cb * CB_SIZE));
}
inline PhysPt CALLBACK_PhysPointer(uint8_t cb) {
    return PhysMake(CB_SEG, (uint16_t)(CB_SOFFSET + cb * CB_SIZE));
}
inline PhysPt CALLBACK_GetBase() { return (CB_SEG << 4) + CB_SOFFSET; }

uint8_t CALLBACK_Allocate();
void    CALLBACK_DeAllocate(uint8_t cb);
bool    CALLBACK_Setup(uint8_t cb, CallBack_Handler handler, Bitu type, const char *descr);
void    CALLBACK_Idle();
void    CALLBACK_RunRealInt(uint8_t intnum);
void    CALLBACK_RunRealFar(uint16_t seg, uint16_t off);
void    CALLBACK_SZF(bool val);
void    CALLBACK_SCF(bool val);
void    CALLBACK_SIF(bool val);

class CALLBACK_HandlerObject {
public:
    CALLBACK_HandlerObject() = default;
    ~CALLBACK_HandlerObject();
    CALLBACK_HandlerObject(const CALLBACK_HandlerObject &) = delete;
    CALLBACK_HandlerObject &operator=(const CALLBACK_HandlerObject &) = delete;

    void Install(CallBack_Handler handler, Bitu type, const char *description);
    void Install(CallBack_Handler handler, Bitu type, PhysPt addr, const char *description);
    void Uninstall();
    void Allocate(CallBack_Handler handler, const char *description = nullptr);
    uint16_t Get_callback()    { return m_cb_number; }
    RealPt   Get_RealPointer() { return CALLBACK_RealPointer((uint8_t)m_cb_number); }
    void     Set_RealVec(uint8_t vec);

private:
    bool     installed   = false;
    uint16_t m_cb_number = 0;
    bool     vec_installed = false;
    uint8_t  vec_interrupt = 0;
    RealPt   vec_old       = 0;
};

// The CPU run loop. Executes emu88 until a callback returns CBRET_STOP or a
// shutdown is requested. Nested calls (from CALLBACK_RunReal*) unwind one level
// per CBRET_STOP, matching DOSBox's DOSBOX_RunMachine semantics.
void DOSBOX_RunMachine();
extern bool shutdown_requested;

// One-time setup of the F000 callback area: the call_stop callback and the
// per-interrupt CALLBACK_RunRealInt stubs.
void CALLBACK_Init();

// ---- Misc DOSBox utility surface ------------------------------------------
void     MEM_A20_Enable(bool enable);
bool     MEM_A20_Enabled();
Bitu     MEM_TotalPages();
void     IO_WriteB(Bitu port, uint8_t val);
const char *DOSBOX_GetVersion() noexcept;
bool     DOS_AllocateMemory(uint16_t *segment, uint16_t *blocks);

typedef void (*TIMER_TickHandler)();
void TIMER_AddTickHandler(TIMER_TickHandler handler);

#define LOG_MSG(...) dosiz_compat_log(__VA_ARGS__)
void dosiz_compat_log(const char *fmt, ...);

namespace cross {
inline void localtime_r(const time_t *t, struct tm *out) { ::localtime_r(t, out); }
}

// Networking (Crynwr packet driver backend) — no host backend in the shim, so
// ETHERNET_OpenConnection always returns null and bridge.cc's pktdrv stays
// inert. The interface mirrors DOSBox's so the call sites type-check.
class EthernetConnection {
public:
    virtual ~EthernetConnection() = default;
    virtual void SendPacket(const uint8_t *packet, int size) = 0;
    virtual void GetPackets(std::function<int(const uint8_t *, int)> callback) = 0;
};
EthernetConnection *ETHERNET_OpenConnection(const char *backend);

// ---- Backend bring-up + register sync ------------------------------------
namespace dosiz_compat {
void init_machine(uint32_t ram_mb);   // create + bind the emu88 machine
void shutdown_machine();
void sync_to_emu();     // cpu_regs + Segs + cpu -> emu88
void sync_from_emu();   // emu88 -> cpu_regs + Segs + cpu
} // namespace dosiz_compat

#endif // DOSIZ_DOSBOX_COMPAT_H
