/* vectors.S send all traps here. */
.global alltraps
alltraps:

    /* Use stp/ldp in favor of str/ldr to maintain stack alignment. */
    str     q0, [sp, #-16]!

    stp     x30, xzr, [sp, #-16]!
    stp     x28, x29, [sp, #-16]!
    stp     x26, x27, [sp, #-16]!
    stp     x24, x25, [sp, #-16]!
    stp     x22, x23, [sp, #-16]!
    stp     x20, x21, [sp, #-16]!
    stp     x18, x19, [sp, #-16]!
    stp     x16, x17, [sp, #-16]!
    stp     x14, x15, [sp, #-16]!
    stp     x12, x13, [sp, #-16]!
    stp     x10, x11, [sp, #-16]!
    stp     x8, x9, [sp, #-16]!
    stp     x6, x7, [sp, #-16]!
    stp     x4, x5, [sp, #-16]!
    stp     x2, x3, [sp, #-16]!
    stp     x0, x1, [sp, #-16]!

    mrs     x12, tpidr_el0
    mrs     x11, sp_el0
    mrs     x10, elr_el1
    mrs     x9, spsr_el1

    stp     x11, x12, [sp, #-16]!
    stp     x9, x10, [sp, #-16]!

    mov     x0, sp

    bl      trap

/* Return falls through to trapret. */
.global trapret
trapret:

    ldp     x9, x10, [sp], #16
    ldp     x11, x12, [sp], #16

    msr     spsr_el1, x9
    msr     elr_el1, x10
    msr     sp_el0, x11
    msr     tpidr_el0, x12

    ldp     x0, x1, [sp], #16
    ldp     x2, x3, [sp], #16
    ldp     x4, x5, [sp], #16
    ldp     x6, x7, [sp], #16
    ldp     x8, x9, [sp], #16
    ldp     x10, x11, [sp], #16
    ldp     x12, x13, [sp], #16
    ldp     x14, x15, [sp], #16
    ldp     x16, x17, [sp], #16
    ldp     x18, x19, [sp], #16
    ldp     x20, x21, [sp], #16
    ldp     x22, x23, [sp], #16
    ldp     x24, x25, [sp], #16
    ldp     x26, x27, [sp], #16
    ldp     x28, x29, [sp], #16
    ldp     x30, xzr, [sp], #16

    ldr     q0, [sp], #16

    ic      iallu
    dsb     sy
    isb
    eret
