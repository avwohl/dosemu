//
// dosbox_compat.cc — DOSBox CPU/memory/callback API implemented over emu88.
//
// See dosbox_compat.h for the strategy. In short: cpu_regs / Segs / cpu are the
// register interface bridge.cc manipulates; a global emu88 machine is the real
// engine; the two are synced at run / callback boundaries. DOSBox's native-call
// opcode (FE 38 lo hi) is recognised in the instruction stream and dispatched
// to the registered C++ handler, after which emu88 executes the trailing
// IRET/RETF bytes the callback stub carries.
//
#include "dosbox_compat.h"
#include "../../emu88/emu88.h"
#include "../../emu88/emu88_mem.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// ---- Register / segment / cpu interface ----------------------------------
CPU_Regs cpu_regs;
Segments Segs;
CPUBlock cpu;
bool     shutdown_requested = false;

namespace {

// emu88 engine. Plain emu88 — dosiz emulates DOS itself, so none of emu88's
// FreeDOS-boot machine (dos_machine/dos_bios/dos_dpmi) is used here.
struct EmuCpu : public emu88 {
    using emu88::emu88;

    // Log PM interrupts/exceptions (vector + error code) when tracing, then let
    // emu88's normal IDT dispatch proceed.
    bool intercept_pm_int(emu88_uint8 vector, bool is_software_int,
                          bool has_error_code, emu88_uint32 error_code) override {
        static int t = -1;
        if (t < 0) t = getenv("DOSIZ_VEC_TRACE") ? 1 : 0;
        if (t && (!is_software_int || vector < 0x20)) {
            const uint32_t lin = seg_cache[seg_CS].base + ip;
            uint8_t b[4];
            for (int i = 0; i < 4; i++)
                b[i] = paging_enabled() ? read_linear8(lin + i) : mem->fetch_mem(lin + i);
            std::fprintf(stderr,
                "[vec] %02x sw=%d err=%04x CS:EIP=%04x:%08x cpl=%d bytes=%02x %02x %02x %02x "
                "DS=%04x(lim=%05x) SS=%04x:%08x\n",
                vector, is_software_int, error_code, sregs[seg_CS], ip, cpl,
                b[0], b[1], b[2], b[3],
                sregs[seg_DS], seg_cache[seg_DS].limit,
                sregs[seg_SS], get_reg32(reg_SP));
        }
        return false;
    }
};
emu88_mem *g_mem = nullptr;
EmuCpu    *g_cpu = nullptr;

} // namespace

// ---- Descriptor-table proxies over emu88 GDTR/IDTR/LDTR -------------------
PhysPt ShimDescTable::GetBase() const  { return g_cpu ? g_cpu->idtr_base  : 0; }
Bitu   ShimDescTable::GetLimit() const { return g_cpu ? g_cpu->idtr_limit : 0; }
void   ShimDescTable::SetBase(PhysPt b)  { if (g_cpu) g_cpu->idtr_base  = b; }
void   ShimDescTable::SetLimit(Bitu l)   { if (g_cpu) g_cpu->idtr_limit = (uint16_t)l; }
bool   ShimDescTable::GetDescriptor(Bitu selector, Descriptor &desc) const {
    if (!g_cpu) return false;
    Bitu addr = selector & ~(Bitu)7u;
    if (addr >= g_cpu->idtr_limit) return false;
    desc.Load(g_cpu->idtr_base + (PhysPt)addr);
    return true;
}

