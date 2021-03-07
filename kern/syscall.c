#include <syscall.h>
// #include <unistd.h>

#include "trap.h"
#include "console.h"
#include "proc.h"
#include "debug.h"

extern int sys_brk();
extern int sys_mmap();
extern int sys_wait4();
extern int sys_yield();

extern int sys_execve();

extern int sys_dup();
extern int sys_chdir();
extern int sys_pipe2();
extern int sys_clone();
extern int sys_fstat();
extern int sys_fstatat();
extern int sys_open();
extern int sys_openat();
extern int sys_mkdirat();
extern int sys_mknodat();
extern int sys_close();
extern int sys_writev();
extern int sys_read();

int
syscall1(struct trapframe *tf)
{
    thisproc()->tf = tf;
    int sysno = tf->x[8];
    switch (sysno) {

    // FIXME: use pid instead of tid since we don't have threads :)
    case SYS_set_tid_address:
        trace("set_tid_address: name '%s'", thisproc()->name);
        return thisproc()->pid;
    case SYS_gettid:
        trace("gettid: name '%s'", thisproc()->name);
        return thisproc()->pid;

    // FIXME: Hack TIOCGWINSZ(get window size)
    case SYS_ioctl:
        trace("ioctl: name '%s'", thisproc()->name);
        if (tf->x[1] == 0x5413) return 0;
        else panic("ioctl unimplemented. ");

    // FIXME: always return 0 since we don't have signals  :)
    case SYS_rt_sigprocmask:
        trace("rt_sigprocmask: name '%s' how 0x%x", thisproc()->name, (int)tf->x[0]);
        return 0;

    case SYS_brk:
        return sys_brk();
    case SYS_mmap:
        return sys_mmap();

    case SYS_execve:
        return sys_execve();

    case SYS_sched_yield:
        return sys_yield();

    case SYS_clone:
        return sys_clone();

    case SYS_wait4:
        return sys_wait4();

    // FIXME: exit_group should kill every thread in the current thread group.
    case SYS_exit_group:
    case SYS_exit:
        trace("sys_exit: '%s' exit with code %d", thisproc()->name, tf->x[0]);
        exit(tf->x[0]);

    case SYS_dup:
        return sys_dup();

    case SYS_pipe2:
        return sys_pipe2();

    case SYS_chdir:
        return sys_chdir();

    case SYS_fstat:
        return sys_fstat();

    case SYS_newfstatat:
        return sys_fstatat();

    case SYS_mkdirat:
        return sys_mkdirat();

    case SYS_mknodat:
        return sys_mknodat();
        
    case SYS_openat:
        return sys_openat();

    case SYS_writev:
        return sys_writev();

    case SYS_read:
        return sys_read();

    case SYS_close:
        return sys_close();

    default:
        // FIXME: don't panic.

        debug_reg();
        panic("Unexpected syscall #%d\n", sysno);
        
        return 0;
    }
}

