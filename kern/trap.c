#include "trap.h"

#include "arm.h"
#include "mmu.h"
#include "proc.h"
#include "console.h"
#include "peripherals/timer.h"
#include "peripherals/irq.h"

static int dt = 1 << 24;
// static uint32_t ticks;

// TODO change 4 to NCPU
/* Stack must always be 16 bytes aligned. */
__attribute__((__aligned__(128)))
char kstack[KSTACKSIZE];

void
irq_init()
{
    extern char vectors[];
    load_vbar_el1(vectors);

    /* Enable timer */
    // timer_init();
    // put32(ENABLE_IRQS_1, SYSTEM_TIMER_IRQ_1);
    
    // generic_timer_init();
    // put32(TIMER_INT_CTRL_0, TIMER_INT_CTRL_0_VALUE);

    cprintf("- irq init\n");
}

void
generic_timer_init()
{
    int x = 1;
    asm volatile("msr cntp_ctl_el0, %[x]": : [x]"r"(x));
    asm volatile("msr cntp_tval_el0, %[x]": : [x]"r"(dt));
}

/*
void
timer_init()
{
    ticks = get32(TIMER_CLO);
    ticks += interval;
    put32(TIMER_C1, ticks);
}
void
irq_timer()
{
    ticks += interval;
    put32(TIMER_C1, ticks);
    put32(TIMER_CS, TIMER_CS_M1);
    cprintf("Timer interrupt received.\n");
}
*/

void
clock()
{
    cprintf("clock\n");
    asm volatile("msr cntp_tval_el0, %[x]": : [x]"r"(dt));
}

void
trap(struct trapframe *tf)
{
    int irq = get32(P2V(INT_SOURCE_0));
    switch (irq) {
    case (GENERIC_TIMER_INTERRUPT):
        clock();
        break;
    default:
        irq_error(99);
    }
}

void
irq_error(uint64_t type)
{
    cprintf("- irq_error\n");
    /* PSTATE */
    int64_t spsel, el, spsr;
    asm volatile("mrs %[x], currentel": [x]"=r"(el));
    asm volatile("mrs %[x], spsel": [x]"=r"(spsel));
    asm volatile("mrs %[x], spsr_el1": [x]"=r"(spsr));
    cprintf("EL: 0x%llx\n", el >> 2);
    cprintf("SPSel: 0x%llx\n", spsel);
    cprintf("SPSR_EL1: 0x%llx\n", spsr);


    /* Stack pointer */
    uint64_t sp, sp0;
    asm volatile("mov %[x], sp": [x]"=r"(sp));
    asm volatile("mrs %[x], sp_el0": [x]"=r"(sp0));
    cprintf("stack pointer: 0x%llx\n", sp);
    cprintf("SP_EL0: 0x%llx\n", sp0);

    /* Exception link and exception syndrome */
    int64_t elr, esr;
    asm volatile("mrs %[x], elr_el1": [x]"=r"(elr));
    asm volatile("mrs %[x], esr_el1": [x]"=r"(esr));
    cprintf("ELR_EL1: 0x%llx, EC: 0x%llx, ISS: 0x%llx. \n", elr, esr >> 26, esr & 0x1FFFFFF);

    panic("irq of type %d unimplemented. \n", type);
}
