#include <stdint.h>

#include "string.h"

#include "arm.h"
#include "console.h"
#include "mm.h"
#include "trap.h"
#include "timer.h"
#include "proc.h"

extern char edata[], end[], vectors[];

/*
 * Keep it in data segment by explicitly initializing by zero,
 * since we have `-fno-zero-initialized-in-bss` in Makefile.
 */
static struct {
    int cnt;
    struct spinlock lock;
} mp = {0};

void
main(uint64_t sp, uint64_t ent)
{
    acquire(&mp.lock);
    if (mp.cnt++ == 0) {
        memset(edata, 0, end-edata);
        console_init();
        mm_init();
        irq_init();
    }
    cprintf("- cpu: %d. hello, world\n", cpuid());
    release(&mp.lock);

    lvbar(vectors);
    timer_init();
    user_init();

    scheduler();

    panic("scheduler return.\n");
}
