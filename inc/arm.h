#ifndef INC_ARM_H
#define INC_ARM_H

#include <stdint.h>

/* Wait N CPU cycles. */
static inline void
delay(int32_t count)
{
    asm volatile("__delay_%=: subs %[count], %[count], #1; bne __delay_%=\n":
                 "=r"(count): [count]"0"(count) : "cc");
}

/* Wait N microsec. */
static inline void
delayus(uint32_t n)
{
    uint64_t f, t, r;
    /* Get the current counter frequency */
    asm volatile ("mrs %[freq], cntfrq_el0" : [freq]"=r"(f));
    /* Read the current counter. */
    asm volatile ("mrs %[cnt], cntpct_el0" : [cnt]"=r"(t));
    /* Calculate expire value for counter */
    t += f / 1000000 * n;
    do {
        asm volatile ("mrs %[cnt], cntpct_el0" : [cnt]"=r"(r));
    } while (r < t);
}

static inline uint64_t
timestamp()
{
    uint64_t t;
    asm volatile ("mrs %[cnt], cntpct_el0" : [cnt]"=r"(t));
    return t;
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

/* Data synchronization barrier. */
static inline void
dsb()
{
    asm volatile("dsb sy" ::: "memory");
}

/* Brute-force data and instruction synchronization barrier. */
static inline void
disb()
{
    asm volatile("dsb sy; isb" ::: "memory");
}

/* Data cache clean and invalidate by virtual address to point of coherency. */
static inline void
dccivac(void *p, int n)
{
    disb();
    while (n--) {
        asm volatile("dc civac, %[x]" : : [x]"r"(p + n));
        asm volatile("dc cvau, %[x]" : : [x]"r"(p + n));
    }
    disb();
}

/* Read Exception Syndrome Register (EL1) */
static inline uint64_t
resr()
{
    disb();
    uint64_t r;
    asm volatile("mrs %[x], esr_el1" : [x]"=r"(r));
    disb();
    return r;
}

/* Read Exception Link Register (EL1) */
static inline uint64_t
relr()
{
    disb();
    uint64_t r;
    asm volatile("mrs %[x], elr_el1" : [x]"=r"(r));
    disb();
    return r;
}

/* Load Exception Syndrome Register (EL1) */
static inline void
lesr()
{
    disb();
    asm volatile("msr esr_el1, %[x]" : : [x]"r"(0));
    disb();
}

/* Load vector base (virtual) address register (EL1) */
static inline void
lvbar(void *p)
{
    disb();
    asm volatile("msr vbar_el1, %[x]" : : [x]"r"(p));
    disb();
}

static inline void
tlbi1()
{
    disb();
    asm volatile("tlbi vmalle1is");
    disb();
}

/* Load Translation Table Base Register 0 (EL1) */
static inline void
lttbr0(uint64_t p)
{
    disb();
    asm volatile("msr ttbr0_el1, %[x]" : : [x]"r"(p));
    tlbi1();
}

/* Load Translation Table Base Register 1 (EL1) */
static inline void
lttbr1(uint64_t p)
{
    disb();
    asm volatile("msr ttbr1_el1, %[x]" : : [x]"r"(p));
    tlbi1();
}

static inline int
cpuid()
{
    int64_t id;
    asm volatile("mrs %[x], mpidr_el1" : [x]"=r"(id));
    return id & 0xFF;
}

#endif
