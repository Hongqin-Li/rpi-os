#include <stdint.h>

#include "string.h"
#include "uart.h"

extern char edata[], end[];

void
main(uint64_t dtb_ptr32, uint64_t x1, uint64_t x2, uint64_t x3)
{
  memset(end, 0, end-edata);

  uart_init();
  uart_puts("Hello, kernel World!\r\n");
 
  while (1)
    uart_putchar(uart_getchar());
}