PhysPt ShimGdtTable::GetBase() const  { return g_cpu ? g_cpu->gdtr_base  : 0; }
Bitu   ShimGdtTable::GetLimit() const { return g_cpu ? g_cpu->gdtr_limit : 0; }
void   ShimGdtTable::SetBase(PhysPt b)  { if (g_cpu) g_cpu->gdtr_base  = b; }
void   ShimGdtTable::SetLimit(Bitu l)   { if (g_cpu) g_cpu->gdtr_limit = (uint16_t)l; }
bool   ShimGdtTable::GetDescriptor(Bitu selector, Descriptor &desc) const {
    if (!g_cpu) return false;
    Bitu addr = selector & ~(Bitu)7u;
    if (selector & 4) {  // TI=1 -> LDT
        if (addr >= g_cpu->ldtr_cache.limit) return false;
        desc.Load(g_cpu->ldtr_cache.base + (PhysPt)addr);
    } else {
        if (addr >= g_cpu->gdtr_limit) return false;
        desc.Load(g_cpu->gdtr_base + (PhysPt)addr);
    }
    return true;
}
bool ShimGdtTable::SetDescriptor(Bitu selector, Descriptor &desc) const {
    if (!g_cpu) return false;
    Bitu addr = selector & ~(Bitu)7u;
    if (selector & 4) {
        if (addr >= g_cpu->ldtr_cache.limit) return false;
        desc.Save(g_cpu->ldtr_cache.base + (PhysPt)addr);
    } else {
        if (addr >= g_cpu->gdtr_limit) return false;
        desc.Save(g_cpu->gdtr_base + (PhysPt)addr);
    }
    return true;
}

// ---- Register sync --------------------------------------------------------
namespace dosiz_compat {

void init_machine(uint32_t ram_mb) {
    if (ram_mb < 16) ram_mb = 16;
    g_mem = new emu88_mem(ram_mb * 1024u * 1024u);
    g_mem->set_a20(true);
    g_cpu = new EmuCpu(g_mem);
    g_cpu->reset();
}

void shutdown_machine() {
    delete g_cpu; g_cpu = nullptr;
    delete g_mem; g_mem = nullptr;
}

void sync_to_emu() {
    if (!g_cpu) return;
    for (int i = 0; i < 8; i++) g_cpu->set_reg32(i, cpu_regs.regs[i].dword[0]);
    g_cpu->ip = cpu_regs.ip.dword[0];
    g_cpu->set_eflags(cpu_regs.flags);
    for (int i = 0; i < 6; i++) {
        g_cpu->sregs[i] = Segs.val[i];
        // Honour a direct Segs.phys[] override (the DPMI host rewrites a CS/SS
        // base under a fixed selector value for its PM trampolines).
        g_cpu->seg_cache[i].base = Segs.phys[i];
    }
    // NOTE: cpl / CS.D/B / SS.D/B are owned by the engine — the CPU_* mutators
    // (CPU_IRET, CPU_SET_CRX, CPU_SetSegGeneral via the descriptors) set them
    // directly. Pushing the interface's cpu.* snapshot here would clobber them
    // with stale values, since bridge.cc handlers don't refresh cpu.* mid-call.
}

void sync_from_emu() {
    if (!g_cpu) return;
    for (int i = 0; i < 8; i++) cpu_regs.regs[i].dword[0] = g_cpu->get_reg32(i);
    cpu_regs.ip.dword[0] = g_cpu->ip;
    cpu_regs.flags = g_cpu->get_eflags();
    for (int i = 0; i < 6; i++) {
        Segs.val[i]  = g_cpu->sregs[i];
        Segs.phys[i] = g_cpu->seg_cache[i].base;
    }
    cpu.cpl   = g_cpu->cpl;
    cpu.mpl   = g_cpu->cpl;
    cpu.cr0   = g_cpu->cr0;
    cpu.pmode = g_cpu->protected_mode();
    cpu.code.big      = g_cpu->code_32();
    cpu.stack.big     = g_cpu->stack_32();
    cpu.stack.mask    = cpu.stack.big ? 0xFFFFFFFFu : 0xFFFFu;
    cpu.stack.notmask = cpu.stack.big ? 0x00000000u : 0xFFFF0000u;
}

emu88_mem *machine_mem() { return g_mem; }
emu88     *machine_cpu() { return g_cpu; }

} // namespace dosiz_compat

// ---- Memory access over emu88_mem ----------------------------------------
uint8_t  mem_readb(PhysPt pt)              { return g_mem ? g_mem->fetch_mem(pt)   : 0xFF; }
uint16_t mem_readw(PhysPt pt)              { return g_mem ? g_mem->fetch_mem16(pt) : 0xFFFF; }
uint32_t mem_readd(PhysPt pt)              { return g_mem ? g_mem->fetch_mem32(pt) : 0xFFFFFFFFu; }
void     mem_writeb(PhysPt pt, uint8_t v)  { if (g_mem) g_mem->store_mem(pt, v); }
void     mem_writew(PhysPt pt, uint16_t v) { if (g_mem) g_mem->store_mem16(pt, v); }
void     mem_writed(PhysPt pt, uint32_t v) { if (g_mem) g_mem->store_mem32(pt, v); }

// ---- CPU control surface --------------------------------------------------
static inline void mirror_seg(int seg_idx) {
    Segs.val[seg_idx]  = g_cpu->sregs[seg_idx];
    Segs.phys[seg_idx] = g_cpu->seg_cache[seg_idx].base;
}

void SegSet16(Bitu seg, uint16_t val) {
    if (!g_cpu) return;
    g_cpu->load_segment_real((int)seg, val);
    mirror_seg((int)seg);
}

bool CPU_SetSegGeneral(SegNames seg, Bitu value) {
    if (!g_cpu) return false;
    g_cpu->load_segment((int)seg, (uint16_t)value);
    mirror_seg((int)seg);
    return false;  // DOSBox returns true on fault; we report success
}

void CPU_SET_CRX(Bitu cr, Bitu value) {
    if (!g_cpu) return;
    switch (cr) {
    case 0: g_cpu->cr0 = (emu88_uint32)value; cpu.cr0 = value;
            cpu.pmode = (value & emu88::CR0_PE) != 0; break;
    case 2: g_cpu->cr2 = (emu88_uint32)value; break;
    case 3: g_cpu->cr3 = (emu88_uint32)value; break;
    case 4: g_cpu->cr4 = (emu88_uint32)value; break;
    default: break;
    }
}

void CPU_LGDT(Bitu limit, Bitu base) {
    if (!g_cpu) return;
    g_cpu->gdtr_limit = (emu88_uint16)limit;
    g_cpu->gdtr_base  = (emu88_uint32)base;
}
void CPU_LIDT(Bitu limit, Bitu base) {
    if (!g_cpu) return;
    g_cpu->idtr_limit = (emu88_uint16)limit;
    g_cpu->idtr_base  = (emu88_uint32)base;
}
Bitu CPU_SIDT_base()  { return g_cpu ? g_cpu->idtr_base  : 0; }
Bitu CPU_SIDT_limit() { return g_cpu ? g_cpu->idtr_limit : 0; }

bool CPU_LLDT(Bitu selector) {
    if (!g_cpu) return false;
    if ((selector & 0xFFFC) == 0) {
        g_cpu->ldtr = 0;
        g_cpu->ldtr_cache = emu88::SegDescCache{};
        return false;
    }
    emu88_uint8 d[8];
    g_cpu->read_descriptor(g_cpu->gdtr_base, (emu88_uint16)(selector >> 3), d);
    emu88::parse_descriptor(d, g_cpu->ldtr_cache);
    g_cpu->ldtr = (emu88_uint16)selector;
    return false;
}

bool CPU_LTR(Bitu selector) {
    if (!g_cpu) return false;
    emu88_uint8 d[8];
    g_cpu->read_descriptor(g_cpu->gdtr_base, (emu88_uint16)(selector >> 3), d);
    emu88::parse_descriptor(d, g_cpu->tr_cache);
    g_cpu->tr = (emu88_uint16)selector;
    return false;
}

void CPU_JMP(bool use32, Bitu selector, Bitu offset, Bitu /*oldeip*/) {
    if (!g_cpu) return;
    g_cpu->far_call_or_jmp((emu88_uint16)selector, (emu88_uint32)offset, /*is_call=*/false);
    mirror_seg(emu88::seg_CS);
    cpu_regs.ip.dword[0] = g_cpu->ip;
    cpu.code.big = g_cpu->code_32();
    (void)use32;
}

void CPU_IRET(bool use32, Bitu /*oldeip*/) {
    if (!g_cpu) return;
    // Push the handler's interface writes (it set SS:ESP and laid down the IRET
    // frame via reg_esp) into the engine, then operate on the engine and refresh
    // the interface at the end.
    dosiz_compat::sync_to_emu();
    const uint32_t ssb = g_cpu->seg_cache[emu88::seg_SS].base;
    uint32_t esp = g_cpu->get_reg32(emu88::reg_SP);
    if (!g_cpu->stack_32()) esp &= 0xFFFF;
    uint32_t neip, ncs, nfl;
    if (use32) {
        neip = mem_readd(ssb + esp);
        ncs  = mem_readd(ssb + esp + 4) & 0xFFFF;
        nfl  = mem_readd(ssb + esp + 8);
        esp += 12;
    } else {
        neip = mem_readw(ssb + esp);
        ncs  = mem_readw(ssb + esp + 2);
        nfl  = mem_readw(ssb + esp + 4);
        esp += 6;
    }
    const unsigned curcpl = g_cpu->cpl;
    const unsigned newcpl = g_cpu->protected_mode() ? (ncs & 3) : curcpl;
    if (g_cpu->protected_mode() && newcpl > curcpl) {
        // Inter-privilege return: pop the outer SS:ESP too, then load CS/SS at
        // the NEW (less-privileged) CPL — emu88's load_segment rejects a DPL=3
        // code/stack load while CPL is still 0 unless told the effective CPL.
        uint32_t nesp, nss;
        if (use32) { nesp = mem_readd(ssb + esp); nss = mem_readd(ssb + esp + 4) & 0xFFFF; }
        else       { nesp = mem_readw(ssb + esp); nss = mem_readw(ssb + esp + 2); }
        g_cpu->cpl = (emu88_uint8)newcpl;
        g_cpu->exception_pending = false;
        g_cpu->load_segment(emu88::seg_CS, (uint16_t)ncs, (int)newcpl);
        g_cpu->load_segment(emu88::seg_SS, (uint16_t)nss, (int)newcpl);
        g_cpu->ip = neip;
        g_cpu->set_eflags(nfl);
        g_cpu->set_reg32(emu88::reg_SP, nesp);
    } else {
        g_cpu->load_segment(emu88::seg_CS, (uint16_t)ncs, (int)newcpl);
        g_cpu->ip = neip;
        g_cpu->set_eflags(nfl);
        g_cpu->set_reg32(emu88::reg_SP, esp);
    }
    dosiz_compat::sync_from_emu();
    (void)use32;
}

