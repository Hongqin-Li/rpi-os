#include "mm.h"
#include "types.h"
#include "mmu.h"
#include "memlayout.h"
#include "string.h"
#include "spinlock.h"
#include "console.h"

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
    // cprintf("free\n");
    *(void **)v = f->next;
    f->next = v;
}

void
free_range(void *start, void *end)
{
    acquire(&memlock);
    int cnt = 0;
    void *p = ROUNDUP((char *)start, PGSIZE);
    for (; p + PGSIZE <= end; p += PGSIZE, cnt ++) 
        freelist_free(&freelist, p);
    cprintf("- free_range: 0x%p ~ 0x%p, %d pages\n", start, end, cnt);
    release(&memlock);

    // mm_test();
    // mm_test();
}

/*
 * Allocate a page of physical memory.
 * Returns 0 if failed else a pointer.
 */
void *
kalloc()
{
    acquire(&memlock);
    void *p = freelist_alloc(&freelist);
    release(&memlock);
    return p;
}

/*
 * Free the physical memory pointed at by v.
 */
void
kfree(void *va)
{
    acquire(&memlock);
    freelist_free(&freelist, va);
    release(&memlock);
}


void
mm_test()
{
    cprintf("* mm test begin\n");
    static void *p[PHYSTOP/PGSIZE];
    int i;
    for (i = 0; p[i] = kalloc(); i++) {
        memset(p[i], 0xFF, PGSIZE);
        if (i % 10000 == 0) cprintf("0x%p\n", p[i]);
    }
    while (i--)
        kfree(p[i]);
    cprintf("* mm test end\n");
}
