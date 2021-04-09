#ifndef INC_ARM_H
#define INC_ARM_H

#include <stdint.h>

/* Prevent compiler from reordering. */
static inline void
barrier()
{
    asm volatile("" ::: "memory");
}

static inline uint64_t
timerfreq()
{
    uint64_t f;
    asm volatile ("mrs %[freq], cntfrq_el0" : [freq]"=r"(f));
    return f;
}

static inline uint64_t
timestamp()
{
    uint64_t t;
    barrier();
    asm volatile ("mrs %[cnt], cntpct_el0" : [cnt]"=r"(t));
    barrier();
    return t;
}

/* Wait n CPU cycles. */
static inline void
delay(uint32_t n)
{
    asm volatile("__delay_%=: subs %[n], %[n], #1; bne __delay_%=\n":
                 "=r"(n): [n]"0"(n) : "cc");
}

/* Wait n microsec. */
static inline void
delayus(uint32_t n)
{
    uint64_t f = timerfreq(), t = timestamp(), r;
    /* Calculate expire value for counter. */
    t += f / 1000000 * n;
    do {
        r = timestamp();
    } while (r < t);
}

/* Instruction synchronization barrier. */
static inline void
isb()
{
    asm volatile("isb" ::: "memory");
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
    dsb();
    isb();
}

/* Don't need dmb since device region is marked as nGnRnE by kpgdir. */
static inline void
put32(uint64_t p, uint32_t x)
{
    barrier();
    *(volatile uint32_t *)p = x;
    barrier();
}

static inline uint32_t
get32(uint64_t p)
{
    barrier();
    uint32_t val = *(volatile uint32_t *)p;
    barrier();
    return val;
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
