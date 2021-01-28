#ifndef INC_VM_H
#define INC_VM_H

#include <stdint.h>
#include <stddef.h>
#include "proc.h"

uint64_t *vm_init();
void vm_free(uint64_t *pgdir);

void uvm_switch(uint64_t *pgdir);
int uvm_map(uint64_t *pgdir, void *va, size_t sz, uint64_t pa);
int uvm_alloc(uint64_t *pgdir, size_t base, size_t stksz, size_t oldsz, size_t newsz);
int uvm_dealloc(uint64_t *pgdir, size_t base, size_t stksz, size_t oldsz, size_t newsz);

uint64_t *uvm_copy(uint64_t *pgdir);

void vm_stat(uint64_t *);

#endif
