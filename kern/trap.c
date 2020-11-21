#include "trap.h"

#include "arm.h"
#include "sysregs.h"
#include "mmu.h"
#include "peripherals/irq.h"

#include "uart.h"
#include "console.h"
#include "clock.h"
#include "timer.h"
#include "sd.h"

#include "debug.h"

void
irq_init()
{
    cprintf("- irq init\n");
    clock_init();
    put32(ENABLE_IRQS_1, AUX_INT);
    put32(ENABLE_IRQS_2, VC_ARASANSDIO_INT);
    put32(GPU_INT_ROUTE, GPU_IRQ2CORE(0));
}

void
trap(struct trapframe *tf)
{
    int src = get32(IRQ_SRC_CORE(cpuid()));
    if (src & IRQ_CNTPNSIRQ) timer(), timer_reset();
    else if (src & IRQ_TIMER) clock(), clock_reset();
    else if (src & IRQ_GPU) {
        if (get32(IRQ_PENDING_1) & AUX_INT) uart_intr();
        else if (get32(IRQ_PENDING_2) & VC_ARASANSDIO_INT) sd_intr();
        else goto bad;
    } else { 
        switch (resr() >> EC_SHIFT) {
        case EC_SVC64:
            cprintf("- hello, world syscall %d\n", resr() & 0xFFFFFF);
            lesr(0);  /* Clear esr. */
            break;
        default:
bad:
            debug_reg();
            cprintf("- IRQ_PENDING_1: 0x%x\n", get32(IRQ_PENDING_1));
            cprintf("- IRQ_PENDING_2: 0x%x\n", get32(IRQ_PENDING_2));
            panic("unexpected irq.\n");
        }
    }
    disb();
}

void
irq_error(uint64_t type)
{
    cprintf("- irq_error\n");
    debug_reg();
    panic("irq of type %d unimplemented. \n", type);
}