// ===========================================================================
// Callback subsystem
// ===========================================================================
namespace {

CallBack_Handler g_cb_handlers[CB_MAX] = {};
bool             g_cb_allocated[CB_MAX] = {};
const char      *g_cb_descr[CB_MAX] = {};
uint8_t          g_call_stop = 0;

bool g_cb_stop   = false;   // a callback asked to unwind one RunMachine level
int  g_run_depth = 0;

std::vector<TIMER_TickHandler> g_tick_handlers;
uint32_t g_tick_ctr = 0;

Bitu stop_handler() { return CBRET_STOP; }

uint8_t peekb(uint32_t lin) {
    if (!g_cpu) return 0;
    return g_cpu->paging_enabled() ? g_cpu->read_linear8(lin) : g_mem->fetch_mem(lin);
}

// Write a callback stub (the native-call opcode + the type-specific trailing
// instructions) at a physical address. Mirrors DOSBox's callback_setup_extra
// for the stub shapes dosiz actually installs.
void write_stub(uint8_t cb, Bitu type, PhysPt at) {
    auto w = [&](uint8_t b) { mem_writeb(at++, b); };
    auto native = [&]() { w(0xFE); w(0x38); w((uint8_t)(cb & 0xFF)); w((uint8_t)(cb >> 8)); };
    switch (type) {
    case CB_RETN:  native(); w(0xC3); break;
    case CB_RETF:  native(); w(0xCB); break;
    case CB_RETF8: native(); w(0xCA); w(0x08); w(0x00); break;
    case CB_IRETD: native(); w(0x66); w(0xCF); break;
    case CB_INT21:
        w(0xFB);             // sti
        native();
        w(0xCF);             // iret
        w(0xCB);             // retf  (alternate entry, unused by the trap path)
        w(0x51); w(0xB9); w(0x40); w(0x01); w(0xE2); w(0xFE); w(0x59); w(0xCF);
        break;
    case CB_IRET:
    default:       native(); w(0xCF); break;
    }
}

void fire_ticks() {
    if (g_tick_handlers.empty()) return;
    dosiz_compat::sync_from_emu();
    for (auto h : g_tick_handlers) h();
    dosiz_compat::sync_to_emu();
}

int g_cb_trace = -1;  // -1 = unread, 0/1 = DOSIZ_CB_TRACE

// Execute one step: dispatch a native callback if CS:IP sits on one, else run
// a single emu88 instruction.
void cpu_step() {
    const uint32_t lin = g_cpu->seg_cache[emu88::seg_CS].base + g_cpu->ip;
    if (peekb(lin) == 0xFE && peekb(lin + 1) == 0x38) {
        const uint16_t n = (uint16_t)(peekb(lin + 2) | (peekb(lin + 3) << 8));
        dosiz_compat::sync_from_emu();
        reg_eip += 4;  // advance past FE 38 lo hi
        if (g_cb_trace < 0) g_cb_trace = getenv("DOSIZ_CB_TRACE") ? 1 : 0;
        if (g_cb_trace) {
            std::fprintf(stderr,
                "[cb %3u %-22s] AX=%04x BX=%04x CX=%04x DX=%04x "
                "CS:EIP=%04x:%08x pmode=%d\n",
                n, (n < CB_MAX && g_cb_descr[n]) ? g_cb_descr[n] : "?",
                reg_ax, reg_bx, reg_cx, reg_dx, SegValue(cs), reg_eip,
                g_cpu->protected_mode());
        }
        Bitu r = CBRET_NONE;
        if (n < CB_MAX && g_cb_handlers[n]) r = g_cb_handlers[n]();
        dosiz_compat::sync_to_emu();
        if (r == CBRET_STOP) g_cb_stop = true;
        return;
    }
    g_cpu->execute();
}

} // namespace

uint8_t CALLBACK_Allocate() {
    for (int i = 1; i < CB_MAX; i++) {
        if (!g_cb_allocated[i]) {
            g_cb_allocated[i] = true;
            g_cb_handlers[i]  = nullptr;
            return (uint8_t)i;
        }
    }
    std::fprintf(stderr, "dosiz: CALLBACK_Allocate exhausted\n");
    return 0;
}

void CALLBACK_DeAllocate(uint8_t cb) {
    if (cb && cb < CB_MAX) {
        g_cb_allocated[cb] = false;
        g_cb_handlers[cb]  = nullptr;
    }
}

bool CALLBACK_Setup(uint8_t cb, CallBack_Handler handler, Bitu type, const char *descr) {
    if (cb >= CB_MAX) return false;
    g_cb_handlers[cb] = handler;
    g_cb_descr[cb]    = descr;
    write_stub(cb, type, CALLBACK_PhysPointer(cb));
    return true;
}

