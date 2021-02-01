#include <elf.h>

#include "trap.h"

#include "file.h"
#include "log.h"
#include "string.h"

#include "console.h"
#include "vm.h"
#include "proc.h"
#include "mm.h"
#include "memlayout.h"

static uint64_t auxv[][2] = {{AT_PAGESZ, PGSIZE}};

int
execve(const char *path, char *const argv[], char *const envp[])
{
    // Save previous page table.
    struct proc *curproc = thisproc();
    void *oldpgdir = curproc->pgdir;

    char *s;
    if (fetchstr(path, &s) < 0) return -1;

    // cprintf("- execve: path='%s', argv=0x%p, envp=0x%p\n", s, argv, envp);

    begin_op();
    struct inode *ip = namei(path);
    if (ip == 0) {
        end_op();
        cprintf("exec: failed\n");
        return -1;
    }
    ilock(ip);

    Elf64_Ehdr elf;
    if (readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))
        goto bad;
    if (!(elf.e_ident[EI_MAG0] == ELFMAG0 && elf.e_ident[EI_MAG1] == ELFMAG1 &&
        elf.e_ident[EI_MAG2] == ELFMAG2 && elf.e_ident[EI_MAG3] == ELFMAG3)) {
        cprintf("- elf header magic invalid\n");
        goto bad;
    }
    if (elf.e_ident[EI_CLASS] != ELFCLASS64) {
        cprintf("- 64 bit program not supported\n");
        goto bad;
    }
    // cprintf("exec: check elf header finish\n");

    char *pgdir = vm_init();
    if (pgdir == 0) {
        cprintf("exec: vm init failed\n");
        goto bad;
    }

    int i;
    uint64_t off;
    Elf64_Phdr ph;

    curproc->pgdir = pgdir; // Required since readi(sdrw) involves context switch(switch page table).

    // Load program into memory.
    size_t sz = 0, base = 0, stksz = 0;
    int first = 1;
    for (i = 0, off = elf.e_phoff; i < elf.e_phnum; i++, off += sizeof(ph)) {
        if (readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
            goto bad;

        if (ph.p_type != PT_LOAD) {
            // cprintf("unsupported type 0x%x, skipped\n", ph.p_type);
            continue;
        }

        if (ph.p_memsz < ph.p_filesz) {
            cprintf("memsz smaller than filesz!\n");
            goto bad;
        }
        if (ph.p_vaddr + ph.p_memsz < ph.p_vaddr) {
            cprintf("vaddr + memsz overflow!\n");
            goto bad;
        }

        if (first) {
            first = 0;
            sz = base = ph.p_vaddr;
            if (base % PGSIZE != 0) {
                cprintf("first section should be page aligned!\n");
                goto bad;
            }
        }

        // cprintf("phdr: vaddr 0x%p, memsz 0x%p, filesz 0x%p\n", ph.p_vaddr, ph.p_memsz, ph.p_filesz);

        if ((sz = uvm_alloc(pgdir, base, stksz, sz, ph.p_vaddr + ph.p_memsz)) == 0)
            goto bad;

        disb();
        uvm_switch(pgdir);
        disb();

        // Check accessibility.
        // for (char *p = ph.p_vaddr; p < ph.p_vaddr + ph.p_memsz; p++) {
        //     char x = *p;
        //     cprintf("%d\r", x);
        // }
        // memset(ph.p_vaddr, 0, ph.p_memsz);
        // cprintf("checked [0x%p, 0x%p)\n", ph.p_vaddr, ph.p_vaddr + ph.p_memsz);

        if (readi(ip, ph.p_vaddr, ph.p_offset, ph.p_filesz) != ph.p_filesz) {
            goto bad;
        }
        // Initialize BSS.
        memset(ph.p_vaddr + ph.p_filesz, 0, ph.p_memsz - ph.p_filesz);
    }

    iunlockput(ip);
    end_op();
    ip = 0;

    // Push argument strings, prepare rest of stack in ustack.
    uvm_switch(oldpgdir);
    char *sp = USERTOP;
    int argc = 0, envc = 0;
    size_t len;
    if (argv) {
        for (; in_user(argv+argc, sizeof(*argv)) && argv[argc]; argc++) {
            if ((len = fetchstr(argv[argc], &s)) < 0)
                goto bad;
            // cprintf("argv[%d] = '%s', len: %d\n", argc, argv[argc], len);
            sp -= len + 1;
            if (copyout(pgdir, sp, argv[argc], len+1) < 0) // include '\0';
                goto bad;
        }
    }
    if (envp) {
        for (; in_user(envp+envc, sizeof(*envp)) && envp[envc]; envc++) {
            if ((len = fetchstr(envp[envc], &s)) < 0)
                goto bad;
            // cprintf("envp[%d] = '%s', len: %d\n", envc, envp[envc], len);
            sp -= len + 1;
            if (copyout(pgdir, sp, envp[envc], len+1) < 0) // include '\0';
                goto bad;
        }
    }


    // Align to 16B. 3 zero terminator of auxv/envp/argv and 1 argc.
    void *newsp = ROUNDDOWN((size_t)sp - sizeof(auxv) - (envc+argc+4)*8, 16);
    if (copyout(pgdir, newsp, 0, (size_t)sp - (size_t)newsp) < 0)
        goto bad;

    uvm_switch(pgdir);

    uint64_t *newargv = newsp + 8;
    uint64_t *newenvp = (void *)newargv + 8*(argc+1);
    uint64_t *newauxv = (void *)newenvp + 8*(envc+1);
    memmove(newauxv, auxv, sizeof(auxv));
    // cprintf("auxv size %d, newauxv: %p\n", sizeof(auxv), newauxv);

    for (int i = envc-1; i >= 0; i--) {
        newenvp[i] = sp;
        for (; *sp; sp++) ;
        sp++;
    }
    for (int i = argc-1; i >= 0; i--) {
        newargv[i] = sp;
        for (; *sp; sp++) ;
    }
    *(size_t *)(newsp) = argc;

    sp = newsp;

    // Allocate user stack.
    stksz = ROUNDUP(USERTOP - (size_t)sp, 2*PGSIZE);
    if (copyout(pgdir, USERTOP - stksz, 0, stksz - (USERTOP - (size_t)sp)) < 0)
        goto bad;

    assert(sp > USERTOP - stksz);

    // Commit to the user image.
    curproc->pgdir = pgdir;

    curproc->base = base;
    curproc->sz = sz;
    curproc->stksz = stksz;

    curproc->tf->elr = elf.e_entry;
    curproc->tf->sp = sp;

    // cprintf("exec: curproc->tf 0x%p\n", tf);
    // cprintf("exec: entry 0x%p\n", elf.e_entry);
    
    uvm_switch(oldpgdir);

    // Save program name for debugging.
    char *last;
    for (last = path, *s = path; *s; s++)
        if (*s == '/') last = s + 1;
    safestrcpy(curproc->name, last, sizeof(curproc->name));

    uvm_switch(curproc->pgdir);
    vm_free(oldpgdir);
    return 0;

bad:
    if (pgdir) vm_free(pgdir);
    if (ip) iunlockput(ip), end_op();
    thisproc()->pgdir = oldpgdir;
    cprintf("exec: bad\n");
    return -1;
}

