#include <syscall.h>
// #include <unistd.h>

#include "trap.h"
#include "console.h"
#include "proc.h"
#include "debug.h"

struct iovec {
    void  *iov_base;    /* Starting address. */
    size_t iov_len;     /* Number of bytes to transfer. */
};

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

    case SYS_set_tid_address:
        // FIXME: use pid instead of tid since threading is unimplemented.
        return thisproc()->pid;
    case SYS_ioctl:
        // FIXME: hack TIOCGWINSZ.
        if (tf->x[1] == 0x5413) return 0;
        else panic("ioctl unimplemented. ");

    case SYS_writev:
        return sys_writev();

    case SYS_exit_group:
        // FIXME: exit_group should kill every thread in the current thread group.
    case SYS_exit:
        exit(tf->x[0]);

    default:
        // FIXME: donot panic.

        debug_reg();
        panic("Unexpected syscall #%d\n", sysno);
        
        return 0;
    }
}

