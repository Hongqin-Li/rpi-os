#ifndef INC_IRQ_H
#define INC_IRQ_H

#if RASPI == 4
// #define USE_GIC
#endif

#ifndef USE_GIC

#define IRQ_AUX             29
#define IRQ_SDIO            56
#define IRQ_ARASANSDIO      62

#define IRQ_LINES           64

#else

/* See 6.3 https://datasheets.raspberrypi.org/bcm2711/bcm2711-peripherals.pdf */
#define GIC_PPI(n)          (16 + (n))  // Private per core
#define GIC_SPI(n)          (32 + (n))  // Shared between cores

#define IRQ_LOCAL_CNTPNS    GIC_PPI(14)
#define IRQ_LOCAL_TIMER     GIC_SPI(21)
#define IRQ_AUX             GIC_SPI(93)
#define IRQ_UART            GIC_SPI(121)
#define IRQ_ARASANSDIO	    GIC_SPI(126)

#define IRQ_LINES           256
#endif

void irq_init();
void irq_enable(int);
void irq_disable(int);
void irq_register(int, void (*)());
void irq_handler();

#endif
