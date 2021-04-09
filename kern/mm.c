#include "mm.h"

#include "types.h"
#include "string.h"
#include "mmu.h"
#include "memlayout.h"
#include "spinlock.h"
#include "console.h"
#include "mbox.h"

#ifdef DEBUG

#define MAX_PAGES 1000
static void *alloc_ptr[MAX_PAGES];

#endif

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
    int cnt = 0;
    for (void *p = start; p + PGSIZE <= end; p += PGSIZE, cnt++)
        freelist_free(&freelist, p);
    info("0x%p ~ 0x%p, %d pages", start, end, cnt);
}

void
mm_init()
{
    // HACK Raspberry pi 4b.
    size_t phystop = MIN(0x3F000000, mbox_get_arm_memory());
    free_range(ROUNDUP((void *)end, PGSIZE), P2V(phystop));
#ifdef DEBUG
    for (int i = 0; i < MAX_PAGES; i++) {
        void *p = freelist_alloc(&freelist);
        memset(p, 0xAC, PGSIZE);
        alloc_ptr[i] = p;
    }
    for (int i = 0; i < MAX_PAGES; i++) {
        freelist_free(&freelist, alloc_ptr[i]);
        alloc_ptr[i] = 0;
    }
#endif
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
#ifdef DEBUG
    if (p) {
        for (int i = 8; i < PGSIZE; i++) {
            assert(*(char *)(p + i) == 0xAC);
        }

        int i;
        for (i = 0; i < MAX_PAGES; i++) {
            if (!alloc_ptr[i]) {
                alloc_ptr[i] = p;
                break;
            }
        }
        if (i == MAX_PAGES) {
            panic("mm: no more space for debug. ");
        }
    } else
        warn("null");
#endif
    release(&memlock);
    return p;
}

/* Free the physical memory pointed at by v. */
void
kfree(void *va)
{
    acquire(&memlock);
#ifdef DEBUG
    memset(va, 0xAC, PGSIZE);   // For debug.
    int i;
    for (i = 0; i < MAX_PAGES; i++) {
        if (alloc_ptr[i] == va) {
            alloc_ptr[i] = 0;
            break;
        }
    }
    if (i == MAX_PAGES) {
        panic("kfree: not allocated. ");
    }
#endif
    freelist_free(&freelist, va);
    release(&memlock);
}


void
mm_dump()
{
#ifdef DEBUG
    int cnt = 0;
    for (int i = 0; i < MAX_PAGES; i++) {
        if (alloc_ptr[i])
            cnt++;
    }
    debug("allocated: %d pages", cnt);
#endif
}

void
mm_test()
{
#ifdef DEBUG
    static void *p[0x100000000 / PGSIZE];
    int i;
    for (i = 0; (p[i] = kalloc()); i++) {
        memset(p[i], 0xFF, PGSIZE);
        if (i % 10000 == 0)
            debug("0x%p", p[i]);
    }
    while (i--)
        kfree(p[i]);
#endif
}
