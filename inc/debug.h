#ifndef INC_DEBUG_H
#define INC_DEBUG_H

#include <stddef.h>
#include "arm.h"
#include "console.h"

static inline void
debug_mem(void *start, size_t sz)
{
    void *end = start + sz;
    for (int cnt = 0; start < end; start += 4, cnt ++) {
        if (cnt % 4 == 0) cprintf("%p: ", start);
        cprintf("0x%x ", get32((uint64_t)start));
        if (cnt % 4 == 3) cprintf("\n");
    }
}

static inline void
debug_reg()
{
    disb();
    /* PSTATE and Saved Program Status Register */
    int64_t spsel, el, spsr, daif;
    asm volatile("mrs %[x], currentel": [x]"=r"(el));
    asm volatile("mrs %[x], daif": [x]"=r"(daif));
    asm volatile("mrs %[x], spsel": [x]"=r"(spsel));
    asm volatile("mrs %[x], spsr_el1": [x]"=r"(spsr));
    cprintf("CurrentEL: 0x%llx\n", el >> 2);
    cprintf("DAIF: Debug(%lld) SError(%lld) IRQ(%lld) FIQ(%lld)\n",
            (daif >> 9) & 1, (daif >> 8) & 1, (daif >> 7) & 1, (daif >> 6) & 1);
    cprintf("SPSel: 0x%llx\n", spsel);
    cprintf("SPSR_EL1: 0x%llx\n", spsr);

    /* Frame pointer and stack pointer */
    uint64_t sp, sp0;
    asm volatile("mov %[x], sp": [x]"=r"(sp));
    asm volatile("mrs %[x], sp_el0": [x]"=r"(sp0));
    cprintf("SP: 0x%llx\n", sp);
    cprintf("SP_EL0: 0x%llx\n", sp0);

    /* Exception link, exception syndrome and fault address */
    int64_t elr, esr, far;
    asm volatile("mrs %[x], elr_el1": [x]"=r"(elr));
    asm volatile("mrs %[x], esr_el1": [x]"=r"(esr));
    asm volatile("mrs %[x], far_el1": [x]"=r"(far));
    cprintf("ELR_EL1: 0x%llx, EC: 0x%llx, ISS: 0x%llx. \n", elr, esr >> 26, esr & 0x1FFFFFF);
    cprintf("FAR_EL1: 0x%llx\n", far);
}

#endif
