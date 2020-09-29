#include <stdint.h>

#include "arm.h"
#include "types.h"
#include "memlayout.h"
#include "mmu.h"
#include "string.h"
#include "console.h"
#include "mm.h"
#include "trap.h"

extern char edata[], end[];

void
main(uint64_t dtb_ptr32, uint64_t x1, uint64_t x2, uint64_t x3)
{
    memset(edata, 0, end-edata);

    console_init();

    cprintf("hello, world\n");

    free_range(ROUNDUP((void *)end, PGSIZE), P2V(PHYSTOP));
    irq_init();

    // uint64_t spsel;
    // asm volatile("mrs %[x], spsel": [x]"=r"(spsel): );
    // cprintf("spsel: %llx\n", spsel);
    
    user_init();

    sti();

    // int x = 10;
    // cprintf("%d %d %d %lld %lld 0x%llx\n", x, 0, 0, 0, -(1ll<<40), edata);

    /* Test interrupt */
    // generate a Data Abort with a bad address access
    // int r=*((volatile unsigned int*)0xFFFFFFFFFF000000);
    // make gcc happy about unused variables :-)
    // r++;

    // cprintf("x1: 0x%x%x, x2: %x%x, x3: %x%x\n", x1>>32, x1, x2>>32, x2, x3>>32, x3);

    cprintf("- spin\n");
    while (1) ;
}
