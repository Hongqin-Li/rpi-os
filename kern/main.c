#include <stdint.h>

#include "string.h"

#include "arm.h"
#include "memlayout.h"
#include "console.h"
#include "mm.h"
#include "trap.h"
#include "proc.h"
#include "debug.h"
#include "timer.h"

extern char edata[], end[], vectors[];

/* Stack must always be 16 bytes aligned. */

static struct {
    int cnt;
    struct spinlock lock;
} mplock = {0};

void
main(uint64_t sp, uint64_t ent)
{
    acquire(&mplock.lock);

    if (mplock.cnt++ == 0) {
        memset(edata, 0, end-edata);
        console_init();
        mm_init();
        irq_init();
    }

    cprintf("hello, world\n");
    cprintf("cpu: %d.\n", cpuid());
    // debug_reg();

    release(&mplock.lock);

    lvbar(vectors);
    timer_init();

    user_init();
    scheduler();

    panic("scheduler return.\n");

    // int x = 10;
    // cprintf("%d %d %d %lld %lld 0x%llx\n", x, 0, 0, 0, -(1ll<<40), edata);

    /* Test interrupt */
    // generate a Data Abort with a bad address access
    // int r=*((volatile unsigned int*)0xFFFFFFFFFF000000);
    // make gcc happy about unused variables :-)
    // r++;

    // cprintf("x1: 0x%x%x, x2: %x%x, x3: %x%x\n", x1>>32, x1, x2>>32, x2, x3>>32, x3);
}
