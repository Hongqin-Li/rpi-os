#include "mm.h"

#include "types.h"
#include "string.h"
#include "mmu.h"
#include "memlayout.h"
#include "spinlock.h"
#include "console.h"
#include "peripherals/mbox.h"

extern char end[];

struct freelist {
    void *next;
    void *start, *end;
} freelist;

static struct spinlock memlock;

void mm_test();

/*
 * Allocate one 4096-byte page of physical memory.
 * Returns a pointer that the kernel can use.
 * Returns 0 if the memory cannot be allocated.
 */ 
static void *
freelist_alloc(struct freelist *f)
{
    void *p = f->next;
    if (p)
        f->next = *(void **)p;
    return p;
}

/*
 * Free the page of physical memory pointed at by v.
 */
static void
freelist_free(struct freelist *f, void *v)
{
    *(void **)v = f->next;
    f->next = v;
}

void
free_range(void *start, void *end)
{
    acquire(&memlock);
    int cnt = 0;
    for (void *p = start; p + PGSIZE <= end; p += PGSIZE, cnt ++) 
        freelist_free(&freelist, p);
    cprintf("- free_range: 0x%p ~ 0x%p, %d pages\n", start, end, cnt);
    release(&memlock);

    // mm_test();
    // mm_test();
}

void
mm_init()
{
    int phystop = mbox_get_arm_memory();
    free_range(ROUNDUP((void *)end, PGSIZE), P2V(phystop));
}

/*
 * Allocate a page of physical memory.
 * Returns 0 if failed else a pointer.
 * Corrupt the page by filling non-zero value in it for debugging.
 */
void *
kalloc()
{
    acquire(&memlock);
    void *p = freelist_alloc(&freelist);
    if (p) {
        memset(p, 0xAC, PGSIZE);
        cprintf("kalloc 0x%p\n", p);
    }
    else cprintf("- kalloc null\n");
    release(&memlock);
    return p;
}

/*
 * Free the physical memory pointed at by v.
 */
void
kfree(void *va)
{
    cprintf("kfree 0x%p...", va);
    acquire(&memlock);
    freelist_free(&freelist, va);
    release(&memlock);
    cprintf("finished\n");
}


void
mm_test()
{
    cprintf("- mm test begin\n");
    static void *p[0x100000000/PGSIZE];
    int i;
    for (i = 0; (p[i] = kalloc()); i++) {
        memset(p[i], 0xFF, PGSIZE);
        if (i % 10000 == 0) cprintf("0x%p\n", p[i]);
    }
    while (i--)
        kfree(p[i]);
    cprintf("- mm test end\n");
}
