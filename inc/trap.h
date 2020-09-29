#ifndef INC_TRAP_H
#define INC_TRAP_H

#include <stdint.h>

struct trapframe {
    uint64_t x[31];
    uint64_t pc, sp;
    uint64_t spsr, elr;
};

void irq_init();
// void timer_init();
void generic_timer_init();

void irq_error();
// void irq_timer();
void irq_generic_timer();

void clock();
void trap(struct trapframe *);

#endif
