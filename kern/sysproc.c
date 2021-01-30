#include "proc.h"
#include "trap.h"
#include "console.h"

int
sys_yield()
{
    yield();
    return 0;
}

size_t
sys_brk()
{
    struct proc *p = thisproc();
    size_t sz, newsz, oldsz = p->sz;

    if (argu64(0, &newsz) < 0)
        return oldsz;

    if (newsz < oldsz) {
        p->sz = uvm_dealloc(p->pgdir, p->base, oldsz, newsz);
    } else {
        sz = uvm_alloc(p->pgdir, p->base, p->stksz, oldsz, newsz);
        if (sz == 0)
            return oldsz;
        p->sz = sz;
    }
    return p->sz;
}

int
sys_clone()
{
    void *childstk;
    uint64_t flag;
    if (argu64(0, &flag) < 0 || argu64(1, &childstk) < 0)
        return -1;
    if (flag != 17) {
        cprintf("sys_clone: flags other than SIGCHLD are not supported.\n");
        return -1;
    }
    // cprintf("sys_clone: flags 0x%llx, child stack 0x%p\n", flag, childstk);
    return fork();
}


int
sys_wait4()
{
    int pid, opt;
    int *wstatus;
    void *rusage;
    if (argint(0, &pid) < 0 ||
        argu64(1, &wstatus) < 0 ||
        argint(2, &opt) < 0 ||
        argu64(3, &rusage) < 0)
        return -1;

    // FIXME:
    if (pid != -1 || wstatus != 0 || opt != 0 || rusage != 0) {
        cprintf("sys_wait4: unimplemented. pid %d, wstatus 0x%p, opt 0x%x, rusage 0x%p\n", pid, wstatus, opt, rusage);
        return -1;
    }

    return wait();
}
