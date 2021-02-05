#include "vm.h"

#include "string.h"
#include "types.h"
#include "arm.h"
#include "mmu.h"
#include "memlayout.h"

#include "console.h"
#include "mm.h"

/* For simplicity, we only support 4k pages in user pgdir. */

extern uint64_t kpgdir[512];

uint64_t *
vm_init()
{
    uint64_t *pgdir = kalloc();
    if (pgdir) {
        memset(pgdir, 0, PGSIZE);
    } else {
        warn("failed");
    }
    return pgdir;
}

/*
 * return the address of the pte in user page table
 * pgdir that corresponds to virtual address va.
 * if alloc != 0, create any required page table pages.
 */
static uint64_t *
pgdir_walk(uint64_t *pgdir, void *vap, int alloc)
{
    uint64_t *pgt = pgdir, va = (uint64_t)vap;
    // cprintf("pgdir_walk: 0x%p\n", pgdir);
    for (int i = 0; i < 3; i++) {
        int idx = (va >> (12+(3-i)*9)) & 0x1FF;
        if (!(pgt[idx] & PTE_VALID)) {
            void *p;
            /* FIXME Free allocated pages and restore modified pgt */
            if (alloc && (p = kalloc())) {
                memset(p, 0, PGSIZE);
                pgt[idx] = V2P(p) | PTE_TABLE;
            } else {
                warn("failed");
                return 0;
            }
        }
        // cprintf("pgt: 0x%p\n", pgt);
        pgt = P2V(PTE_ADDR(pgt[idx]));
    }
    return &pgt[(va >> 12) & 0x1FF];
}

/*
 * Iterater over physical pages whose low address lies in range [start, end)
 * and callback function f with arguments va and pa, indicating the page maps
 * from va to pa. If f returns non-zero value, free this page.
 * If alloc != 0, create any required page table pages.
 * The newly created pages won't be freed if the iteration failed.
 */
// int
// page_iter(void *pgdir, size_t start, size_t end, int alloc, int (*f)(size_t, size_t))
// {
//     void *p;
//     size_t va, pa;
//     uint64_t *pte;
//     assert(start % PGSIZE == 0 && end % PGSIZE == 0);
//     assert(start < end && end <= USERTOP);
// 
//     for (va = start; va < end; va += PGSIZE) {
//         if (alloc) {
//             if ((pte = pgdir_walk(pgdir, start, 1)) == 0)
//                 return -1;
//             if (!(*pte & PTE_VALID) & !(p = kalloc()))
//                 return -1;
//             pa = V2P(p);
//             *pte = pa | PTE_UDATA;
//         } else {
//             if ((pte = pgdir_walk(pgdir, start, 0)) == 0)
//                 continue;
//             if (!(*pte & PTE_VALID))
//                 continue;
//             pa = PTE_ADDR(*pte); 
//         }
//         if (f(va, *pte) != 0) {
//             kfree(P2V(pa));
//             *pte = 0;
//         }
//     }
// }