void CALLBACK_Idle() { CALLBACK_RunRealFar(0, 0); }

void CALLBACK_RunRealFar(uint16_t seg, uint16_t off) {
    reg_sp -= 4;
    real_writew(SegValue(ss), reg_sp + 0, RealOff(CALLBACK_RealPointer(g_call_stop)));
    real_writew(SegValue(ss), reg_sp + 2, RealSeg(CALLBACK_RealPointer(g_call_stop)));
    const uint32_t oldeip = reg_eip;
    const uint16_t oldcs  = SegValue(cs);
    reg_eip = off;
    SegSet16(cs, seg);
    DOSBOX_RunMachine();
    reg_eip = oldeip;
    SegSet16(cs, oldcs);
}

void CALLBACK_RunRealInt(uint8_t intnum) {
    const uint32_t oldeip = reg_eip;
    const uint16_t oldcs  = SegValue(cs);
    reg_eip = CB_SOFFSET + (CB_MAX * CB_SIZE) + (intnum * 6);
    SegSet16(cs, CB_SEG);
    DOSBOX_RunMachine();
    reg_eip = oldeip;
    SegSet16(cs, oldcs);
}

// FLAGS masks (avoid the FLAG_* names — they collide with emu88's enum).
static constexpr uint16_t MASK_CF = 0x0001;
static constexpr uint16_t MASK_ZF = 0x0040;
static constexpr uint16_t MASK_IF = 0x0200;

void CALLBACK_SZF(bool val) {
    uint16_t f = real_readw(SegValue(ss), reg_sp + 4);
    if (val) f |= MASK_ZF; else f &= ~MASK_ZF;
    real_writew(SegValue(ss), reg_sp + 4, f);
}
void CALLBACK_SCF(bool val) {
    uint16_t f = real_readw(SegValue(ss), reg_sp + 4);
    if (val) f |= MASK_CF; else f &= ~MASK_CF;
    real_writew(SegValue(ss), reg_sp + 4, f);
}
void CALLBACK_SIF(bool val) {
    uint16_t f = real_readw(SegValue(ss), reg_sp + 4);
    if (val) f |= MASK_IF; else f &= ~MASK_IF;
    real_writew(SegValue(ss), reg_sp + 4, f);
}

CALLBACK_HandlerObject::~CALLBACK_HandlerObject() { Uninstall(); }

void CALLBACK_HandlerObject::Install(CallBack_Handler handler, Bitu type, const char *description) {
    if (!installed) {
        installed   = true;
        m_cb_number = CALLBACK_Allocate();
    }
    CALLBACK_Setup((uint8_t)m_cb_number, handler, type, description);
}

void CALLBACK_HandlerObject::Install(CallBack_Handler handler, Bitu type, PhysPt addr, const char *description) {
    if (!installed) {
        installed   = true;
        m_cb_number = CALLBACK_Allocate();
    }
    g_cb_handlers[m_cb_number] = handler;
    g_cb_descr[m_cb_number]    = description;
    write_stub((uint8_t)m_cb_number, type, addr);
}

void CALLBACK_HandlerObject::Allocate(CallBack_Handler handler, const char *description) {
    if (!installed) {
        installed   = true;
        m_cb_number = CALLBACK_Allocate();
        g_cb_handlers[m_cb_number] = handler;
        g_cb_descr[m_cb_number]    = description;
    }
}

void CALLBACK_HandlerObject::Set_RealVec(uint8_t vec) {
    if (vec_installed) return;
    vec_installed = true;
    vec_interrupt = vec;
    vec_old = mem_readd(vec * 4u);
    mem_writed(vec * 4u, CALLBACK_RealPointer((uint8_t)m_cb_number));
}

