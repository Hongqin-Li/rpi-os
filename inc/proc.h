#ifndef INC_PROC_H
#define INC_PROC_H

#include <stdint.h>

#define NPROC 100

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

/* Per-process state */
struct proc {
    uint64_t  *pgdir;            // Page table
    char *kstack;                // Bottom of kernel stack for this process
    enum procstate state;        // Process state
    int pid;                     // Process ID
    struct proc *parent;         // Parent process
    struct trapframe *tf;        // Trap frame for current syscall
    struct context *context;     // swtch() here to run process
    void *chan;                  // If non-zero, sleeping on chan
    int killed;                  // If non-zero, have been killed
    // struct file *ofile[NOFILE];  // Open files
    // struct inode *cwd;           // Current directory
    char name[16];               // Process name (debugging)
};

#endif
