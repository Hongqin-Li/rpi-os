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
#include "file.h"
#include "log.h"

extern void trapret();
extern void swtch(struct context **old, struct context *new);

static void forkret();
static void idle_init();

#define SQSIZE  0x100    /* Must be power of 2. */
#define HASH(x) ((((int)(x)) >> 5) & (SQSIZE - 1))

struct cpu cpu[NCPU];

struct {
  struct proc proc[NPROC];
  struct list_head slpque[SQSIZE];
  struct list_head sched_que;
  struct spinlock lock;
} ptable;

struct proc *initproc;
static int pid = 0;

void
proc_init()
{
    list_init(&ptable.sched_que);
    for (int i = 0; i < SQSIZE; i++)
        list_init(&ptable.slpque[i]);
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
    struct proc *p;
    int found = 0;

    acquire(&ptable.lock);
    for (p = ptable.proc; p < ptable.proc + NPROC; p++) {
        if (p->state == UNUSED) {
            memset(p, 0, sizeof(*p));
            p->pid = ++pid;
            found = 1;
            break;
        }
    }

    if (!found || !(p->kstack = kalloc())) {
        release(&ptable.lock);
        return 0;
    }

    p->state = EMBRYO;
    release(&ptable.lock);

    p->name[0] = 0;

    void *sp = p->kstack + PGSIZE;
    assert(sizeof(*p->tf) == 19*16 && sizeof(*p->context) == 7*16);

    sp -= sizeof(*p->tf);
    p->tf = sp;
    /* No user stack for init process. */
    p->tf->spsr = p->tf->sp = 0;

    sp -= sizeof(*p->context);
    p->context = sp;
    p->context->lr0 = (uint64_t)forkret;
    p->context->lr = (uint64_t)trapret;

    list_init(&p->child);
   
    return p;
}

/* Initialize per-cpu idle process. */
static void
idle_init()
{
    struct proc *p;
    if (!(p = proc_alloc()))
        panic("idle init: failed\n");

    p->pgdir = vm_init();
    void *va = kalloc();
    uvm_map(p->pgdir, 0, PGSIZE, V2P(va));

    extern char ispin[], eicode[];
    memmove(va, ispin, eicode - ispin);
    assert((size_t)(eicode - ispin) <= PGSIZE);

    p->stksz = 0;
    p->sz = PGSIZE;
    p->base = 0;

    p->tf->elr = 0;

    safestrcpy(p->name, "idle", sizeof(p->name));
    // p->cwd = namei("/");
    thiscpu()->idle = p;
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
    struct proc *p = proc_alloc();
    if (p == 0)
        panic("user init: failed\n");

    if ((p->pgdir = vm_init()) == 0)
        panic("user init: failed\n");

    assert(!initproc);
    initproc = p;

    void *va = kalloc();
    uvm_map(p->pgdir, 0, PGSIZE, V2P(va));

    extern char icode[], eicode[];
    memmove(va, icode, eicode - icode);
    assert((size_t)(eicode - icode) <= PGSIZE);
    p->stksz = 0;
    p->sz = PGSIZE;
    p->base = 0;

    p->tf->elr = 0;

    safestrcpy(p->name, "icode", sizeof(p->name));
    p->cwd = namei("/");

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
        } else {
            p = container_of(list_front(head), struct proc, link);
            list_pop_front(head);
        }
        uvm_switch(p->pgdir);
        thiscpu()->proc = p;
        swtch(&thiscpu()->scheduler, p->context);
        release(&ptable.lock);
    }
}


/*
 * A fork child's very first scheduling by scheduler()
 * will swtch here. "Return" to user space.
 */
static void
forkret()
{
    static int first = 1;
    if (first && thisproc() != thiscpu()->idle) {
        first = 0;
        release(&ptable.lock);
        iinit(ROOTDEV);
        initlog(ROOTDEV);
    } else {
        release(&ptable.lock);
    }
    trace("proc '%s'", thisproc()->name);
}

