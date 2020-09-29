#include "vm.h"
#include "mm.h"
#include "string.h"
#include "mmu.h"

/* For simplicity, we only support 4k pages in user pgdir. */

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
vm_walk(uint64_t *pgdir, void *vap, int alloc)
{
    uint64_t *pgt = pgdir, va = (uint64_t)vap;
    for (int i = 0; i < 4; i++) {
        int idx = (va >> (12+(3-i)*9)) & 0x1FF;
        if (!pgt[idx]) {
            if (alloc) {
                void *p = kalloc();
                memset(p, 0, PGSIZE);
                pgt[idx] = V2P(p) | (i == 3 ? PTE_BLOCK : PTE_TABLE);
            } else {
                return 0;
            }
        }
        pgt = P2V(PTE_ADDR(pgt[idx]));
    }
    return pgt;
}

/* Free a user page table and all the physical memory pages. */
void
vm_free(uint64_t *pgdir)
{

}

void
uvm_switch()
{

}

/* Fork a process's page table. */
uint64_t *
vm_fork(uint64_t *pgdir)
{

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
