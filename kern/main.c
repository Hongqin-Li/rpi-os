#include <stdint.h>

#include "string.h"

#include "arm.h"
#include "console.h"
#include "vm.h"
#include "mm.h"
#include "clock.h"
#include "timer.h"
#include "sd.h"
#include "trap.h"
#include "proc.h"

extern char edata[], end[], vectors[];
extern void mbox_test();

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
        clock_init();
        sd_init();

        mm_init();

        irq_init();

        proc_init();
        user_init();

        binit();

        // Tests
        mbox_test();
        // mm_test();
        vm_test();
    }
    release(&mp.lock);

    timer_init();
    lvbar(vectors);
    lesr(0);

    info("cpu %d init finished", cpuid());

    scheduler();

    panic("scheduler return.\n");
}
