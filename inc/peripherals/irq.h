/* See BCM2837 ARM Peripherals and QA7_rev3.4. */
#ifndef INC_PERIPHERALS_IRQ_H
#define INC_PERIPHERALS_IRQ_H

#include "peripherals/base.h"

#define IRQ_BASIC_PENDING       (MMIO_BASE + 0x0000B200)
#define IRQ_PENDING_1           (MMIO_BASE + 0x0000B204)
#define IRQ_PENDING_2           (MMIO_BASE + 0x0000B208)
#define FIQ_CONTROL             (MMIO_BASE + 0x0000B20C)
#define ENABLE_IRQS_1           (MMIO_BASE + 0x0000B210)
#define ENABLE_IRQS_2           (MMIO_BASE + 0x0000B214)
#define ENABLE_BASIC_IRQS       (MMIO_BASE + 0x0000B218)
#define DISABLE_IRQS_1          (MMIO_BASE + 0x0000B21C)
#define DISABLE_IRQS_2          (MMIO_BASE + 0x0000B220)
#define DISABLE_BASIC_IRQS      (MMIO_BASE + 0x0000B224)

// #define SYSTEM_TIMER_IRQ_0      (1 << 0)
// #define SYSTEM_TIMER_IRQ_1      (1 << 1)
// #define SYSTEM_TIMER_IRQ_2      (1 << 2)
// #define SYSTEM_TIMER_IRQ_3      (1 << 3)
#define AUX_INT                 (1 << 29)

/* ARM Local Peripherals */
#define GPU_INT_ROUTE           (LOCAL_BASE + 0xC)
#define GPU_IRQ2CORE(i)         (i)

#define IRQ_SRC_CORE(i)     (LOCAL_BASE + 0x60 + 4*(i))
#define IRQ_TIMER               (1 << 11)   /* Local Timer */
#define IRQ_GPU                 (1 << 8)
#define IRQ_CNTPNSIRQ           (1 << 1)    /* Core Timer */

/* Local timer */
#define TIMER_ROUTE             (LOCAL_BASE + 0x24)
#define TIMER_IRQ2CORE(i)       (i)

#define TIMER_CTRL              (LOCAL_BASE + 0x34)
#define TIMER_INTENA            (1 << 29)
#define TIMER_ENABLE            (1 << 28)
#define TIMER_RELOAD_SEC        (38400000)  /* 2 * 19.2 MHz */

#define TIMER_CLR               (LOCAL_BASE + 0x38)
#define TIMER_CLR_INT           (1 << 31)
#define TIMER_RELOAD            (1 << 30)

/* Core Timer */
#define CORE_TIMER_CTRL(i)      (LOCAL_BASE + 0x40 + 4*(i))
#define CORE_TIMER_ENABLE       (1 << 1)    /* CNTPNSIRQ */

#endif
