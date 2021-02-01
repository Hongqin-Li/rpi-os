#include "syscall.h"

.global icode
.global ispin
.global eicode

icode:
    mov     x8, #SYS_execve
    adr     x0, init
    mov     x1, #0
    mov     x2, #0
    svc     #0

ispin:
    mov     x8, #SYS_sched_yield
    svc     #0
    b       ispin

init:
    .string "/init\0"

.align 4
eicode:
