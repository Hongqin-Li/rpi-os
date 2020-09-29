#include "proc.h"
#include "spinlock.h"

// #define SQSIZE 0x100    /* Must be power of 2 */
// #define HASH(x) (((int)x >> 5) & (SQSIZE - 1))
// struct proc *slpque[SQSIZE];

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

/*
 * Look in the process table for an UNUSED proc.
 * If found, change state to EMBRYO and initialize
 * state required to run in the kernel.
 * Otherwise return 0.
 */
static struct proc *
proc_alloc()
{
    struct proc *p;
    int found = 0;

    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state == UNUSED) {
            found = 1;
            break;
        }
    }

    if (!found || !(p->kstack = kalloc())) {
        release(&ptable.lock);
        return 0;
    }

    p->state = EMBRYO;
    p->pid = p - ptable.proc;

    release(&ptable.lock);

    return p;
}

/* Set up the first user process. */
void
user_init()
{

}