void CALLBACK_HandlerObject::Uninstall() {
    if (vec_installed) {
        mem_writed(vec_interrupt * 4u, vec_old);
        vec_installed = false;
    }
    if (installed) {
        CALLBACK_DeAllocate((uint8_t)m_cb_number);
        installed = false;
    }
}

void DOSBOX_RunMachine() {
    dosiz_compat::sync_to_emu();
    g_run_depth++;
    for (;;) {
        if (shutdown_requested) break;
        if (g_cpu->halted)      break;
        cpu_step();
        if (g_cb_stop) { g_cb_stop = false; break; }
        if (g_run_depth == 1 && ((++g_tick_ctr) & 0x3FFF) == 0) fire_ticks();
    }
    g_run_depth--;
    dosiz_compat::sync_from_emu();
}

void CALLBACK_Init() {
    // Slot 0 is reserved; mark it so Allocate never hands it out.
    g_cb_allocated[0] = true;

    // call_stop: the universal "unwind one RunMachine level" callback used by
    // CALLBACK_RunRealInt / CALLBACK_RunRealFar.
    g_call_stop = CALLBACK_Allocate();
    CALLBACK_Setup(g_call_stop, &stop_handler, CB_IRET, "stop");

    // Per-interrupt RunRealInt stubs: `INT n ; native-call(call_stop)`. Landing
    // here runs the real IVT handler; when it IRETs back the native call fires
    // call_stop -> CBRET_STOP, unwinding the nested machine.
    const PhysPt rint_base = CALLBACK_GetBase() + CB_MAX * CB_SIZE;
    for (int i = 0; i <= 0xFF; i++) {
        PhysPt at = rint_base + i * 6;
        mem_writeb(at + 0, 0xCD);
        mem_writeb(at + 1, (uint8_t)i);
        mem_writeb(at + 2, 0xFE);
        mem_writeb(at + 3, 0x38);
        mem_writeb(at + 4, (uint8_t)(g_call_stop & 0xFF));
        mem_writeb(at + 5, (uint8_t)(g_call_stop >> 8));
    }
}

// ===========================================================================
// Misc utility surface
// ===========================================================================
void MEM_A20_Enable(bool enable) { if (g_mem) g_mem->set_a20(enable); }
bool MEM_A20_Enabled()           { return g_mem ? g_mem->get_a20() : true; }
Bitu MEM_TotalPages()            { return g_mem ? (g_mem->get_mem_size() / 4096u) : 0; }

void IO_WriteB(Bitu port, uint8_t val) { if (g_cpu) g_cpu->port_out((emu88_uint16)port, val); }

const char *DOSBOX_GetVersion() noexcept { return "emu88"; }

void TIMER_AddTickHandler(TIMER_TickHandler handler) {
    if (handler) g_tick_handlers.push_back(handler);
}

void dosiz_compat_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    va_end(ap);
    std::fputc('\n', stderr);
}

// Networking backend unavailable in the shim — the packet driver installs but
// has nowhere to send. bridge.cc tolerates a null connection.
EthernetConnection *ETHERNET_OpenConnection(const char * /*backend*/) { return nullptr; }

// Screenshot capture is a DOSBox display feature; no-op without a renderer.
// (bridge.cc forward-declares this for its optional DOSIZ_SCREENSHOT_SECS path.)
void CAPTURE_RequestRawScreenshot() {}

// Tiny conventional-memory bump allocator for the few paragraphs bridge asks
// DOS for (currently just the INT 60h packet-driver stub). Pulls from the free
// BIOS-data region [0x0060, 0x0100) below the program load area at PSP 0x0100.
bool DOS_AllocateMemory(uint16_t *segment, uint16_t *blocks) {
    static uint16_t next = 0x0060;
    uint16_t need = (blocks && *blocks) ? *blocks : 1;
    if ((uint32_t)next + need > 0x0100) {
        if (blocks) *blocks = 0x0100 - next;
        return false;
    }
    if (segment) *segment = next;
    next += need;
    return true;
}
