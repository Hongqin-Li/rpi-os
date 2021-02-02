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

// User code makes a system call with INT T_SYSCALL. System call number
// in r0. Arguments on the stack, from the user call to the C library
// system call function. The saved user sp points to the first argument.

// Fetch the int at addr from the current process.
// int
// fetchint(uint64_t addr, long *ip)
// {
//     struct proc *proc = thisproc();
//     if(addr >= proc->sz || addr+8 > proc->sz) {
//         return -1;
//     }
// 
//     *ip = *(long*)(addr);
//     return 0;
// }

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
// In our ABI, x8 contains system call index, x0-r3 contain parameters.
// now we support system calls with at most 4 parameters.
int
argint(int n, int *ip)
{
    struct proc *proc = thisproc();
    if (n > 3) {
        cprintf("too many system call parameters\n");
        return -1;
    }
    *ip = proc->tf->x[n];

    return 0;
}

// Fetch the nth (starting from 0) 64-bit system call argument.
// In our ABI, x8 contains system call index, x0-r3 contain parameters.
// now we support system calls with at most 4 parameters.
int
argu64(int n, size_t *ip)
{
    struct proc *proc = thisproc();
    if (n > 3) {
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
trap(struct trapframe *tf)
{
    int src = get32(IRQ_SRC_CORE(cpuid()));
    if (src & IRQ_CNTPNSIRQ)
        timer(), timer_reset();
    else if (src & IRQ_TIMER)
        clock(), clock_reset();
    else if (src & IRQ_GPU) {
        int p1 = get32(IRQ_PENDING_1), p2 = get32(IRQ_PENDING_2);
        if (p1 & AUX_INT) {
            uart_intr();
        } else if (p2 & VC_ARASANSDIO_INT) {
            sd_intr();
        } else {
            cprintf("trap: unexpected gpu intr p1 %x, p2 %x, sd %d, omitted\n", p1, p2, p2 & VC_ARASANSDIO_INT);
        }
    } else {
        int ec = resr() >> EC_SHIFT, iss = resr() & ISS_MASK;
        switch (ec) {
        case EC_SVC64:
            if (iss == 0) {
                tf->x[0] = syscall1(tf);
            } else {
                cprintf("Unexpected svc number %d, omitted.\n", iss);
            }
            lesr(0);  /* Clear esr. */
            break;
        default:
bad:
            debug_reg();
            cprintf("trap: IRQ_SRC_CORE(%d): 0x%x\n", cpuid(), src);
            cprintf("trap: IRQ_PENDING_1: 0x%x\n", get32(IRQ_PENDING_1));
            cprintf("trap: IRQ_PENDING_2: 0x%x\n", get32(IRQ_PENDING_2));
            cprintf("trap: unexpected irq\n");
            vm_stat(thisproc()->pgdir);
            cprintf("trap: proc '%s' terminated\n", thisproc()->name);
            lesr(0);
            exit(1);
        }
    }
    disb();
}

void
irq_error(uint64_t type)
{
    cprintf("- irq_error\n");
    debug_reg();
    panic("irq of type %d unimplemented. \n", type);
}
