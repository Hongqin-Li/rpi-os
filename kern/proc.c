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

static void user_init();
static void idle_init();

#define SQSIZE  0x100    /* Must be power of 2. */
#define HASH(x) ((((int)(x)) >> 5) & (SQSIZE - 1))

struct cpu cpu[NCPU];

struct {
  struct list_head slpque[SQSIZE];
  struct list_head sched_que;
  struct spinlock lock;
} ptable;

void
proc_init()
{
    list_init(&ptable.sched_que);
    for (int i = 0; i < SQSIZE; i++)
        list_init(&ptable.slpque[i]);
    // FIXME: 
    user_init();
}


/*
 * A fork child's very first scheduling by scheduler()
 * will swtch here. "Return" to user space.
 */
static void
forkret()
{
    static int first = 1;
    release(&ptable.lock);
    cprintf("- forkret\n");
    if (first) {
        first = 0;
        cprintf("iinit...\m");
        iinit(ROOTDEV);
        cprintf("iinit done!\m");
        cprintf("initlog...\m");
        initlog(ROOTDEV);
        cprintf("initlog done!\m");
    }
    return;
}

// TODO: use kmalloc
/*
 * Look in the process table for an UNUSED proc.
 * If found, change state to EMBRYO and initialize
 * state required to run in the kernel.
 * Otherwise return 0.
 */
static struct proc *
proc_alloc()
{
    static struct proc proc[NPROC];
    struct proc *p;
    int found = 0;

    acquire(&ptable.lock);
    for (p = proc; p < &proc[NPROC]; p++) {
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
    p->pid = p - proc;
    cprintf("- proc alloc: pid %d\n", p->pid);
    p->pgdir = vm_init();

    void *sp = p->kstack + PGSIZE;
    assert(sizeof(*p->tf) == 17*16 && sizeof(*p->context) == 7*16);

    sp -= sizeof(*p->tf);
    p->tf = sp;
    /* No user stack for init process. */
    p->tf->spsr = p->tf->sp = 0;

    sp -= sizeof(*p->context);
    p->context = sp;
    p->context->lr0 = (uint64_t)forkret;
    p->context->lr = (uint64_t)trapret;
   
    release(&ptable.lock);
    return p;
}

/* Initialize per-cpu idle process. */
static void
idle_init()
{
    cprintf("- idle init\n");

    struct proc *p;
    while (!(p = proc_alloc())) ;

    void *va = kalloc();
    uvm_map(p->pgdir, 0, PGSIZE, V2P(va));

    extern char ispin[], eicode[];
    memmove(va, ispin, eicode - ispin);
    assert((size_t)(eicode - ispin) <= PGSIZE);

    p->tf->elr = 0;

    thiscpu()->idle = p;
}

/*
 * Set up the first user process. Specifically, we need to:
 * - alloc a new proc.
 * - move the code snippet in initcode.S into its virtual memory.
 * - set up link register in trap frame.
 * - mark as RUNNABLE so that our scheduler can swtch to it.
 */
static void
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
    p->sz = PGSIZE;

    p->tf->elr = 0;

    acquire(&ptable.lock);
    list_push_back(&ptable.sched_que, &p->link);
    release(&ptable.lock);
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
    idle_init();
    for (struct proc *p; ; ) {
        acquire(&ptable.lock);
        struct list_head *head = &ptable.sched_que;
        if (list_empty(head)) {
            p = thiscpu()->idle;
            // cprintf("- scheduler: cpu %d to idle\n", cpuid());
        } else {
            p = container_of(list_front(head), struct proc, link);
            list_pop_front(head);
            // cprintf("- scheduler: cpu %d to pid %d\n", cpuid(), p->pid);
        }
        uvm_switch(p->pgdir);
        thiscpu()->proc = p;
        swtch(&thiscpu()->scheduler, p->context);
        // cprintf("- scheduler: cpu %d back to scheduler from pid %d\n", cpuid(), p->pid);
        thiscpu()->proc = 0;
        release(&ptable.lock);
    }
}

/* Give up CPU. */
void
yield()
{
    struct proc *p = thisproc();
    acquire(&ptable.lock);
    if (p != thiscpu()->idle)
        list_push_back(&ptable.sched_que, &p->link);
    swtch(&p->context, thiscpu()->scheduler);
    release(&ptable.lock);
}

/*
 * Atomically release lock and sleep on chan.
 * Reacquires lock when awakened.
 */
void
sleep(void *chan, struct spinlock *lk)
{
    struct proc *p = thisproc();
    int i = HASH(chan);
    assert(i < SQSIZE);

    if (lk != &ptable.lock) {
        acquire(&ptable.lock);
        release(lk);
    }

    p->chan = chan;
    list_push_back(&ptable.slpque[i], &p->link);

    // cprintf("- cpu %d: sleep pid %d on chan 0x%p\n", cpuid(), p->pid, chan);
    swtch(&thisproc()->context, thiscpu()->scheduler);
    // cprintf("- cpu %d: wake on chan 0x%p\n", cpuid(), chan);

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
    struct proc *p, *np;

    LIST_FOREACH_ENTRY_SAFE(p, np, q, link) {
        // cprintf("- wakeup1: try pid %d\n", p->pid);
        if (p->chan == chan) {
            // cprintf("- wakeup1: pid %d\n", p->pid);
            list_drop(&p->link);
            list_push_back(&ptable.sched_que, &p->link);
        }
    }

}

/* Wake up all processes sleeping on chan. */
void
wakeup(void *chan)
{
    // cprintf("- wakeup: chan 0x%p\n", chan);
    acquire(&ptable.lock);
    wakeup1(chan);
    release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
    panic("not implemented");
//   struct proc *p;

//   acquire(&ptable.lock);
//   for(p = &proc; p < &proc[NPROC]; p++){
//     if(p->pid == pid){
//       p->killed = 1;
//       // Wake process from sleep if necessary.
//       if(p->state == SLEEPING)
//         p->state = RUNNABLE;
//       release(&ptable.lock);
//       return 0;
//     }
//   }
//   release(&ptable.lock);
//   return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
    panic("not implemented");
//   static char *states[] = {
//   [UNUSED]    "unused",
//   [EMBRYO]    "embryo",
//   [SLEEPING]  "sleep ",
//   [RUNNABLE]  "runble",
//   [RUNNING]   "run   ",
//   [ZOMBIE]    "zombie"
//   };
//   int i;
//   struct proc *p;
//   char *state;
//   uint pc[10];

//   for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
//     if(p->state == UNUSED)
//       continue;
//     if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
//       state = states[p->state];
//     else
//       state = "???";
//     cprintf("%d %s %s", p->pid, state, p->name);
//     if(p->state == SLEEPING){
//       getcallerpcs((uint*)p->context->ebp+2, pc);
//       for(i=0; i<10 && pc[i] != 0; i++)
//         cprintf(" %p", pc[i]);
//     }
//     cprintf("\n");
//   }
}
