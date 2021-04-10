#include "trap.h"

#include "arm.h"
#include "sysregs.h"
#include "mmu.h"
#include "irq.h"

#include "memlayout.h"
#include "console.h"
#include "proc.h"

#include "debug.h"

void
trap_init()
{
    extern char vectors[];
    lvbar(vectors);
    lesr(0);
}

void
trap(struct trapframe *tf)
{
    int ec = resr() >> EC_SHIFT, iss = resr() & ISS_MASK, il =
        resr() & IR_MASK;
    /* Clear esr. */
    lesr(0);
    switch (ec) {
    case EC_UNKNOWN:
        if (il) {
            panic("unknown error");
        } else
            irq_handler();
        break;

    case EC_SVC64:
        if (iss == 0) {
            tf->x[0] = syscall1(tf);
        } else {
            warn("unexpected svc iss 0x%x", iss);
        }
        break;

    default:
        exit(1);
    }
}

void
trap_error(uint64_t type)
{
    debug_reg();
    panic("irq of type %d unimplemented. \n", type);
}
