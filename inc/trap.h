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
void trap_init();

/* In kern/syscall.c */
extern int in_user(void *base, size_t size);
extern int argint(int n, int *ip);
extern int argu64(int n, uint64_t *ip);
extern int argptr(int n, char **pp, size_t size);
extern int argstr(int n, char **pp);
extern int fetchstr(uint64_t addr, char **pp);
extern int syscall1(struct trapframe *);

#endif
