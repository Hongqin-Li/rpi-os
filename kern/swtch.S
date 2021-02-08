/*
 * Context switch
 *
 *   void swtch(struct context **old, struct context *new);
 * 
 * Save the current registers on the stack, creating
 * a struct context, and save its address in *old.
 * Switch stacks to new and pop previously-saved registers.
 */
.global swtch
swtch:
    dsb     sy; isb
    /* Save old callee-saved registers. */
    # str     q0, [sp, #-16]!

    stp     x19, xzr, [sp, #-16]!
    stp     x21, x20, [sp, #-16]!
    stp     x23, x22, [sp, #-16]!
    stp     x25, x24, [sp, #-16]!
    stp     x27, x26, [sp, #-16]!
    stp     x29, x28, [sp, #-16]!
    stp     x30, x30, [sp, #-16]!

    /* Switch stacks. */
    mov     x9, sp
    str     x9, [x0]
    mov     sp, x1

    /* Load new callee-saved registers. */
    ldp     x15, x30, [sp], #16
    ldp     x29, x28, [sp], #16
    ldp     x27, x26, [sp], #16
    ldp     x25, x24, [sp], #16
    ldp     x23, x22, [sp], #16
    ldp     x21, x20, [sp], #16
    ldp     x19, xzr, [sp], #16

    # ldr     q0, [sp], #16

    /* Why not simply ret by x30? This is a hack for forkret. */
    dsb     sy; isb
    ret     x15
