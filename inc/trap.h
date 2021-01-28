#ifndef INC_TRAP_H
#define INC_TRAP_H

#include <stdint.h>

struct trapframe {
    uint64_t sp, spsr, elr;
    uint64_t x[31];
    uint64_t q0[2]; // FIXME: dirty hack since musl's `memset` only used q0.
};

void trap(struct trapframe *);
void irq_init();
void irq_error();
extern int syscall1(struct trapframe *);

#endif