/* Fork a process's page table. */
uint64_t *
uvm_copy(uint64_t *pgdir)
{
    uint64_t *newpgdir = vm_init();
    if (!newpgdir) return 0;

    for (int i = 0; i < 512; i++) if (pgdir[i] & PTE_VALID) {
        assert(pgdir[i] & PTE_TABLE);
        uint64_t *pgt1 = P2V(PTE_ADDR(pgdir[i]));
        for (int i1 = 0; i1 < 512; i1++) if (pgt1[i1] & PTE_VALID) {
            assert(pgt1[i1] & PTE_TABLE);
            uint64_t *pgt2 = P2V(PTE_ADDR(pgt1[i1]));
            for (int i2 = 0; i2 < 512; i2++) if (pgt2[i2] & PTE_VALID) {
                assert(pgt2[i2] & PTE_TABLE);
                uint64_t *pgt3 = P2V(PTE_ADDR(pgt2[i2]));
                for (int i3 = 0; i3 < 512; i3++) if (pgt3[i3] & PTE_VALID) {

                    assert(pgt3[i3] & PTE_PAGE);
                    assert(pgt3[i3] & PTE_USER);
                    assert(pgt3[i3] & PTE_NORMAL);

                    assert(PTE_ADDR(pgt3[i3]) < KERNBASE);

                    uint64_t pa = PTE_ADDR(pgt3[i3]);
                    uint64_t va = (uint64_t)i << (12 + 9*3) | (uint64_t)i1 << (12 + 9*2)| (uint64_t)i2 << (12 + 9) | i3 << 12;

                    void *np = kalloc();
                    if (np == 0) {
                        vm_free(newpgdir);
                        warn("kalloc failed");
                        return 0;
                    }
                    memmove(np, P2V(pa), PGSIZE);
                    if (uvm_map(newpgdir, va, PGSIZE, V2P(np)) < 0) {
                        vm_free(newpgdir);
                        kfree(np);
                        warn("uvm_map failed");
                        return 0;
                    }
                }
            }
        }
    }
    return newpgdir;
}

// FIXME: Not tested.
/* Free a user page table and all the physical memory pages. */
void
vm_free(uint64_t *pgdir)
{
    // cprintf("--- vm free: 0x%p\n", pgdir);
    // vm_stat(pgdir);
    for (int i = 0; i < 512; i++) if (pgdir[i] & PTE_VALID) {
        assert(pgdir[i] & PTE_TABLE);
        uint64_t *pgt1 = P2V(PTE_ADDR(pgdir[i]));
        for (int i = 0; i < 512; i++) if (pgt1[i] & PTE_VALID) {
            assert(pgt1[i] & PTE_TABLE);
            uint64_t *pgt2 = P2V(PTE_ADDR(pgt1[i]));
            for (int i = 0; i < 512; i++) if (pgt2[i] & PTE_VALID) {
                assert(pgt2[i] & PTE_TABLE);
                uint64_t *pgt3 = P2V(PTE_ADDR(pgt2[i]));
                for (int i = 0; i < 512; i++) if (pgt3[i] & PTE_VALID) {
                    uint64_t *p = P2V(PTE_ADDR(pgt3[i]));
                    kfree(p);
                }
                kfree(pgt3);
            }
            kfree(pgt2);
        }
        kfree(pgt1);
    }
    kfree(pgdir);
    // cprintf("--- vm free end\n\n");
}

/*
 * Create PTEs for virtual addresses starting at va that refer to
 * physical addresses starting at pa. va and size might not
 * be page-aligned.
 * Return -1 if failed else 0.
 */
int
uvm_map(uint64_t *pgdir, void *va, size_t sz, uint64_t pa)
{
    void *p = ROUNDDOWN(va, PGSIZE), *end = va + sz;
    assert(pa < USERTOP);
    pa = ROUNDDOWN(pa, PGSIZE);
    for (; p < end; pa += PGSIZE, p += PGSIZE) {
        uint64_t *pte = pgdir_walk(pgdir, p, 1);
        if (!pte) {
            warn("walk failed");
            return -1;
        }
        if (*pte & PTE_VALID)
            panic("remap.");
        *pte = pa | PTE_UDATA;
    }
    return 0;
}

/*
 * Allocate page tables and physical memory to grow process
 * from oldsz to newsz, which need not be page aligned.
 * Stack size stksz should be page aligned.
 * Returns new size or 0 on error.
 */
