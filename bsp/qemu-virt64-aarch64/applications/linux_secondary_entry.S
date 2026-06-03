#ifndef __ASSEMBLY__
#define __ASSEMBLY__
#endif

#define SPSR_EL2_M_EL1H         0x5
#define SPSR_EL2_D              (1 << 9)
#define SPSR_EL2_A              (1 << 8)
#define SPSR_EL2_I              (1 << 7)
#define SPSR_EL2_F              (1 << 6)
#define SPSR_EL2_EL1H_DAIF      (SPSR_EL2_M_EL1H | SPSR_EL2_D | SPSR_EL2_A | SPSR_EL2_I | SPSR_EL2_F)

    .section ".text.linux_secondary_entry", "ax"
    .align 6
    .global linux_secondary_entry
linux_secondary_entry:
    ldr     x5, =linux_secondary_hyp_stack_top
    mov     sp, x5

    ldr     x5, =hyp_vectors
    msr     vbar_el2, x5
    isb

    ldr     x5, =linux_secondary_raw_entry
    ldr     x0, [x5]
    bl      linux_hyp_enter_el1

    ldr     x5, =linux_secondary_raw_context
    ldr     x0, [x5]
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr

    mov     x5, #SPSR_EL2_EL1H_DAIF
    msr     spsr_el2, x5
    isb
    eret

    .section ".bss.noclean.linux_hyp", "aw"
    .global linux_secondary_raw_entry
    .global linux_secondary_raw_context

    .align 3
linux_secondary_raw_entry:
    .quad 0
linux_secondary_raw_context:
    .quad 0

    .align 12
    .space 4096
linux_secondary_hyp_stack_top:
