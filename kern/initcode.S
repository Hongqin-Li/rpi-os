icode:
    adr     x0, init
    // mov     x0, 
    svc     #0x30

init:
    .string "/init"
.align 4
eicode:
