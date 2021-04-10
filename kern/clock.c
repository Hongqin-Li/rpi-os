#include "clock.h"

#include "arm.h"
#include "base.h"
#include "irq.h"

#include "console.h"

/* Local timer */
#define TIMER_ROUTE             (LOCAL_BASE + 0x24)
#define TIMER_IRQ2CORE(i)       (i)

#define TIMER_CTRL              (LOCAL_BASE + 0x34)
#define TIMER_INTENA            (1 << 29)
#define TIMER_ENABLE            (1 << 28)

#if RASPI <= 3
#define TIMER_RELOAD_SEC        (38400000)      /* 2 * 19.2 MHz */
#elif RASPI == 4
#define TIMER_RELOAD_SEC        (108000000)     /* 2 * 54 MHz */
#endif

#define TIMER_CLR               (LOCAL_BASE + 0x38)
#define TIMER_CLR_INT           (1 << 31)
#define TIMER_RELOAD            (1 << 30)

static uint64_t cnt;

void
clock_init()
{
    put32(TIMER_CTRL, TIMER_INTENA | TIMER_ENABLE | TIMER_RELOAD_SEC);
    put32(TIMER_ROUTE, TIMER_IRQ2CORE(0));
    put32(TIMER_CLR, TIMER_RELOAD | TIMER_CLR_INT);
#ifdef USE_GIC
    irq_enable(IRQ_LOCAL_TIMER);
    irq_register(IRQ_LOCAL_TIMER, clock_intr);
#endif
}

static void
clock_reset()
{
    put32(TIMER_CLR, TIMER_CLR_INT);
}

/*
 * Real time clock (local timer) interrupt. It gets impluse from crystal clock,
 * thus independent of the variant cpu clock.
 */
void
clock_intr()
{
    trace("c: %d", ++cnt);
    clock_reset();
}
