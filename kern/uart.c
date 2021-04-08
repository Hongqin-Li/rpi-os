#include "bsp/base.h"
#include "bsp/gpio.h"
#include "bsp/uart.h"

#include "arm.h"

#define AUX_BASE            (MMIO_BASE + 0x215000)

#define AUX_ENABLES         (AUX_BASE + 0x04)
#define AUX_MU_IO_REG       (AUX_BASE + 0x40)
#define AUX_MU_IER_REG      (AUX_BASE + 0x44)
#define AUX_MU_IIR_REG      (AUX_BASE + 0x48)
#define AUX_MU_LCR_REG      (AUX_BASE + 0x4C)
#define AUX_MU_MCR_REG      (AUX_BASE + 0x50)
#define AUX_MU_LSR_REG      (AUX_BASE + 0x54)
#define AUX_MU_MSR_REG      (AUX_BASE + 0x58)
#define AUX_MU_SCRATCH      (AUX_BASE + 0x5C)
#define AUX_MU_CNTL_REG     (AUX_BASE + 0x60)
#define AUX_MU_STAT_REG     (AUX_BASE + 0x64)
#define AUX_MU_BAUD_REG     (AUX_BASE + 0x68)

#if RASPI <= 3
#define AUX_UART_CLOCK      250000000
#elif RASPI == 4
#define AUX_UART_CLOCK      500000000
#else
#endif

#define AUX_MU_BAUD(baud)   ((AUX_UART_CLOCK/(baud*8))-1)

void
uart_putchar(int c)
{
    while (!(get32(AUX_MU_LSR_REG) & 0x20)) ;
    put32(AUX_MU_IO_REG, c & 0xFF);
    /* Fix Windows's '\r'. */
    if (c == '\n')
        uart_putchar('\r');
}

int
uart_getchar()
{
    int stat = get32(AUX_MU_IIR_REG);
    if ((stat & 1) || (stat & 6) != 4)
        return -1;
    return get32(AUX_MU_IO_REG) & 0xFF;
}

void
uart_init()
{
    // uint32_t sel;

    // sel = get32(GPFSEL1);
    // sel &= ~(7<<12);                   /* Clean GPIO14. */
    // sel |= 2<<12;                      /* Set alt5 for GPIO14. */
    // sel &= ~(7<<15);                   /* Clean GPIO15. */
    // sel |= 2<<15;                      /* Set alt5 for GPIO15. */
    // put32(GPFSEL1, sel);

    put32(GPPUD, 0);
    delayus(5);
    put32(GPPUDCLK0, (1 << 14) | (1 << 15));
    delayus(5);
    put32(GPPUDCLK0, 0);

    /* Enable mini uart and enable access to its registers. */
    put32(AUX_ENABLES, 1);
    /* Disable auto flow control and disable receiver and transmitter (for now). */
    put32(AUX_MU_CNTL_REG, 0);
    /* Enable receive interrupts. */
    put32(AUX_MU_IER_REG, 3 << 2 | 1);
    /* Enable 8 bit mode. */
    put32(AUX_MU_LCR_REG, 3);
    /* Set RTS line to be always high. */
    put32(AUX_MU_MCR_REG, 0);
    /* Set baud rate to 115200 */
    put32(AUX_MU_BAUD_REG, AUX_MU_BAUD(115200));
    /* Clear receive and transmit FIFO. */
    put32(AUX_MU_IIR_REG, 6);
    /* Finally, enable transmitter and receiver. */
    put32(AUX_MU_CNTL_REG, 3);
}
