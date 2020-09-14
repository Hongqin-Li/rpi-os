#include "mmu.h"
#include "memlayout.h"
#include "sysregs.h"

.section ".text.boot"

.globl _start

_start:
    // read cpu id, stop slave cores
    mrs     x1, mpidr_el1
    and     x1, x1, #3
    cbnz    x1, halt

    // set stack before our code
    ldr     x1, =_start

    // Get current exception level
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    and     x0, x0, #3

    // Running at EL3?
    cmp     x0, #3
    bne     init2
    mov     x2, #0x5b1
    msr     scr_el3, x2
    mov     x2, #0x3c9
    msr     spsr_el3, x2
    adr     x2, init2
    msr     elr_el3, x2
    eret

    // Running at EL2?
init2:
    cmp     x0, #1
    beq     init1
    msr     sp_el1, x1
    // enable CNTP for EL1
    mrs     x0, cnthctl_el2
    orr     x0, x0, #3
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr
    // enable AArch64 in EL1
    mov     x0, #(1 << 31)      // AArch64
    orr     x0, x0, #(1 << 1)   // SWIO hardwired on Pi3
    msr     hcr_el2, x0
    mrs     x0, hcr_el2
    // Setup SCTLR access
    mov     x2, #0x0800
    movk    x2, #0x30d0, lsl #16
    msr     sctlr_el1, x2
    // change execution level to EL1
    mov     x2, #0x3c4
    msr     spsr_el2, x2
    adr     x2, init1
    msr     elr_el2, x2
    eret

init1:
    mov     sp, x1

    mrs     x1, CurrentEL
    // Jump to C code, should not return
    bl      main

halt:
    wfe
    b       halt
