
#include "trap.h"
#include "../libc/include/elf.h"

#include "file.h"
#include "log.h"
#include "string.h"

#include "console.h"
#include "vm.h"
#include "proc.h"
#include "mm.h"
#include "memlayout.h"

int
execve(const char *path, char *const argv[], char *const envp[])
{
    char *s;
    fetchstr(path, &s);

    cprintf("- execve: path='%s', argv=0x%p, envp=0x%p\n", s, argv, envp);
    asserts(envp == 0, "envp unimplemented. ");
    // asserts(argv == 0, "argv unimplemented. ");

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
    cprintf("exec: check elf header finish\n");

    char *pgdir = vm_init();
    if (pgdir == 0) {
        cprintf("exec: vm init failed\n");
        goto bad;
    }

    // Allocate user stack.
    void *stk_p = kalloc();
    if (stk_p == 0) {
        cprintf("exec: allocate user stack failed\n");
        goto bad;
    }
    uvm_map(pgdir, USERTOP - PGSIZE, PGSIZE, V2P(stk_p));

    int i;
    uint64_t off;
    Elf64_Phdr ph;

    // Save previous page table.
    struct proc *curproc = thisproc();
    void *oldpgdir = curproc->pgdir;
    curproc->pgdir = pgdir; // Required since readi(sdrw) involves context switch(switch page table).

    // Save program name for debugging.
    char *last;
    for (last = path, *s = path; *s; s++)
        if(*s == '/') last = s + 1;
    safestrcpy(curproc->name, last, sizeof(curproc->name));

    cprintf("exec: allocate user stack finished\n");

    // Load program into memory.
    size_t sz = 0, base = 0, stksz = PGSIZE;
    int first = 1;
    for (i = 0, off = elf.e_phoff; i < elf.e_phnum; i++, off += sizeof(ph)) {
        if (readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
            goto bad;

        if (ph.p_type != PT_LOAD) {
            cprintf("unsupported type 0x%x, skipped\n", ph.p_type);
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
            first = 0; base = ph.p_vaddr;
            if (base % PGSIZE != 0) {
                cprintf("first section should be page aligned!\n");
                goto bad;
            }
        }

        cprintf("phdr: vaddr 0x%p, memsz 0x%p, filesz 0x%p\n", ph.p_vaddr, ph.p_memsz, ph.p_filesz);

        if ((sz = uvm_alloc(pgdir, base, stksz, sz, ph.p_vaddr + ph.p_memsz - base)) == 0)
            goto bad;

        disb();
        uvm_switch(pgdir);
        disb();

        // Check accessibility.
        for (char *p = ph.p_vaddr; p < ph.p_vaddr + ph.p_memsz; p++) {
            char x = *p;
            cprintf("%d\r", x);
        }
        memset(ph.p_vaddr, 0, ph.p_memsz);
        cprintf("checked [0x%p, 0x%p)\n", ph.p_vaddr, ph.p_vaddr + ph.p_memsz);

        if (readi(ip, ph.p_vaddr, ph.p_offset, ph.p_filesz) != ph.p_filesz) {
            goto bad;
        }
        // Initialize BSS.
        memset(ph.p_vaddr + ph.p_filesz, 0, ph.p_memsz - ph.p_filesz);
        cprintf("exec: memset finished\n");

    }
    cprintf("exec: load binary finished\n");

    iunlockput(ip);
    end_op();
    ip = 0;

    char *sp = USERTOP;

  // TODO:
  // Push argument strings, prepare rest of stack in ustack.
  // for(argc = 0; argv[argc]; argc++) {
  //   if(argc >= MAXARG)
  //     goto bad;
  //   sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
  //   if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
  //     goto bad;
  //   ustack[3+argc] = sp;
  // }
  // ustack[3+argc] = 0;

  // ustack[0] = 0xffffffff;  // fake return PC
  // ustack[1] = argc;
  // ustack[2] = sp - (argc+1)*4;  // argv pointer

  // sp -= (3+argc+1) * 4;
  // if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
  //   goto bad;
    
    sp -= 8; *(size_t *)sp = AT_NULL;

    sp -= 8; *(size_t *)sp = PGSIZE;
    sp -= 8; *(size_t *)sp = AT_PAGESZ;

    sp -= 8; *(size_t *)sp = 0;

    // envp here.

    sp -= 8; *(size_t *)sp = 0;

    // argv here.
    
    sp -= 8; *(size_t *)sp = 0; // argc


    // Commit to the user image.
    curproc->pgdir = pgdir;

    curproc->base = base;
    curproc->sz = sz;
    curproc->stksz = stksz;

    curproc->tf->elr = elf.e_entry;
    curproc->tf->sp = sp;
    // cprintf("exec: curproc->tf 0x%p\n", tf);
    cprintf("exec: entry 0x%p\n", elf.e_entry);

    vm_free(oldpgdir);
    vm_stat(pgdir);

    uvm_switch(curproc->pgdir);
    cprintf("exec: finish\n");
    return 0;

bad:
    if (pgdir) vm_free(pgdir);
    if (ip) iunlockput(ip), end_op();
    panic("exec: bad.\n"); // FIXME: donot panic
    return -1;
}

