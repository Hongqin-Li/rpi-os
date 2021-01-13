#include <syscall.h>
// #include <unistd.h>

#include "console.h"
#include "trap.h"
#include "sd.h"
#include "debug.h"

int
execve(const char *path, char *const argv[], char *const envp[])
{
    cprintf("- execve: path=0x%p, argv=0x%p, envp=0x%p\n", path, argv, envp);
    // TODO: check path

    return 0xad;
}

int
syscall1(struct trapframe *tf)
{
    int sysno = tf->x[8];
    switch (sysno) {
    case SYS_execve:
        return execve(tf->x[0], tf->x[1], tf->x[2]);
    case SYS_sched_yield:
        yield();
        return 0;
    default:
        cprintf("Unexpected syscall #%d\n", sysno);
    }
}
