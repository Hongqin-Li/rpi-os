#include "mm.h"

#include "types.h"
#include "mmu.h"
#include "memlayout.h"
#include "spinlock.h"
#include "console.h"

struct freelist {
    void *next;
    void *start, *end;
} freelist;

static struct spinlock memlock;

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
static void *
freelist_alloc(struct freelist *f)
{
    void *p = f->next;
    if (p)
        f->next = *(void **)p;
    return p;
}

// Free the page of physical memory pointed at by v.
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
    void *p = ROUNDUP((char *)start, PGSIZE);
    for (; p + PGSIZE <= end; p += PGSIZE, cnt ++) 
        freelist_free(&freelist, p);
    cprintf("free_range: 0x%x ~ 0x%x, %d pages\n", start, end, cnt);
    release(&memlock);
}

// Allocate sz size of physical memory.
// Returns 0 if failed else a pointer.
void *
kalloc(size_t sz)
{
    acquire(&memlock);
    //void *p = buddy_alloc(bsp, sz);
    void *p = freelist_alloc(&freelist);
    assert(p);

    #ifdef DEBUG
    cprintf("kalloc: p: 0x%x, sz: %d\n", p, sz);
    assert(p);
    int alloc = 0;
    for (int i = 0; i < MAXN; i ++) {
        if (!pool[i]) {
            pool[i] = p;
            alloc = 1;
            break;
        }
    }
    assert(alloc);
    #endif

    release(&memlock);
    return p;
}

// Free the physical memory pointed at by v.
void
kfree(void *va)
{
    acquire(&memlock);

    #ifdef DEBUG
    cprintf("kfree: va: 0x%x, ", va);
    int nalloc = 0;
    int valid = 0;
    for (int i = 0; i < MAXN; i ++) {
        if (!valid && pool[i] == va) {
            valid = 1;
            pool[i] = 0;
        }
        else if (pool[i]) nalloc ++;
    }
    assert(valid);
    cprintf("nalloc: %d\n", nalloc);
    #endif

    freelist_free(&freelist, va);
    //buddy_free(bsp, va);
    release(&memlock);
}
