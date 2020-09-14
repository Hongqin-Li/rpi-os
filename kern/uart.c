#include <stdint.h>

#include "uart.h"
#include "utils.h"
#include "peripherals/mini_uart.h"
#include "peripherals/gpio.h"

void
uart_putchar(int c)
{
  while(!(get32(AUX_MU_LSR_REG) & 0x20))
    ;
  put32(AUX_MU_IO_REG, c & 0xFF);
}

char
uart_getchar(void)
{
  while(!(get32(AUX_MU_LSR_REG) & 0x01))
    ;
  return get32(AUX_MU_IO_REG) & 0xFF;
}

void
uart_puts(char *s)
{
  for (int i = 0; s[i] != '\0'; i ++)
    uart_putchar(s[i]);
}

void
uart_init(void)
{
  uint32_t selector;

  selector = get32(GPFSEL1);
  selector &= ~(7<<12);                   // clean gpio14
  selector |= 2<<12;                      // set alt5 for gpio14
  selector &= ~(7<<15);                   // clean gpio15
  selector |= 2<<15;                      // set alt5 for gpio15
  put32(GPFSEL1, selector);

  put32(GPPUD, 0);
  delay(150);
  put32(GPPUDCLK0, (1<<14)|(1<<15));
  delay(150);
  put32(GPPUDCLK0, 0);

  put32(AUX_ENABLES, 1);                  //Enable mini uart (this also enables access to it registers)
  put32(AUX_MU_CNTL_REG, 0);              //Disable auto flow control and disable receiver and transmitter (for now)
  put32(AUX_MU_IER_REG, 0);               //Disable receive and transmit interrupts
  put32(AUX_MU_LCR_REG, 3);               //Enable 8 bit mode
  put32(AUX_MU_MCR_REG, 0);               //Set RTS line to be always high
  put32(AUX_MU_BAUD_REG, 270);            //Set baud rate to 115200

  put32(AUX_MU_CNTL_REG, 3);              //Finally, enable transmitter and receiver
}
