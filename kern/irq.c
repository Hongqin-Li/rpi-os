/*
 * Raspberry Pi 3 has its own interrupt controller(see BCM2837 ARM Peripherals and QA7_rev3.4),
 * while Pi 4 use a standard GIC-400.
 */

#include "irq.h"
#include "base.h"
#include "arm.h"
#include "timer.h"
#include "clock.h"
#include "console.h"

#define IRQ_BASIC_PENDING       (MMIO_BASE + 0xB200)
#define IRQ_PENDING_1           (MMIO_BASE + 0xB204)
#define IRQ_PENDING_2           (MMIO_BASE + 0xB208)
#define FIQ_CONTROL             (MMIO_BASE + 0xB20C)
#define ENABLE_IRQS_1           (MMIO_BASE + 0xB210)
#define ENABLE_IRQS_2           (MMIO_BASE + 0xB214)
#define ENABLE_BASIC_IRQS       (MMIO_BASE + 0xB218)
#define DISABLE_IRQS_1          (MMIO_BASE + 0xB21C)
#define DISABLE_IRQS_2          (MMIO_BASE + 0xB220)
#define DISABLE_BASIC_IRQS      (MMIO_BASE + 0xB224)

/* ARM Local Peripherals */
#define GPU_INT_ROUTE           (LOCAL_BASE + 0xC)
#define GPU_IRQ2CORE(i)         (i)

#define IRQ_SRC_CORE(i)         (LOCAL_BASE + 0x60 + 4*(i))
#define IRQ_SRC_TIMER               (1 << 11)   /* Local Timer */
#define IRQ_SRC_GPU                 (1 << 8)
#define IRQ_SRC_CNTPNSIRQ           (1 << 1)    /* Core Timer */

/*
 * The following definitions are valid for non-secure access,
 * if not labeled otherwise.
 */

/* Generic Interrupt Controller (GIC-400). */
#define GICD_BASE   (KERNBASE + 0xFF841000)
#define GICC_BASE   (KERNBASE + 0xFF842000)
#define GIC_END     (KERNBASE + 0xFF847FFF)

/* GIC distributor registers. */
#define GICD_CTLR               (GICD_BASE + 0x000)
#define GICD_CTLR_DISABLE       (0 << 0)
#define GICD_CTLR_ENABLE        (1 << 0)
/* Secure access. */
#define GICD_CTLR_ENABLE_GROUP0 (1 << 0)
#define GICD_CTLR_ENABLE_GROUP1 (1 << 1)

#define GICD_IGROUPR0           (GICD_BASE + 0x080)     // secure access for group 0
#define GICD_ISENABLER0         (GICD_BASE + 0x100)
#define GICD_ICENABLER0         (GICD_BASE + 0x180)
#define GICD_ISPENDR0           (GICD_BASE + 0x200)
#define GICD_ICPENDR0           (GICD_BASE + 0x280)
#define GICD_ISACTIVER0         (GICD_BASE + 0x300)
#define GICD_ICACTIVER0         (GICD_BASE + 0x380)
#define GICD_IPRIORITYR0        (GICD_BASE + 0x400)
#define GICD_IPRIORITYR_DEFAULT 0xA0
#define GICD_IPRIORITYR_FIQ     0x40

#define GICD_ITARGETSR0         (GICD_BASE + 0x800)
#define GICD_ITARGETSR_CORE0    (1 << 0)

#define GICD_ICFGR0                 (GICD_BASE + 0xC00)
#define GICD_ICFGR_LEVEL_SENSITIVE  (0 << 1)
#define GICD_ICFGR_EDGE_TRIGGERED   (1 << 1)

#define GICD_SGIR                           (GICD_BASE + 0xF00)
#define GICD_SGIR_SGIINTID__MASK            0x0F
#define GICD_SGIR_CPU_TARGET_LIST__SHIFT    16
#define GICD_SGIR_TARGET_LIST_FILTER__SHIFT 24

/* GIC CPU interface registers. */
#define GICC_CTLR               (GICC_BASE + 0x000)
#define GICC_CTLR_DISABLE       (0 << 0)
#define GICC_CTLR_ENABLE        (1 << 0)
/* Secure access. */
#define GICC_CTLR_ENABLE_GROUP0 (1 << 0)
#define GICC_CTLR_ENABLE_GROUP1 (1 << 1)
#define GICC_CTLR_FIQ_ENABLE    (1 << 3)

#define GICC_PMR                (GICC_BASE + 0x004)
#define GICC_PMR_PRIORITY       (0xF0 << 0)

#define GICC_IAR                    (GICC_BASE + 0x00C)
#define GICC_IAR_INTERRUPT_ID__MASK 0x3FF
#define GICC_IAR_CPUID__SHIFT       10
#define GICC_IAR_CPUID__MASK        (3 << 10)

