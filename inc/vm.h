#ifndef INC_VM_H
#define INC_VM_H

#include <stdint.h>
#include <stddef.h>
#include "proc.h"

uint64_t *vm_init();
void uvm_switch(uint64_t *pgdir);
int uvm_map(uint64_t *pgdir, void *va, size_t sz, uint64_t pa);

#endif