int
uvm_alloc(uint64_t *pgdir, size_t base, size_t stksz, size_t oldsz, size_t newsz)
{
    assert(stksz % PGSIZE == 0);
    // cprintf("--- uvm alloc: base 0x%p, stksz 0x%p, oldsz 0x%p, newsz 0x%p\n", base, stksz, oldsz, newsz);
    if (!(stksz < USERTOP &&
          base <= oldsz &&
          oldsz <= newsz &&
          newsz < USERTOP - stksz)) {

        warn("invalid arg");
        return 0;
    }

    // cprintf("before alloc\n");
    // vm_stat(pgdir);

    for (size_t a = ROUNDUP(oldsz, PGSIZE); a < newsz; a += PGSIZE) {
        void *p = kalloc();
        if (p == 0) {
            warn("kalloc failed");
            uvm_dealloc(pgdir, base, newsz, oldsz);
            return 0;
        }
        if (uvm_map(pgdir, a, PGSIZE, V2P(p)) < 0) {
            warn("uvm_map failed");
            kfree(p);
            uvm_dealloc(pgdir, base, newsz, oldsz);
            return 0;
        }
    }
    // cprintf("after alloc\n");
    // vm_stat(pgdir);
    // cprintf("--- uvm alloc end\n\n");
    
    return newsz;
}

/*
 * Deallocate user pages to bring the process size from oldsz to
 * newsz.  oldsz and newsz need not be page-aligned, nor does newsz
 * need to be less than oldsz.  oldsz can be larger than the actual
 * process size.  Returns the new process size.
 */
int
uvm_dealloc(uint64_t *pgdir, size_t base, size_t oldsz, size_t newsz)
{
    if (newsz >= oldsz || newsz < base)
        return oldsz;

    for (size_t a = ROUNDUP(newsz, PGSIZE); a < oldsz; a += PGSIZE) {
        uint64_t *pte = pgdir_walk(pgdir, (char *)a, 0);
        if (pte && (*pte & PTE_VALID)) {
            uint64_t pa = PTE_ADDR(*pte);
            assert(pa);
            kfree(P2V(pa));
            *pte = 0;
        } else {
            cprintf("uvm_dealloc: attempt to free unallocated page.\n");
        }
    }
    return newsz;
}

void
uvm_switch(uint64_t *pgdir)
{
    // FIXME: Use NG and ASID for efficiency.
    lttbr0(V2P(pgdir));

#ifdef DEBUG
    for (int i = 0; i < 512; i++) if (pgdir[i] & PTE_VALID) {
        assert(pgdir[i] & PTE_TABLE);
        uint64_t *pgt1 = P2V(PTE_ADDR(pgdir[i]));
        for (int i1 = 0; i1 < 512; i1++) if (pgt1[i1] & PTE_VALID) {
            assert(pgt1[i1] & PTE_TABLE);
            uint64_t *pgt2 = P2V(PTE_ADDR(pgt1[i1]));
            for (int i2 = 0; i2 < 512; i2++) if (pgt2[i2] & PTE_VALID) {
                assert(pgt2[i2] & PTE_TABLE);
                uint64_t *pgt3 = P2V(PTE_ADDR(pgt2[i2]));
                for (int i3 = 0; i3 < 512; i3++) if (pgt3[i3] & PTE_VALID) {

                    assert(PTE_FLAGS(pgt3[i3]) == PTE_UDATA);
                    assert(PTE_ADDR(pgt3[i3]) < USERTOP);

                    char *p = P2V(PTE_ADDR(pgt3[i3]));
                    char *va = (char *)((uint64_t)i << (12 + 9*3) |
                                        (uint64_t)i1 << (12 + 9*2) |
                                        (uint64_t)i2 << (12 + 9) |
                                        i3 << 12);

                    dccivac(p, PGSIZE);
                    for (int i = 0; i < PGSIZE; i++) {
                        assert(va[i] == p[i]);
                    }
                }
            }
        }
    }
#endif

}

/*
 * Copy len bytes from p to user address va in page table pgdir.
 * Allocate physical pages if required.
 * Most useful when pgdir is not the current page table.
 */
