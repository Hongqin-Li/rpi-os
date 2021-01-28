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
    if (pgdir)
        memset(pgdir, 0, PGSIZE);
    return pgdir;
}

/*
 * Return the address of the PTE in user page table
 * pgdir that corresponds to virtual address va.
 * If alloc != 0, create any required page table pages.
 */
static uint64_t *
pgdir_walk(uint64_t *pgdir, void *vap, int alloc)
{
    uint64_t *pgt = pgdir, va = (uint64_t)vap;
    cprintf("pgdir_walk: 0x%p\n", pgdir);
    for (int i = 0; i < 3; i++) {
        int idx = (va >> (12+(3-i)*9)) & 0x1FF;
        if (!(pgt[idx] & PTE_VALID)) {
            void *p;
            /* FIXME Free allocated pages and restore modified pgt */
            if (alloc && (p = kalloc())) {
                memset(p, 0, PGSIZE);
                pgt[idx] = V2P(p) | PTE_TABLE;
            } else {
                panic("pages used out.");
                return 0;
            }
        }
        cprintf("pgt: 0x%p\n", pgt);
        pgt = P2V(PTE_ADDR(pgt[idx]));
    }
    return &pgt[(va >> 12) & 0x1FF];
}

/* Fork a process's page table. */
uint64_t *
vm_fork(uint64_t *pgdir)
{

}

// FIXME: Not tested.
/* Free a user page table and all the physical memory pages. */
void
vm_free(uint64_t *pgdir)
{
    cprintf("--- vm free: 0x%p\n", pgdir);
    vm_stat(pgdir);
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

                    assert(pgt3[i] & PTE_PAGE);
                    assert(pgt3[i] & PTE_USER);
                    assert(pgt3[i] & PTE_NORMAL);

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
    cprintf("--- vm free end\n\n");
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
    cprintf("uvm_map: pgdir 0x%p, va 0x%p, pa 0x%llx\n", pgdir, va, pa);
    assert(pa < KERNBASE);
    pa = ROUNDDOWN(pa, PGSIZE);
    void *p = ROUNDDOWN(va, PGSIZE), *end = va + sz;
    for (; p < end; pa += PGSIZE, p += PGSIZE) {
        uint64_t *pte = pgdir_walk(pgdir, p, 1);
        if (!pte) {
            panic("walk failed.");
            return -1;
        }
        if (*pte & PTE_VALID) panic("remap.");
        *pte = pa | PTE_UDATA;
        cprintf("pte: 0x%llx, *pte: 0x%llx\n", pte, *pte);
    }
    return 0;
}

/* TODO
 * Load a program segment into pgdir.  addr must be page-aligned
 * and the pages from addr to addr+sz must already be mapped.
int
uvm_load(uint64_t *pgdir, char *addr, struct inode *ip, size_t offset, size_t sz)
{

}
 */

/*
 * Allocate page tables and physical memory to grow process
 * from oldsz to newsz, which need not be page aligned.
 * Stack size stksz should be page aligned.
 * Returns new size or 0 on error.
 */
int
uvm_alloc(uint64_t *pgdir, size_t base, size_t stksz, size_t oldsz, size_t newsz)
{
    assert(stksz == ROUNDUP(stksz, PGSIZE) && stksz == ROUNDDOWN(stksz, PGSIZE));
    size_t old_top = base + oldsz;
    size_t new_top = base + newsz;
    cprintf("--- uvm alloc: base 0x%p, stksz 0x%p, oldsz 0x%p, newsz 0x%p\n", base, stksz, oldsz, newsz);
    if (!(stksz < USERTOP && base <= old_top && old_top <= new_top && new_top < USERTOP - stksz)) {
        cprintf("- uvm alloc: invalid arg\n");
        return 0;
    }

    cprintf("before alloc\n");
    vm_stat(pgdir);

    for (size_t a = ROUNDUP(old_top, PGSIZE); a < new_top; a += PGSIZE) {
        void *p;
        if ((p = kalloc()) == 0) {
            cprintf("- uvm alloc: memory used out\n");
            kfree(p);
            uvm_dealloc(pgdir, base, stksz, newsz, oldsz);
            return 0;
        }
        if (uvm_map(pgdir, a, PGSIZE, V2P(p)) < 0) {
            cprintf("- uvm alloc: memory used out\n");
            uvm_dealloc(pgdir, base, stksz, newsz, oldsz);
            return 0;
        }
    }

    cprintf("after alloc\n");
    vm_stat(pgdir);
    cprintf("--- uvm alloc end\n\n");

    return newsz;
}

/*
 * Deallocate user pages to bring the process size from oldsz to
 * newsz.  oldsz and newsz need not be page-aligned, nor does newsz
 * need to be less than oldsz.  oldsz can be larger than the actual
 * process size.  Returns the new process size.
 */
int
uvm_dealloc(uint64_t *pgdir, size_t base, size_t stksz, size_t oldsz, size_t newsz)
{
    panic("unimplemented. ");
}

void
uvm_switch(uint64_t *pgdir)
{
    lttbr0(V2P(pgdir));
}

/*
 * Copy len bytes from p to user address va in page table pgdir.
 * Most useful when pgdir is not the current page table.
 * uva2ka ensures this only works for PTE_U pages.
 */
void
copyout()
{

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
    void *pgdir = vm_init();
    vm_free(pgdir);
}
