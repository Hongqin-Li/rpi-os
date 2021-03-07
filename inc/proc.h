#ifndef INC_PROC_H
#define INC_PROC_H

#include <stdint.h>
#include "arm.h"
#include "mmu.h"
#include "trap.h"
#include "spinlock.h"
#include "list.h"

#define NPROC           100
#define NCPU            4
#define NOFILE          16      // Open files per process

/* Stack must always be 16 bytes aligned. */
struct context {
    uint64_t lr0, lr, fp;
    uint64_t x[10];             /* X28 ... X19 */
    uint64_t padding;
    // uint64_t q0[2];             /* V0 */
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

/* Per-process state */
struct proc {
    /* 
     * Memory layout
     *
     * +----------+
     * |  Kernel  | 
     * +----------+  KERNBASE
     * |  Stack   |  
     * +----------+  KERNBASE - stksz
     * |   ....   |
     * |   ....   |
     * +----------+  base + sz
     * |   Heap   |
     * +----------+
     * |   Code   |
     * +----------+  base
     * | Reserved | 
     * +----------+  0
     *
     */
    size_t base, sz;
    size_t stksz;

    void *pgdir;                /* User space page table. */
    void *kstack;               /* Bottom of kernel stack for this process. */
    enum procstate state;       /* Process state. */
    int pid;                    /* Process ID. */
    struct trapframe *tf;       /* Trap frame for current syscall. */
    struct context *context;    /* swtch() here to run process. */
    struct list_head link;      /* linked list of running process. */
    void *chan;                 /* If non-zero, sleeping on chan */

    struct proc *parent;        /* Parent process */
    struct list_head child;     /* Child list of this process. */
    struct list_head clink;     /* Child list of this process. */

    int killed;                  // If non-zero, have been killed
    struct file *ofile[NOFILE];  // Open files
    struct inode *cwd;           // Current directory
    char name[16];               // Process name (debugging)
};

/* Per-CPU state */
struct cpu {
    struct context *scheduler;  /* swtch() here to enter scheduler */
    struct proc *proc;          /* The process running on this cpu or null. */
    struct proc *idle;          /* The idle process. */
    volatile int started;       /* Has the CPU started? */
    struct spinlock lock;
};

extern struct cpu cpu[NCPU];

static inline struct cpu *
thiscpu()
{
    return &cpu[cpuid()];
}

static inline struct proc *
thisproc()
{
    return thiscpu()->proc;
}

void proc_init();
void user_init();
void scheduler();
void sleep(void *chan, struct spinlock *lk);
void wakeup(void *chan);
void yield();
void exit(int);
int  wait();
int  fork();
void procdump();

#endif
