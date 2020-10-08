#ifndef INC_TRAP_H
#define INC_TRAP_H

#include <stdint.h>

struct trapframe {
    uint64_t sp, spsr, elr;
    uint64_t x[31];
};

void trap(struct trapframe *);
void irq_init();
void irq_error();

#endif
