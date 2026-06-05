//
// dosbox_compat.cc — definitions for the DOSBox→emu88 CPU shim foundation.
//
#include "dosbox_compat.h"
#include "../../emu88/emu88.h"
#include "../../emu88/emu88_mem.h"

// The register interface bridge.cc reads/writes via reg_* / Segs.
CPU_Regs cpu_regs;
Segments Segs;

namespace {
// emu88 engine. A subclass so the next phase can override do_interrupt() to
// trap INT 21h/31h/etc. into dosiz's DOS+DPMI emulation (CALLBACK dispatch).
struct EmuCpu : public emu88 {
    using emu88::emu88;
};
emu88_mem *g_mem = nullptr;
EmuCpu    *g_cpu = nullptr;
} // namespace

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
    for (int i = 0; i < 6; i++) g_cpu->sregs[i] = Segs.val[i];
    // NOTE: segment descriptor caches are programmed via CPU_SetSegGeneral /
    // CPU_L*DT in the PM/descriptor phase; this only mirrors the values.
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
}

emu88_mem *machine_mem() { return g_mem; }
emu88     *machine_cpu() { return g_cpu; }

} // namespace dosiz_compat

// ---- memory access over emu88_mem ----------------------------------------
uint8_t  mem_readb(PhysPt pt)              { return g_mem ? g_mem->fetch_mem(pt)   : 0xFF; }
uint16_t mem_readw(PhysPt pt)              { return g_mem ? g_mem->fetch_mem16(pt) : 0xFFFF; }
uint32_t mem_readd(PhysPt pt)              { return g_mem ? g_mem->fetch_mem32(pt) : 0xFFFFFFFFu; }
void     mem_writeb(PhysPt pt, uint8_t v)  { if (g_mem) g_mem->store_mem(pt, v); }
void     mem_writew(PhysPt pt, uint16_t v) { if (g_mem) g_mem->store_mem16(pt, v); }
void     mem_writed(PhysPt pt, uint32_t v) { if (g_mem) g_mem->store_mem32(pt, v); }