/* Give up CPU. */
void
yield()
{
    struct proc *p = thisproc();
    acquire(&ptable.lock);
    if (p != thiscpu()->idle)
        list_push_back(&ptable.sched_que, &p->link);
    p->state = RUNNABLE;
    swtch(&p->context, thiscpu()->scheduler);
    p->state = RUNNING;
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

    p->state = SLEEPING;
    // cprintf("- cpu %d: sleep pid %d on chan 0x%p\n", cpuid(), p->pid, chan);
    swtch(&thisproc()->context, thiscpu()->scheduler);
    // cprintf("- cpu %d: wake on chan 0x%p\n", cpuid(), chan);
    p->state = RUNNING;

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

/*
 * Create a new process copying p as the parent.
 * Sets up stack to return as if from system call.
 * Caller must set state of returned proc to RUNNABLE.
 */
int
fork()
{
    struct proc *cp = thisproc();
    struct proc *np = proc_alloc();

    if (np == 0)
        return -1;

    if ((np->pgdir = uvm_copy(cp->pgdir)) == 0) {
        kfree(np->kstack);

        acquire(&ptable.lock);
        np->state = UNUSED;
        release(&ptable.lock);

        return -1;
    }

    np->parent = cp;

    np->base = cp->base;
    np->sz = cp->sz;
    np->stksz = cp->stksz;

    memmove(np->tf, cp->tf, sizeof(*np->tf));

    // Fork returns 0 in the child.
    np->tf->x[0] = 0;

    for (int i = 0; i < NOFILE; i++)
        if (cp->ofile[i])
            np->ofile[i] = filedup(cp->ofile[i]);
    np->cwd = idup(cp->cwd);

    int pid = np->pid;

    acquire(&ptable.lock);
    list_push_back(&cp->child, &np->clink);
    list_push_back(&ptable.sched_que, &np->link);
    np->state = RUNNABLE;
    release(&ptable.lock);

    return pid;
}


/*
 * Wait for a child process to exit and return its pid.
 * Return -1 if this process has no children.
 */
int
wait()
{   
    struct proc *cp = thisproc();

    struct list_head *q = &cp->child;
    struct proc *p, *np;

    acquire(&ptable.lock);
    while (!list_empty(q)) {
        LIST_FOREACH_ENTRY_SAFE(p, np, q, clink) {
            if (p->state == ZOMBIE) {
                assert(p->parent == cp);

                list_drop(&p->clink);

                kfree(p->kstack);
                vm_free(p->pgdir);
                p->state = UNUSED;

                int pid = p->pid;
                release(&ptable.lock);
                return pid;
            }
        }
        sleep(cp, &ptable.lock);
    }
    release(&ptable.lock);
    return -1;
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

/*
 * Exit the current process.  Does not return.
 * An exited process remains in the zombie state
 * until its parent calls wait() to find out it exited.
 */
void
exit(int err)
{
    struct proc *cp = thisproc();
    if (cp == initproc)
        panic("init exit");

    if (err) {
        cprintf("exit: pid %d, err %d\n", cp->pid, err);
        procdump();
    }

    // Close all open files.
    for (int fd = 0; fd < NOFILE; fd++) {
        if (cp->ofile[fd]) {
            fileclose(cp->ofile[fd]);
            cp->ofile[fd] = 0;
        }
    }

    begin_op();
    iput(cp->cwd);
    end_op();
    cp->cwd = 0;

    acquire(&ptable.lock);

    // Parent might be sleeping in wait().
    wakeup1(cp->parent);

    // Pass abandoned children to init.
    struct list_head *q = &cp->child;
    struct proc *p, *np;
    LIST_FOREACH_ENTRY_SAFE(p, np, q, clink) {
        assert(p->parent == cp);
        // cprintf("exit: pass child %d to init\n", p->pid);
        p->parent = initproc;

        list_drop(&p->clink);
        list_push_back(&initproc->child, &p->clink);
        if (p->state == ZOMBIE)
            wakeup1(initproc);
    }
    assert(list_empty(q));
 
    // Jump into the scheduler, never to return.
    cp->state = ZOMBIE;

    swtch(&cp->context, thiscpu()->scheduler);
    panic("zombie exit");
}

/*
 * Print a process listing to console. For debugging.
 * Runs when user types ^P on console.
 */
void
procdump()
{
    static char *states[] = {
        [UNUSED]    "unused",
        [EMBRYO]    "embryo",
        [SLEEPING]  "sleep ",
        [RUNNABLE]  "runble",
        [RUNNING]   "run   ",
        [ZOMBIE]    "zombie"
    };
    int i;
    struct proc *p;
    char *state;

    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state == UNUSED) continue;
        cprintf("%d %s %s\n", p->pid, states[p->state], p->name);
    }
    release(&ptable.lock);
}
