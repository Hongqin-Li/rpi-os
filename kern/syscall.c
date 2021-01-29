#include <syscall.h>
// #include <unistd.h>

#include "trap.h"
#include "console.h"
#include "proc.h"
#include "debug.h"

int
syscall1(struct trapframe *tf)
{
    thisproc()->tf = tf;
    int sysno = tf->x[8];
    switch (sysno) {

    // FIXME: use pid instead of tid since we don't have threads :)
    case SYS_set_tid_address:
        return thisproc()->pid;
    case SYS_gettid:
        return thisproc()->pid;

    // FIXME: Hack TIOCGWINSZ(get window size)
    case SYS_ioctl:
        if (tf->x[1] == 0x5413) return 0;
        else panic("ioctl unimplemented. ");

    // FIXME: always return 0 since we don't have signals  :)
    case SYS_rt_sigprocmask:
        cprintf("sigprocmask: TODO\n");
        return 0;

    case SYS_execve:
        return execve(tf->x[0], tf->x[1], tf->x[2]);

    case SYS_sched_yield:
        return sys_yield();

    case SYS_clone:
        return sys_clone();

    case SYS_wait4:
        return sys_wait4();

    case SYS_dup:
        return sys_dup();

    case SYS_mknodat:
        return sys_mknodat();
        
    case SYS_openat:
        return sys_openat();

    case SYS_writev:
        return sys_writev();

    // FIXME: exit_group should kill every thread in the current thread group.
    case SYS_exit_group:
    case SYS_exit:
        exit(tf->x[0]);

    default:
        // FIXME: don't panic.

        debug_reg();
        panic("Unexpected syscall #%d\n", sysno);
        
        return 0;
    }
}

