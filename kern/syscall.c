#include <syscall.h>
// #include <unistd.h>

#include <stdint.h>
#include "memlayout.h"
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

/* Check if a block of memory lies within the process user space. */
int
in_user(void *s, size_t n)
{
    struct proc *p = thisproc();
    if ((p->base <= (uint64_t) s && (uint64_t) s + n <= p->sz) ||
        (USERTOP - p->stksz <= (uint64_t) s
         && (uint64_t) s + n <= USERTOP))
        return 1;
    return 0;
}

/*
 * Fetch the nul-terminated string at addr from the current process.
 * Doesn't actually copy the string - just sets *pp to point at it.
 * Returns length of string, not including nul.
 */
int
fetchstr(uint64_t addr, char **pp)
{
    struct proc *p = thisproc();
    char *s;
    *pp = s = (char *)addr;
    if (p->base <= addr && addr < p->sz) {
        for (; (uint64_t) s < p->sz; s++)
            if (*s == 0)
                return s - *pp;
    } else if (USERTOP - p->stksz <= addr && addr < USERTOP) {
        for (; (uint64_t) s < USERTOP; s++)
            if (*s == 0)
                return s - *pp;
    }
    return -1;
}

/*
 * Fetch the nth (starting from 0) 32-bit system call argument.
 * In our ABI, x8 contains system call index, x0-x5 contain parameters.
 * now we support system calls with at most 6 parameters.
 */
int
argint(int n, int *ip)
{
    struct proc *proc = thisproc();
    if (n > 5) {
        warn("too many system call parameters");
        return -1;
    }
    *ip = proc->tf->x[n];

    return 0;
}

/*
 * Fetch the nth (starting from 0) 64-bit system call argument.
 * In our ABI, x8 contains system call index, x0-x5 contain parameters.
 * now we support system calls with at most 6 parameters.
 */
int
argu64(int n, uint64_t * ip)
{
    struct proc *proc = thisproc();
    if (n > 5) {
        warn("too many system call parameters");
        return -1;
    }
    *ip = proc->tf->x[n];

    return 0;
}

/*
 * Fetch the nth word-sized system call argument as a pointer
 * to a block of memory of size bytes. Check that the pointer
 * lies within the process address space.
 */
int
argptr(int n, char **pp, size_t size)
{
    uint64_t i = 0;
    if (argu64(n, &i) < 0) {
        return -1;
    }
    if (in_user((void *)i, size)) {
        *pp = (char *)i;
        return 0;
    } else {
        return -1;
    }
}

/*
 * Fetch the nth word-sized system call argument as a string pointer.
 * Check that the pointer is valid and the string is nul-terminated.
 * (There is no shared writable memory, so the string can't change
 * between this check and being used by the kernel.)
 */
int
argstr(int n, char **pp)
{
    uint64_t addr = 0;
    if (argu64(n, &addr) < 0)
        return -1;
    int r = fetchstr(addr, pp);
    return r;
}

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
        if (tf->x[1] == 0x5413)
            return 0;
        else
            panic("ioctl unimplemented. ");

        // FIXME: always return 0 since we don't have signals  :)
    case SYS_rt_sigprocmask:
        trace("rt_sigprocmask: name '%s' how 0x%x", thisproc()->name,
              (int)tf->x[0]);
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
        trace("sys_exit: '%s' exit with code %d", thisproc()->name,
              tf->x[0]);
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
