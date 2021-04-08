#include "timer.h"

#include "arm.h"
#include "bsp/base.h"
#include "console.h"
#include "proc.h"

/* Core Timer */
#define CORE_TIMER_CTRL(i)      (LOCAL_BASE + 0x40 + 4*(i))
#define CORE_TIMER_ENABLE       (1 << 1)        /* CNTPNSIRQ */

static int dt = 19200000;

void
timer_init()
{
    asm volatile ("msr cntp_ctl_el0, %[x]"::[x] "r"(1));
    asm volatile ("msr cntp_tval_el0, %[x]"::[x] "r"(dt));
    put32(CORE_TIMER_CTRL(cpuid()), CORE_TIMER_ENABLE);
}

static void
timer_reset()
{
    asm volatile ("msr cntp_tval_el0, %[x]"::[x] "r"(dt));
}

/*
 * This is a per-cpu non-stable version of clock, frequency of 
 * which is determined by cpu clock (may be tuned for power saving).
 */
void
timer_intr()
{
    debug("t");
    timer_reset();
    yield();
}
