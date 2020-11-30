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
    put32(ENABLE_IRQS_1, AUX_INT);
    put32(ENABLE_IRQS_2, VC_ARASANSDIO_INT);
    put32(GPU_INT_ROUTE, GPU_IRQ2CORE(0));
}

void
trap(struct trapframe *tf)
{
    // cprintf("- trap: cpu %d\n", cpuid());
    int src = get32(IRQ_SRC_CORE(cpuid()));
    if (src & IRQ_CNTPNSIRQ) timer(), timer_reset();
    else if (src & IRQ_TIMER) clock(), clock_reset();
    else if (src & IRQ_GPU) {
        int p1 = get32(IRQ_PENDING_1), p2 = get32(IRQ_PENDING_2);
        if (p1 & AUX_INT) uart_intr();
        else if (p2 & VC_ARASANSDIO_INT) sd_intr();
        else {
            cprintf("- trap: unexpected gpu intr p1 %x, p2 %x, sd %d.\n", p1, p2, p2 & VC_ARASANSDIO_INT);
            goto bad;
        }
    } else {
        int ec = resr() >> EC_SHIFT, iss = resr() & ISS_MASK;
        switch (ec) {
        case EC_SVC64:
            if (iss == 0) {
                // thisproc()->tf = tf;
                tf->x[0] = syscall1(tf);
            } else {
                cprintf("Unexpected svc number %d, omitted.\n", iss);
            }
            lesr(0);  /* Clear esr. */
            break;
        default:
bad:
            debug_reg();
            cprintf("- IRQ_SRC_CORE(%d): 0x%x\n", cpuid(), src);
            cprintf("- IRQ_PENDING_1: 0x%x\n", get32(IRQ_PENDING_1));
            cprintf("- IRQ_PENDING_2: 0x%x\n", get32(IRQ_PENDING_2));
            panic("Unexpected irq.\n");
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
