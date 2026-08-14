#ifndef ARCH_H
#define ARCH_H

#include "types.h"

// Hardware Feature Bitmasks (g_arch.features)
#define ARCH_FEAT_FP   (1ULL << 0)
#define ARCH_FEAT_VEC  (1ULL << 1)
#define ARCH_FEAT_DBG  (1ULL << 2)

struct fp_state {
    uint64 f[32];
    uint32 fcsr;
    uint32 reserved;
};

struct vector_state {
    void   *regs;        /* 32 x vlenb bytes */
    uint32  vlenb;
    uint32  reserved;

    uint64  vl;
    uint64  vtype;
    uint64  vstart;
    uint64  vcsr;
};

struct trigger_state {
    uint64 tdata1;
    uint64 tdata2;
    uint64 tdata3;
};

struct debug_state {
    int trigger_count;
    struct trigger_state *triggers;
};

struct cpu_arch_config {
    uint64 features;             // Feature flags (ARCH_FEAT_*)

    struct fp_state fp;          // Embedded FP state
    struct vector_state vec;     // Embedded Vector header
    struct debug_state dbg;      // Embedded Debug header
};

extern struct cpu_arch_config g_arch;

void arch_init(void);
uint32 get_faulting_instruction(uint64 stval, struct proc *p);
int is_fp_instruction(uint32 inst);
int is_vec_instruction(uint32 inst);
void enable_fp(void);
void enable_vec(void);
void disable_fp(void);
void disable_vec(void);
#endif