#include <syscall.h>
// #include <unistd.h>

#include "trap.h"
#include "console.h"
#include "proc.h"

int
syscall1(struct trapframe *tf)
{
    thisproc()->tf = tf;
    int sysno = tf->x[8];
    switch (sysno) {
    case SYS_execve:
        return execve(tf->x[0], tf->x[1], tf->x[2]);
    case SYS_sched_yield:
        yield();
        return 0;
    case SYS_openat:
        // FIXME: Omit dirfd and mode.
        return sys_open(tf->x[1], tf->x[2]);
    default:
        cprintf("Unexpected syscall #%d\n", sysno);
    }
}

