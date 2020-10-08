#ifndef INC_PROC_H
#define INC_PROC_H

#include <stdint.h>
#include "arm.h"
#include "mmu.h"
#include "trap.h"
#include "spinlock.h"

#define NPROC           100
#define NCPU            4

/* Stack must always be 16 bytes aligned. */

struct context {
    uint64_t lr0, lr, fp;
    uint64_t x[10];             /* X28 ... X19 */
    uint64_t padding;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

/* Per-process state */
struct proc {
    uint64_t  *pgdir;           /* User space page table */
    char *kstack;               /* Bottom of kernel stack for this process */
    enum procstate state;       /* Process state */
    int pid;                    /* Process ID */
    struct trapframe *tf;       /* Trap frame for current syscall */
    struct context *context;    /* swtch() here to run process */

    // struct proc *parent;         /* Parent process */

    // void *chan;                  /* If non-zero, sleeping on chan */
    // int killed;                  // If non-zero, have been killed
    // struct file *ofile[NOFILE];  // Open files
    // struct inode *cwd;           // Current directory
    // char name[16];               // Process name (debugging)
};

void user_init();
void scheduler();

/* Per-CPU state */
struct cpu {
    struct context *scheduler;  /* swtch() here to enter scheduler */
    struct proc *proc;          /* The process running on this cpu or null */
    volatile int started;       /* Has the CPU started? */
    struct spinlock lock;
};

extern struct cpu cpu[NCPU];

static inline struct cpu *
thiscpu()
{
    return &cpu[cpuid()];
}

#endif
