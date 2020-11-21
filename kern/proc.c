#include "proc.h"

#include "string.h"
#include "types.h"
#include "memlayout.h"
#include "list.h"
#include "console.h"
#include "mm.h"
#include "vm.h"
#include "spinlock.h"

#include "sd.h"
#include "debug.h"

extern void trapret();
extern void swtch(struct context **old, struct context *new);

#define SQSIZE  0x100    /* Must be power of 2. */
#define HASH(x) ((((int)(x)) >> 5) & (SQSIZE - 1))

struct cpu cpu[NCPU];

struct {
  struct list_head slpque[SQSIZE];
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

void
proc_init()
{
    for (int i = 0; i < SQSIZE; i++) list_init(&ptable.slpque[i]);
}


/*
 * A fork child's very first scheduling by scheduler()
 * will swtch here. "Return" to user space.
 */
static void
forkret()
{
    cprintf("- forkret\n");
    static int first = 1;
    if (first) {
        static struct buf b;
        b.flags = 0;
        b.blockno = 0;
       
        sd_start(&b);
        // memset(buf, 0xAC, sizeof(buf));
        // sdTransferBlocks(0, 1, buf, 1);
        // sdTransferBlocks(0, 1, buf, 0);
        debug_mem(b.data, sizeof(b.data));
        // b.flags = B_DIRTY;
        // sd_start(&b);
        // debug_mem(b.data, sizeof(b.data));
        sd_test();

        first = 0;
    }

    release(&ptable.lock);
    cprintf("- forkret finish\n");
    return;
}

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
    p->pgdir = vm_init();

    void *sp = p->kstack + PGSIZE;
    assert(sizeof(*p->tf) == 17*16 && sizeof(*p->context) == 7*16);

    sp -= sizeof(*p->tf);
    p->tf = sp;
    /* No user stack for init process. */
    p->tf->spsr = p->tf->sp = 0;

    sp -= sizeof(*p->context);
    p->context = sp;
    p->context->lr0 = forkret;
    p->context->lr = trapret;
   
    release(&ptable.lock);
    return p;
}

/*
 * Per-CPU process scheduler.
 * Each CPU calls scheduler() after setting itself up.
 * Scheduler never returns. It loops, doing:
 * - choose a process to run
 * - swtch to start running that process
 * - eventually that process transfers control
 *   via swtch back to the scheduler.
 */
void
scheduler()
{
    while (1) {
        acquire(&ptable.lock);
        for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->state == RUNNABLE) {
                p->state = RUNNING;
                thiscpu()->proc = p;
                uvm_switch(p->pgdir);
                swtch(&thiscpu()->scheduler, p->context);
            }
        }
        release(&ptable.lock);
    }
}

/*
 * Set up the first user process. Specifically, we need to:
 * - alloc a new proc.
 * - move the code snippet in initcode.S into its virtual memory.
 * - set up link register in trap frame.
 * - mark as RUNNABLE so that our scheduler can swtch to it.
 */
void
user_init()
{
    cprintf("- user init\n");

    struct proc *p;
    while (!(p = proc_alloc())) ;

    void *va = kalloc();
    uvm_map(p->pgdir, 0, PGSIZE, V2P(va));

    extern char icode[], eicode[];
    memmove(va, icode, eicode - icode);
    assert((size_t)(eicode - icode) <= PGSIZE);

    p->tf->elr = 0;

    acquire(&ptable.lock);
    p->state = RUNNABLE;
    release(&ptable.lock);
}

/*
 * Atomically release lock and sleep on chan.
 * Reacquires lock when awakened.
 */
void
sleep(void *chan, struct spinlock *lk)
{
    struct list_head t;
    int i = HASH(chan);
    assert(i < SQSIZE);

    if (lk != &ptable.lock) {
        acquire(&ptable.lock);
        release(lk);
    }

    list_push_back(&ptable.slpque[i], &t);

    swtch(&thisproc()->context, thiscpu()->scheduler);

    assert(list_find(&ptable.slpque[i], &t) == &t);
    list_drop(list_find(&ptable.slpque[i], &t));

    if (lk != &ptable.lock) {
        acquire(lk);
        release(&ptable.lock);
    }
}

/*
 * Wake up all processes sleeping on chan.
 * The ptable lock must be held.
 */
static void
wakeup1(void *chan)
{
    struct list_head *q = &ptable.slpque[HASH(chan)];
    struct proc *p;
    LIST_FOREACH_ENTRY(p, q, link) {
        if (p->chan == chan) {
            list_drop(&p->link);
            p->state = RUNNABLE;
        }
    }

}

/* Wake up all processes sleeping on chan. */
void
wakeup(void *chan)
{
    acquire(&ptable.lock);
    wakeup1(chan);
    release(&ptable.lock);
}
