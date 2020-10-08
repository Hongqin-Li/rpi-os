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

/* Free a user page table and all the physical memory pages. */
void
vm_free(uint64_t *pgdir)
{

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
 * Returns new size or 0 on error.
 */
int
uvm_alloc(uint64_t *pgdir, size_t oldsz, size_t newsz)
{

}

/*
 * Deallocate user pages to bring the process size from oldsz to
 * newsz.  oldsz and newsz need not be page-aligned, nor does newsz
 * need to be less than oldsz.  oldsz can be larger than the actual
 * process size.  Returns the new process size.
 */
int
uvm_dealloc(uint64_t *pgdir, size_t oldsz, size_t newsz)
{

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
