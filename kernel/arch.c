#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "arch.h"
#include "proc.h"

struct cpu_arch_config g_arch;

/*
 * Get the instruction that caused the illegal instruction trap.
 * Safely handles 16-bit vs 32-bit fetches across page boundaries.
 */
uint32
get_faulting_instruction(uint64 stval, struct proc *p)
{
    if (stval != 0)
        return (uint32)stval;

    uint16 cinst = 0;
    // Read first 2 bytes to check if instruction is compressed
    if (copyin(p->pagetable, (char *)&cinst, p->trapframe->epc, sizeof(cinst)) < 0)
        return 0;

    // If compressed (inst[1:0] != 11), return the 16-bit instruction
    if ((cinst & 0x3) != 0x3)
        return (uint32)cinst;

    // Otherwise fetch the full 32-bit instruction
    uint32 inst = 0;
    if (copyin(p->pagetable, (char *)&inst, p->trapframe->epc, sizeof(inst)) < 0)
        return 0;

    return inst;
}

static int
is_compressed(uint32 inst)
{
    return ((inst & 0x3) != 0x3);
}

/*
 * Scalar Floating Point decoder (RV64)
 */
int
is_fp_instruction(uint32 inst)
{
    if (inst == 0)
        return 0;

    /*
     * 32-bit instructions
     */
    if (!is_compressed(inst))
    {
        uint32 opcode = inst & 0x7f;
        uint32 funct3 = (inst >> 12) & 0x7;

        switch (opcode)
        {
        case 0x53: // OP-FP
        case 0x43: // FMADD
        case 0x47: // FMSUB
        case 0x4b: // FNMSUB
        case 0x4f: // FNMADD
            return 1;

        case 0x07: // LOAD-FP
        case 0x27: // STORE-FP
            // FLH(1), FLW(2), FLD(3), FLQ(4)
            if (funct3 >= 1 && funct3 <= 4)
                return 1;
            break;
        }

        return 0;
    }

    /*
     * 16-bit Compressed FP instructions (RV64)
     */
    uint16 cinst = inst & 0xffff;
    uint32 funct3 = (cinst >> 13) & 0x7;
    uint32 quadrant = cinst & 0x3;

    switch (quadrant)
    {
    case 0: // C0
    case 2: // C2
        // In RV64:
        // funct3 == 1 (001): c.fld   / c.fldsp
        // funct3 == 5 (101): c.fsd   / c.fsdsp
        if (funct3 == 1 || funct3 == 5)
            return 1;
        break;
    }

    return 0;
}

/*
 * Vector instruction decoder (RVV)
 */
int
is_vec_instruction(uint32 inst)
{
    if (inst == 0)
        return 0;

    // Vector instructions are strictly 32-bit
    if (is_compressed(inst))
        return 0;

    uint32 opcode = inst & 0x7f;

    switch (opcode)
    {
    case 0x57: // OP-VEC (arithmetic, vsetvli, etc.)
        return 1;

    case 0x07: // Vector Loads
    case 0x27: // Vector Stores
    {
        uint32 funct3 = (inst >> 12) & 0x7;

        // Vector loads/stores use funct3 = 0, 5, 6, 7 (e8, e16, e32, e64)
        if (funct3 == 0 || funct3 == 5 || funct3 == 6 || funct3 == 7)
            return 1;

        break;
    }
    }

    return 0;
}
// Probe number of supported hardware debug triggers
static int probe_trigger_count(void) {
    int count = 0;

    for (int i = 0; i < 32; i++) {
        asm volatile("csrw tselect, %0" : : "r"(i));
        
        uint64 read_back;
        asm volatile("csrr %0, tselect" : "=r"(read_back));

        if (read_back != i) {
            break;
        }
        count++;
    }

    if (count > 0) {
        asm volatile("csrw tselect, x0");
    }

    return count;
}

// kernel/arch.c

// Probe Vector Register Length in Bytes (VLENB) safely
static uint32 probe_vlenb(void) {
    uint64 old_sstatus = r_sstatus();
    uint32 vlenb_val = 0;

    // 1. Attempt to turn ON Vector Unit (VS = 01 Initial)
    uint64 test_sstatus = (old_sstatus & ~(3L << 9)) | (1L << 9);
    w_sstatus(test_sstatus);

    // 2. Read sstatus back to see if the hardware accepted the write
    uint64 read_back = r_sstatus();

    if ((read_back & (3L << 9)) == 0) {
        // The VS bits remained 0. 
        // Vector is either missing from silicon or disabled by Machine Mode.
        // We abort safely before executing the illegal CSR read.
        w_sstatus(old_sstatus);
        return 0; 
    }

    // 3. The CPU accepted the VS state change! 
    // It is now 100% safe to read vector CSRs without trapping.
    asm volatile("csrr %0, 0xc22" : "=r"(vlenb_val));

    // 4. Restore original sstatus state (Vector OFF) for lazy context loading
    w_sstatus(old_sstatus);

    return vlenb_val;
}

void arch_init(void) {
    memset(&g_arch, 0, sizeof(g_arch));

    // 1. Probe Debug Triggers -> Store in g_arch.dbg
    g_arch.dbg.trigger_count = probe_trigger_count();
    if (g_arch.dbg.trigger_count > 0) {
        g_arch.features |= ARCH_FEAT_DBG;
    }

    // 2. Probe Vector Unit -> Store in g_arch.vec
    g_arch.vec.vlenb = probe_vlenb();
    if (g_arch.vec.vlenb > 0) {
        g_arch.features |= ARCH_FEAT_VEC;
    }

    // 3. Log probed architectural features
    printf("arch: probed vlenb=%d bytes (%d-bit vectors), dbg_triggers=%d\n",
           g_arch.vec.vlenb, g_arch.vec.vlenb * 8, g_arch.dbg.trigger_count);
}
void enable_fp(void)
{
  uint64 s = r_sstatus();
  s = (s & ~SSTATUS_FS_MASK) | SSTATUS_FS_CLEAN;
  w_sstatus(s);
}

void enable_vec(void)
{
  uint64 s = r_sstatus();
  s = (s & ~SSTATUS_VS_MASK) | SSTATUS_VS_CLEAN;
  w_sstatus(s);
}

void disable_fp(void)
{
  w_sstatus(r_sstatus() & ~SSTATUS_FS_MASK);
}
void disable_vec(void)
{
  w_sstatus(r_sstatus() & ~SSTATUS_VS_MASK);
}