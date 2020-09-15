#include <stdint.h>

#include "memlayout.h"
#include "mmu.h"

__attribute__((__aligned__(PGSIZE)))
uint64_t entry_pud[512] = { MMU_FLAGS };

__attribute__((__aligned__(PGSIZE)))
uint64_t entry_pgd[512] = { V2P(entry_pud) + MM_TYPE_PAGE };
