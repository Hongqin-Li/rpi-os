#ifndef INC_TRAP_H
#define INC_TRAP_H

#include <stdint.h>
#include <stddef.h>

struct trapframe {
    uint64_t spsr, elr, sp, tpidr;
    uint64_t x[31];
    uint64_t padding;
    uint64_t q0[2]; // FIXME: dirty hack since musl's `memset` only used q0.
};

void trap(struct trapframe *);
void irq_init();
void irq_error();
extern int syscall1(struct trapframe *);

// Validation of syscall arguments.
int in_user(void *base, size_t size);
int argint(int n, int *ip);
int argu64(int n, uint64_t *ip);
int argptr(int n, char **pp, size_t size);
int argstr(int n, char **pp);
int fetchstr(uint64_t addr, char **pp);

#endif
