#ifndef INC_ARM_H
#define INC_ARM_H

#include <stdint.h>

static inline void
delay(int32_t count)
{
    asm volatile("__delay_%=: subs %[count], %[count], #1; bne __delay_%=\n":
                 "=r"(count): [count]"0"(count) : "cc");
}

static inline void
put32(uint64_t p, uint32_t x)
{
    *(volatile uint32_t *)p = x;
}

static inline uint32_t
get32(uint64_t p)
{
    return *(volatile uint32_t *)p;
}

/* Brute-force data and instruction synchronization barrier */
static inline void
disb()
{
    asm volatile("dsb sy; isb");
}

/*
 * Load vector base address register.
 * Note that this is a virtual address.
 */
static inline void
load_vbar_el1(void *p)
{
    disb();
    asm volatile("msr vbar_el1, %[x]": : [x]"r"(p));
    disb();
}

static inline void
load_ttbr0(uint64_t p)
{
    asm volatile("msr ttbr0_el1, %[x]": : [x]"r"(p));
    disb();
    asm volatile("tlbi vmalle1");
    disb();
}

static inline void
load_ttbr1(uint64_t p)
{
    asm volatile("msr ttbr1_el1, %[x]": : [x]"r"(p));
    disb();
    asm volatile("tlbi vmalle1");
    disb();
}

static inline int
cpuid()
{
    int64_t id;
    asm volatile("mrs %[x], mpidr_el1": [x]"=r"(id));
    return id & 0xFF;
}

#endif
