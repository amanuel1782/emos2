```asm
# kernel/arch_context.S
#
# Floating-point and RISC-V Vector context save/restore helpers
#
# C structures:
#
# struct fp_state {
#     uint64 f[32];       // offset 256 bytes total
#     uint32 fcsr;        // offset 256
#     uint32 reserved;    // offset 260
# };
#
# struct vector_state {
#     void   *regs;       // offset 0
#     uint32 vlenb;       // offset 8
#     uint32 reserved;    // offset 12
#     uint64 vl;          // offset 16
#     uint64 vtype;       // offset 24
#     uint64 vstart;      // offset 32
#     uint64 vcsr;        // offset 40
# };

.text

.globl save_fp
.globl restore_fp
.globl save_vec
.globl restore_vec


# ============================================================================
# Vector register save/load macros
#
# t6 = current vector register buffer address
# a1 = vlenb
#
# Each vector register occupies exactly vlenb bytes.
# ============================================================================

.macro SAVE_VREG reg
    vse8.v \reg, (t6)
    add    t6, t6, a1
.endm

.macro LOAD_VREG reg
    vle8.v \reg, (t6)
    add    t6, t6, a1
.endm


# ============================================================================
# void save_fp(struct fp_state *state)
#
# a0 = pointer to struct fp_state
#
# struct fp_state:
#     f[32]   -> offsets 0..255
#     fcsr    -> offset 256
#     reserved-> offset 260
# ============================================================================

save_fp:

    fsd f0,   0(a0)
    fsd f1,   8(a0)
    fsd f2,  16(a0)
    fsd f3,  24(a0)
    fsd f4,  32(a0)
    fsd f5,  40(a0)
    fsd f6,  48(a0)
    fsd f7,  56(a0)
    fsd f8,  64(a0)
    fsd f9,  72(a0)
    fsd f10, 80(a0)
    fsd f11, 88(a0)
    fsd f12, 96(a0)
    fsd f13,104(a0)
    fsd f14,112(a0)
    fsd f15,120(a0)
    fsd f16,128(a0)
    fsd f17,136(a0)
    fsd f18,144(a0)
    fsd f19,152(a0)
    fsd f20,160(a0)
    fsd f21,168(a0)
    fsd f22,176(a0)
    fsd f23,184(a0)
    fsd f24,192(a0)
    fsd f25,200(a0)
    fsd f26,208(a0)
    fsd f27,216(a0)
    fsd f28,224(a0)
    fsd f29,232(a0)
    fsd f30,240(a0)
    fsd f31,248(a0)

    # fcsr is a 32-bit CSR.
    csrr t0, fcsr
    sw   t0, 256(a0)

    ret


# ============================================================================
# void restore_fp(struct fp_state *state)
#
# a0 = pointer to struct fp_state
# ============================================================================

restore_fp:

    fld f0,   0(a0)
    fld f1,   8(a0)
    fld f2,  16(a0)
    fld f3,  24(a0)
    fld f4,  32(a0)
    fld f5,  40(a0)
    fld f6,  48(a0)
    fld f7,  56(a0)
    fld f8,  64(a0)
    fld f9,  72(a0)
    fld f10, 80(a0)
    fld f11, 88(a0)
    fld f12, 96(a0)
    fld f13,104(a0)
    fld f14,112(a0)
    fld f15,120(a0)
    fld f16,128(a0)
    fld f17,136(a0)
    fld f18,144(a0)
    fld f19,152(a0)
    fld f20,160(a0)
    fld f21,168(a0)
    fld f22,176(a0)
    fld f23,184(a0)
    fld f24,192(a0)
    fld f25,200(a0)
    fld f26,208(a0)
    fld f27,216(a0)
    fld f28,224(a0)
    fld f29,232(a0)
    fld f30,240(a0)
    fld f31,248(a0)

    # Restore 32-bit fcsr.
    lw   t0, 256(a0)
    csrw fcsr, t0

    ret


# ============================================================================
# void save_vec(struct vector_state *state)
#
# a0 = pointer to struct vector_state
#
# struct vector_state:
#
#   offset 0   : regs pointer
#   offset 8   : vlenb
#   offset 12  : reserved
#   offset 16  : vl
#   offset 24  : vtype
#   offset 32  : vstart
#   offset 40  : vcsr
#
# state->regs points to:
#
#   v0  = 0 * vlenb
#   v1  = 1 * vlenb
#   ...
#   v31 = 31 * vlenb
#
# Total register storage = 32 * vlenb bytes.
# ============================================================================

save_vec:

    # ------------------------------------------------------------------------
    # First save the vector CSRs.
    #
    # vsetvli below changes vl and vtype, so these must be captured first.
    # ------------------------------------------------------------------------

    csrr t0, vstart
    csrr t1, vtype
    csrr t2, vl
    csrr t3, vcsr

    # Store metadata into struct vector_state.
    sd   t2, 16(a0)        # state->vl
    sd   t1, 24(a0)        # state->vtype
    sd   t0, 32(a0)        # state->vstart
    sd   t3, 40(a0)        # state->vcsr

    # ------------------------------------------------------------------------
    # Load:
    #
    #   t6 = state->regs
    #   a1 = state->vlenb
    # ------------------------------------------------------------------------

    ld   t6, 0(a0)
    lwu  a1, 8(a0)

    # ------------------------------------------------------------------------
    # Configure vector unit so each vector register can be stored as
    # exactly vlenb bytes using e8,m1.
    # ------------------------------------------------------------------------

    vsetvli t0, a1, e8, m1, ta, ma

    # ------------------------------------------------------------------------
    # Save all 32 vector registers.
    # ------------------------------------------------------------------------

    SAVE_VREG v0
    SAVE_VREG v1
    SAVE_VREG v2
    SAVE_VREG v3
    SAVE_VREG v4
    SAVE_VREG v5
    SAVE_VREG v6
    SAVE_VREG v7
    SAVE_VREG v8
    SAVE_VREG v9
    SAVE_VREG v10
    SAVE_VREG v11
    SAVE_VREG v12
    SAVE_VREG v13
    SAVE_VREG v14
    SAVE_VREG v15
    SAVE_VREG v16
    SAVE_VREG v17
    SAVE_VREG v18
    SAVE_VREG v19
    SAVE_VREG v20
    SAVE_VREG v21
    SAVE_VREG v22
    SAVE_VREG v23
    SAVE_VREG v24
    SAVE_VREG v25
    SAVE_VREG v26
    SAVE_VREG v27
    SAVE_VREG v28
    SAVE_VREG v29
    SAVE_VREG v30
    SAVE_VREG v31

    ret


# ============================================================================
# void restore_vec(struct vector_state *state)
#
# a0 = pointer to struct vector_state
# ============================================================================

restore_vec:

    # ------------------------------------------------------------------------
    # Load register buffer pointer and vlenb.
    # ------------------------------------------------------------------------

    ld   t6, 0(a0)
    lwu  a1, 8(a0)

    # ------------------------------------------------------------------------
    # Configure vector unit for loading the complete vector registers.
    # ------------------------------------------------------------------------

    vsetvli t0, a1, e8, m1, ta, ma

    # ------------------------------------------------------------------------
    # Restore all 32 vector registers.
    # ------------------------------------------------------------------------

    LOAD_VREG v0
    LOAD_VREG v1
    LOAD_VREG v2
    LOAD_VREG v3
    LOAD_VREG v4
    LOAD_VREG v5
    LOAD_VREG v6
    LOAD_VREG v7
    LOAD_VREG v8
    LOAD_VREG v9
    LOAD_VREG v10
    LOAD_VREG v11
    LOAD_VREG v12
    LOAD_VREG v13
    LOAD_VREG v14
    LOAD_VREG v15
    LOAD_VREG v16
    LOAD_VREG v17
    LOAD_VREG v18
    LOAD_VREG v19
    LOAD_VREG v20
    LOAD_VREG v21
    LOAD_VREG v22
    LOAD_VREG v23
    LOAD_VREG v24
    LOAD_VREG v25
    LOAD_VREG v26
    LOAD_VREG v27
    LOAD_VREG v28
    LOAD_VREG v29
    LOAD_VREG v30
    LOAD_VREG v31

    # ------------------------------------------------------------------------
    # Restore vector CSRs.
    #
    # vsetvl restores both vl and vtype.
    # ------------------------------------------------------------------------

    ld   t0, 16(a0)        # vl
    ld   t1, 24(a0)        # vtype
    ld   t2, 32(a0)        # vstart
    ld   t3, 40(a0)        # vcsr

    vsetvl x0, t0, t1

    csrw vstart, t2
    csrw vcsr,   t3

    ret
```
