#include "proc.h"
#include "trap.h"
#include "console.h"
#include "vm.h"

#include <sys/mman.h>

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

    panic("sys_brk: unimplemented. ");

    if (argu64(0, &newsz) < 0)
        return oldsz;

    trace("name %s: 0x%llx to 0x%llx", p->name, oldsz, newsz);

    if (newsz == 0)
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

size_t
sys_mmap()
{
    void *addr;
    size_t len, off;
    int prot, flags, fd;
    if (argu64(0, (uint64_t *) & addr) < 0 ||
        argu64(1, &len) < 0 ||
        argint(2, &prot) < 0 ||
        argint(3, &flags) < 0 || argint(4, &fd) < 0 || argu64(5, &off) < 0)
        return -1;

    if ((flags & MAP_PRIVATE) == 0 || (flags & MAP_ANON) == 0 || fd != -1
        || off != 0) {
        warn("non-private mmap unimplemented: flags 0x%x, fd %d, off %d",
             flags, fd, off);
        return -1;
    }

    if (addr) {
        if (prot != PROT_NONE) {
            warn("mmap unimplemented");
            return -1;
        }
        trace("map none at 0x%p", addr);
        return (size_t)addr;
    } else {
        if (prot != (PROT_READ | PROT_WRITE)) {
            warn("non-rw unimplemented");
            return -1;
        }
        panic("unimplemented. ");
        return -1;
    }
}

int
sys_clone()
{
    void *childstk;
    uint64_t flag;
    if (argu64(0, &flag) < 0 || argu64(1, (uint64_t *) & childstk) < 0)
        return -1;
    trace("flags 0x%llx, child stack 0x%p", flag, childstk);
    if (flag != 17) {
        warn("flags other than SIGCHLD are not supported");
        return -1;
    }
    return fork();
}


int
sys_wait4()
{
    int pid, opt;
    int *wstatus;
    void *rusage;
    if (argint(0, &pid) < 0 ||
        argu64(1, (uint64_t *) & wstatus) < 0 ||
        argint(2, &opt) < 0 || argu64(3, (uint64_t *) & rusage) < 0)
        return -1;

    // FIXME:
    if (pid != -1 || wstatus != 0 || opt != 0 || rusage != 0) {
        warn("unimplemented. pid %d, wstatus 0x%p, opt 0x%x, rusage 0x%p",
             pid, wstatus, opt, rusage);
        return -1;
    }

    return wait();
}
