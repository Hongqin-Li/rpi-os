#include "mmu.h"
#include "memlayout.h"
#include "sysregs.h"

.section ".text.boot"

.globl _start

_start:
    /* Read cpu id, stop slave cores */
    mrs     x1, mpidr_el1
    and     x1, x1, #3
    cbnz    x1, halt

    /*
     * Get current exception level by CurrentEL[3:2]
     * For example, CurrentEL = 0b1000 indicates exception level 2
     */
    mrs     x9, CurrentEL
    and     x9, x9, #0xc
    cmp     x9, #8
    beq     el2
    bgt     el3

el3:
    mov     x2, #0x5b1
    msr     scr_el3, x2
    mov     x2, #0x3c9
    msr     spsr_el3, x2
    adr     x2, el2
    msr     elr_el3, x2
    eret

el2:
    /* Set up exception stack */
    // ldr     x1, =(kstack+PGSIZE)
    // msr     sp_el1, x1

    /* Enable CNTP for EL1 */
    mrs     x0, cnthctl_el2
    orr     x0, x0, #3
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr

    /*
     * HCR_EL2.RW, bit[31] = 1: The Execution state for EL1 is AArch64.
     * The Execution state for EL0 is determined by the current
     * value of PSTATE.nRW when executing at EL0.
     */
    mov     x0, #(1 << 31)
    // orr     x0, x0, #(1 << 1)   /* SWIO hardwired on Pi3 */
    msr     hcr_el2, x0
    // mrs     x0, hcr_el2

    /* Setup SCTLR access */
    mov     x2, #0x0800
    movk    x2, #0x30d0, lsl #16
    msr     sctlr_el1, x2

    /* Change execution level to EL1 */
    mov     x2, #0x3c4
    msr     spsr_el2, x2
    adr     x2, el1
    msr     elr_el2, x2
    eret

el1:
    adr     x0, kpgdir

    /* Higher and lower half map to same address */
    msr     ttbr0_el1, x0
    msr     ttbr1_el1, x0

    ldr     x0, =(TCR_VALUE)        
    msr     tcr_el1, x0

    ldr     x0, =(MAIR_VALUE)
    msr     mair_el1, x0

    isb

    /* Read System Control Register configuration data */
    mrs     x0, sctlr_el1
    // orr     x0, x0, #SCTLR_MMU_ENABLED 
    /* SP Alignment check enable for EL0/1. */
    orr     x0, x0, #0x1F
    msr     sctlr_el1, x0

    isb

    /* Set up stack pointer */
    msr     spsel, #1
    ldr     x1, =_start
    mov     sp, x1

    /*
     * Jump to C code, should not return
     * Why not directly run `b main` ?
     */ 
    ldr     x9, =main
    br      x9

halt:
    wfe
    b       halt
