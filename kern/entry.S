#include "mmu.h"
#include "memlayout.h"
#include "sysregs.h"

.section ".text.boot"

.globl _start

_start:
    mrs     x0, mpidr_el1    
    and     x0, x0, #0xFF    // Check processor id
    cbnz    x0, spin         // Hang for all non-primary CPU

    // ttbr1_el1 is updated to point to the previously populated PGD table.
    ldr     x0, =(V2P_WO(entry_pgd))
    msr     ttbr1_el1, x0

    // tcr_el1 of Translation Control Register is responsible 
    // for configuring some general parameters of the MMU. 
    // For example, here we configure that both kernel and 
    // user page tables should use 4 KB pages.
    ldr    x0, =(TCR_VALUE)
    msr    tcr_el1, x0

    ldr    x0, =(MAIR_VALUE)
    msr    mair_el1, x0

    mov    x0, #SCTLR_MMU_ENABLED
    msr    sctlr_el1, x0

    // Set stack before our code
    ldr     x5, =_start
    mov     sp, x5

    // jump to C code
    ldr    x2, =main
    br     x2

spin:
    b   spin
