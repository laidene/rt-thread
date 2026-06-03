
#define SPSR_EL2_M_EL1H     0x5
#define SPSR_EL2_D          (1 << 9)
#define SPSR_EL2_A          (1 << 8)
#define SPSR_EL2_I          (1 << 7)
#define SPSR_EL2_F          (1 << 6)

#define SPSR_EL2_EL1H_DAIF  (SPSR_EL2_M_EL1H | SPSR_EL2_D | SPSR_EL2_A | SPSR_EL2_I | SPSR_EL2_F)
#define HCR_EL2_RW              0x80000000


.global linux_secondary_entry
linux_secondary_entry:
    ldr     x5, =linux_secondary_hyp_stack_top
    mov     sp, x5

    ldr     x5, =hyp_vectors
    msr     vbar_el2, x5
    isb

    ldr     x5, =HCR_EL2_RW
    msr     hcr_el2, x5
    isb

    /* 允许el1访问counter */
    mov     x5,          xzr
    msr     cptr_el2,    x5
    msr     cntvoff_el2, x5

    mrs     x5, cnthctl_el2
    orr     x5, x5, #3
    msr     cnthctl_el2, x5
    isb

    /* 恢复 Linux PSCI CPU_ON 传入的 context */
    ldr     x5, =linux_secondary_raw_context
    ldr     x0, [x5]
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr

    /* eret 到 Linux 原始 secondary entry */
    ldr     x5, =linux_secondary_raw_entry
    ldr     x5, [x5]
    msr     elr_el2, x5

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