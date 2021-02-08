#include "trap.h"

#include "arm.h"
#include "sysregs.h"
#include "mmu.h"
#include "peripherals/irq.h"

#include "uart.h"
#include "console.h"
#include "clock.h"
#include "timer.h"
#include "sd.h"

#include "debug.h"

void
irq_init()
{
    cprintf("- irq init\n");
    put32(ENABLE_IRQS_1, AUX_INT);
    put32(ENABLE_IRQS_2, VC_ARASANSDIO_INT);
    put32(GPU_INT_ROUTE, GPU_IRQ2CORE(0));
}

// Check if a block of memory lies within the process user space.
int
in_user(void *s, size_t n)
{
    struct proc *p = thisproc();
    if ((p->base <= s && s + n <= p->sz) ||
        (USERTOP - p->stksz <= s && s + n <= USERTOP))
        return 1;

    return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Doesn't actually copy the string - just sets *pp to point at it.
// Returns length of string, not including nul.
int
fetchstr(uint64_t addr, char **pp)
{
    struct proc *p = thisproc();
    char *s;
    *pp = s = (char *)addr;
    // cprintf("fetchstr: at 0x%p to 0x%p, p->sz 0x%llx, p->base 0x%llx\n", addr, pp, p->sz, p->base);
    if (p->base <= addr && addr < p->sz) {
        for (; (uint64_t)s < p->sz; s++)
            if (*s == 0) return s - *pp;
    } else if (USERTOP - p->stksz <= addr && addr < USERTOP) {
        for (; (uint64_t)s < USERTOP; s++)
            if (*s == 0) return s - *pp;
    }
    return -1;
}

// Fetch the nth (starting from 0) 32-bit system call argument.
// In our ABI, x8 contains system call index, x0-x5 contain parameters.
// now we support system calls with at most 6 parameters.
int
argint(int n, int *ip)
{
    struct proc *proc = thisproc();
    if (n > 5) {
        cprintf("too many system call parameters\n");
        return -1;
    }
    *ip = proc->tf->x[n];

    return 0;
}

// Fetch the nth (starting from 0) 64-bit system call argument.
// In our ABI, x8 contains system call index, x0-x5 contain parameters.
// now we support system calls with at most 6 parameters.
int
argu64(int n, size_t *ip)
{
    struct proc *proc = thisproc();
    if (n > 5) {
        cprintf("too many system call parameters\n");
        return -1;
    }
    *ip = proc->tf->x[n];

    return 0;
}

// Fetch the nth word-sized system call argument as a pointer
// to a block of memory of size bytes. Check that the pointer
// lies within the process address space.
int
argptr(int n, char **pp, size_t size)
{
    uint64_t i;
    if (argu64(n, &i) < 0) {
        return -1;
    }
    if (in_user((void *)i, size)) {
        *pp = (char*)i;
        return 0;
    } else {
        return -1;
    }
}

// Fetch the nth word-sized system call argument as a string pointer.
// Check that the pointer is valid and the string is nul-terminated.
// (There is no shared writable memory, so the string can't change
// between this check and being used by the kernel.)
int
argstr(int n, char **pp)
{
    uint64_t addr;
    if (argu64(n, &addr) < 0)
        return -1;
    int r = fetchstr(addr, pp);
    return r;
}

void
interrupt(struct trapframe *tf)
{
    int src = get32(IRQ_SRC_CORE(cpuid()));
    if (src & IRQ_CNTPNSIRQ) {
        timer_reset();
        timer();
    } else if (src & IRQ_TIMER) {
        clock_reset();
        clock();
    } else if (src & IRQ_GPU) {
        int p1 = get32(IRQ_PENDING_1), p2 = get32(IRQ_PENDING_2);
        if (p1 & AUX_INT) {
            uart_intr();
        } else if (p2 & VC_ARASANSDIO_INT) {
            sd_intr();
        } else {
            warn("unexpected gpu intr p1 %x, p2 %x, sd %d, omitted", p1, p2, p2 & VC_ARASANSDIO_INT);
            panic(""); // FIXME:
        }
    } else {
        warn("unexpected interrupt at cpu %d", cpuid());
        panic(""); // FIXME:
    }
}

void
trap(struct trapframe *tf)
{
    acquire_kern();

#ifdef DEBUG
    uint64_t sp, sp0;
    uint64_t elr, esr, far;
    if (thisproc() != thiscpu()->idle) {
        trace("%s(%d) trap on cpu %d", thisproc()->name, thisproc()->pid, cpuid());
        trace("fp 0x%p, sp 0x%p, elr 0x%p", tf->x[29], tf->sp, tf->elr);
    }
#endif

    int ec = resr() >> EC_SHIFT, iss = resr() & ISS_MASK;
    lesr(0);  /* Clear esr. */
    switch (ec) {
    case EC_UNKNOWN:
        interrupt(tf);
        break;
        
    case EC_SVC64:
        if (iss == 0) {
            tf->x[0] = syscall1(tf);
        } else {
            warn("unexpected svc iss 0x%x", iss);
            panic(""); // FIXME:
        }
        break;

    default:
#ifdef DEBUG
        sp0 = tf->sp;
        elr = tf->elr;
        if (sp0 < USERTOP && in_user(sp0, USERTOP - sp0))
            debug_mem(sp0, USERTOP - sp0);
        if (in_user(elr, 0x10))
            debug_mem(elr, 0x10);
        for (int i = 0; i < 32; i++) {
            debug("x[%d] = 0x%llx(%lld)", i, tf->x[i], tf->x[i]);
        }
        debug("ec: %llx, iss: %llx", ec, iss);
        debug_reg();
        vm_stat(thisproc()->pgdir);
        debug("trap: proc '%s' terminated on cpu %d", thisproc()->name, cpuid());
#endif
        exit(1);
    }

#ifdef DEBUG
    if (thisproc() != thiscpu()->idle)
        trace("%s(%d) trap finish on cpu %d", thisproc()->name, thisproc()->pid, cpuid());
#endif
    disb();
    release_kern();
}

void
irq_error(uint64_t type)
{
    cprintf("- irq_error\n");
    debug_reg();
    panic("irq of type %d unimplemented. \n", type);
}