#define GICC_EOIR                   (GICC_BASE + 0x010)
#define GICC_EOIR_EOIINTID__MASK    0x3FF
#define GICC_EOIR_CPUID__SHIFT      10
#define GICC_EOIR_CPUID__MASK       (3 << 10)

static void (*handler[IRQ_LINES])();

/* Route all interrupt to cpu 0. */
void
irq_init()
{
    for (int i = 0; i < IRQ_LINES; i++)
        handler[i] = 0;

    put32(GPU_INT_ROUTE, GPU_IRQ2CORE(0));

#if RASPI == 4
    put32(GICD_CTLR, GICD_CTLR_DISABLE);

    /* Disable, acknowledge and deactivate all interrupts. */
    for (int n = 0; n < IRQ_LINES / 32; n++) {
        put32(GICD_ICENABLER0 + 4 * n, ~0);
        put32(GICD_ICPENDR0 + 4 * n, ~0);
        put32(GICD_ICACTIVER0 + 4 * n, ~0);
    }

    /* Direct all interrupts to core 0 with default priority. */
    for (int n = 0; n < IRQ_LINES / 4; n++) {
        put32(GICD_IPRIORITYR0 + 4 * n, GICD_IPRIORITYR_DEFAULT
              | GICD_IPRIORITYR_DEFAULT << 8
              | GICD_IPRIORITYR_DEFAULT << 16
              | GICD_IPRIORITYR_DEFAULT << 24);

        put32(GICD_ITARGETSR0 + 4 * n, GICD_ITARGETSR_CORE0
              | GICD_ITARGETSR_CORE0 << 8
              | GICD_ITARGETSR_CORE0 << 16 | GICD_ITARGETSR_CORE0 << 24);
    }

    /* Set all interrupts to level triggered. */
    for (int n = 0; n < IRQ_LINES / 16; n++) {
        put32(GICD_ICFGR0 + 4 * n, GICD_ICFGR_LEVEL_SENSITIVE);
    }

    put32(GICD_CTLR, GICD_CTLR_ENABLE);

    /* Initialize core 0 CPU interface. */
    put32(GICC_PMR, GICC_PMR_PRIORITY);
    put32(GICC_CTLR, GICC_CTLR_ENABLE);
#endif
}

/* Enable interrupt i. */
void
irq_enable(int i)
{
#if RASPI == 3
    put32(ENABLE_IRQS_1 + 4 * (i / 32), 1 << (i % 32));
#elif RASPI == 4
    put32(GICD_ISENABLER0 + 4 * (i / 32), 1 << (i % 32));
#endif
}

void
irq_disable(int i)
{
#if RASPI == 3

#elif RASPI == 4
    put32(GICD_ICENABLER0 + 4 * (i / 32), 1 << (i % 32));
#endif
}

/*
 * Register an interrupt handler.
 * When interrupt i happens, it will call function f.
 */
void
irq_register(int i, void (*f)())
{
    handler[i] = f;
}

static int
handle1(int i)
{
    if (handler[i]) {
        handler[i] ();
        return 1;
    } else {
        debug("no handler for irq %d", i);
    }
    return 0;
}

void
irq_handler()
{
    int nack = 0;
#if RASPI == 3
    int src = get32(IRQ_SRC_CORE(cpuid()));
    assert(!(src & ~(IRQ_SRC_CNTPNSIRQ | IRQ_SRC_GPU | IRQ_SRC_TIMER)));
    if (src & IRQ_SRC_CNTPNSIRQ) {
        timer_intr();
        nack++;
    }
    if (src & IRQ_SRC_TIMER) {
        clock_intr();
        nack++;
    }
    uint64_t irq =
        get32(IRQ_PENDING_1) | (((uint64_t) get32(IRQ_PENDING_2)) << 32);
    for (int i = 0; i < IRQ_LINES && irq; i++) {
        if (irq & 1) {
            nack += handle1(i);
        }
        irq >>= 1;
    }
#elif RASPI == 4
    uint32_t iar = get32(GICC_IAR);
    uint32_t i = iar & GICC_IAR_INTERRUPT_ID__MASK;
    if (i < IRQ_LINES) {
        if (i > 15) {
            /* Peripheral interrupts (PPI and SPI). */
            nack += handle1(i);
        } else {
            /* Software generated interrupts (SGI). */
            uint32_t core =
                (iar & GICC_IAR_CPUID__MASK) >> GICC_IAR_CPUID__SHIFT;

            panic("unimplemented, irq %d, core %d", i, core);
        }
        put32(GICC_EOIR, iar);
    } else {
        assert(i >= 1020);
        trace("spurious interrupt %u, src 0x%x", i,
              get32(IRQ_SRC_CORE(cpuid())));
    }
#endif
    if (nack == 0) {
        trace("unexpected interrupt, maybe sdhost or EC_UNKNOWN");
    }
}
