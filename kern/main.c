#include <stdint.h>

#include "memlayout.h"
#include "mmu.h"
#include "string.h"
#include "uart.h"
#include "console.h"
#include "mm.h"

extern char edata[], end[];
extern int64_t entry_pgd[];

void
main(uint64_t dtb_ptr32, uint64_t x1, uint64_t x2, uint64_t x3)
{
    memset(edata, 0, end-edata);

    uart_init();
    free_range(end, P2V(PHYSTOP));
    cprintf("Hello, kernel World!\n");

    // cprintf("x1: 0x%x%x, x2: %x%x, x3: %x%x\n", x1>>32, x1, x2>>32, x2, x3>>32, x3);

    while (1)
      uart_putchar(uart_getchar());
}