int
copyout(uint64_t *pgdir, void *va, void *p, size_t len)
{
    // cprintf("copyout %lld bytes from 0x%p to va 0x%p\n", len, p, va);
    // vm_stat(pgdir);

    void *page;
    size_t n, pgoff;
    uint64_t *pte;
    if (va + len > USERTOP)
        return -1;
    for (; len; len -= n, va += n) {
        pgoff = va - ROUNDDOWN(va, PGSIZE);
        if ((pte = pgdir_walk(pgdir, va, 1)) == 0)
            return -1;
        if (*pte & PTE_VALID) {
            page = P2V(PTE_ADDR(*pte));
        } else {
            if ((page = kalloc()) == 0)
                return -1;
            *pte = V2P(page) | PTE_UDATA;
        }
        n = MIN(PGSIZE - pgoff, len);
        if (p) {
            memmove(page + pgoff, p, n);
            p += n;
        }
        else memset(page + pgoff, 0, n);
    }
    return 0;
    // vm_stat(pgdir);
    // cprintf("copyout: end\n");
}

void
vm_stat(uint64_t *pgdir)
{
    cprintf("stat pgdir: 0x%p\n", pgdir);
    uint64_t va_start = 0, va_end = 0;

    for (int i = 0; i < 512; i++) if (pgdir[i] & PTE_VALID) {
        assert(pgdir[i] & PTE_TABLE);
        uint64_t *pgt1 = P2V(PTE_ADDR(pgdir[i]));
        for (int i1 = 0; i1 < 512; i1++) if (pgt1[i1] & PTE_VALID) {
            assert(pgt1[i1] & PTE_TABLE);
            uint64_t *pgt2 = P2V(PTE_ADDR(pgt1[i1]));
            for (int i2 = 0; i2 < 512; i2++) if (pgt2[i2] & PTE_VALID) {
                assert(pgt2[i2] & PTE_TABLE);
                uint64_t *pgt3 = P2V(PTE_ADDR(pgt2[i2]));
                for (int i3 = 0; i3 < 512; i3++) if (pgt3[i3] & PTE_VALID) {

                    assert(pgt3[i3] & PTE_PAGE);
                    assert(pgt3[i3] & PTE_USER);
                    assert(pgt3[i3] & PTE_NORMAL);

                    assert(PTE_ADDR(pgt3[i3]) < KERNBASE);

                    uint64_t *p = P2V(PTE_ADDR(pgt3[i3]));
                    uint64_t va = (uint64_t)i << (12 + 9*3) | (uint64_t)i1 << (12 + 9*2)| (uint64_t)i2 << (12 + 9) | i3 << 12;
                    cprintf("va: 0x%p, pa: 0x%p, pte: 0x%p, PTE_ADDR(pte): 0x%p, P2V(...): 0x%p\n", va, p, pgt3[i3], PTE_ADDR(pgt3[i3]), P2V(PTE_ADDR(pgt3[i3])));

                    if (va == va_end)
                        va_end = va + PGSIZE;
                    else {
                        if (va_start < va_end)
                            cprintf("va: [0x%p ~ 0x%p)\n", va_start, va_end);

                        va_start = va;
                        va_end = va + PGSIZE;
                    }
                }
            }
        }
    }
    if (va_start < va_end) {
        cprintf("va: [0x%p ~ 0x%p)\n", va_start, va_end);
    }
}


void
vm_test()
{
#ifdef DEBUG
    void *pgdir = vm_init();
    void *p = kalloc(), *p2 = kalloc(), *va = 0x1000;
    memset(p, 0xAB, PGSIZE);
    memset(p2, 0xAC, PGSIZE);
    uvm_map(pgdir, va, PGSIZE, V2P(p));
    uvm_switch(pgdir);
    for (char *i = va; i < va + PGSIZE; i++) {
        assert(*i == 0xAB);
    }
    uvm_dealloc(pgdir, va, va + PGSIZE, va);

    uvm_map(pgdir, va, PGSIZE, V2P(p2));
    uvm_switch(pgdir);
    for (char *i = va; i < va + PGSIZE; i++) {
        assert(*i == 0xAC);
    }
    vm_free(pgdir);
    info("vm test pass");
#endif
}
