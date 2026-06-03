/*
 * 进入linux的入口
 */

#ifndef __ASSEMBLY__
#define __ASSEMBLY__
#endif

#define LINUX_IMAGE_ENTRY       0x48200000
#define LINUX_DTB_ADDRESS       0x4f000000

#define HCR_EL2_RW              0x80000000


#define SPSR_EL2_M_EL1H     0x5
#define SPSR_EL2_D          (1 << 9)
#define SPSR_EL2_A          (1 << 8)
#define SPSR_EL2_I          (1 << 7)
#define SPSR_EL2_F          (1 << 6)

#define SPSR_EL2_EL1H_DAIF  (SPSR_EL2_M_EL1H | SPSR_EL2_D | SPSR_EL2_A | SPSR_EL2_I | SPSR_EL2_F)

    .section ".text.linux_entry", "ax"
    .align 6
    .global linux_entry
linux_entry:
    mrs     x5, CurrentEL
    lsr     x5, x5, #2
    cmp     x5, #2
    b.ne    .linux_enter_el1

    ldr     x5, =linux_primary_hyp_stack_top
    mov     sp, x5

    ldr     x5, =hyp_vectors
    msr     vbar_el2, x5
    isb

    ldr     x5, =HCR_EL2_RW
    msr     hcr_el2, x5
    isb

    /* 允许 EL1 访问通用 counter，避免早期 timer/counter trap */
    mov     x5, xzr
    msr     cptr_el2, x5
    msr     cntvoff_el2, x5

    mrs     x5, cnthctl_el2
    orr     x5, x5, #3
    msr     cnthctl_el2, x5
    isb

    /* Linux arm64 boot protocol: x0=dtb, x1-x3=0 */
    ldr     x0, =LINUX_DTB_ADDRESS
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr

    /* eret 目标地址：Linux Image */
    ldr     x5, =LINUX_IMAGE_ENTRY
    msr     elr_el2, x5

    /*
    * SPSR_EL2:
    *   M[3:0] = 0b0101 -> EL1h
    *   DAIF masked     -> 0x3c0
    *   合计             -> 0x3c5
    */
    mov     x5, #SPSR_EL2_EL1H_DAIF
    msr     spsr_el2, x5
    isb

    eret

.linux_enter_el1:
    ldr     x0, =LINUX_DTB_ADDRESS
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    ldr     x4, =LINUX_IMAGE_ENTRY
    br      x4

    .global linux_entry_end
linux_entry_end:


    .section ".bss.noclean.linux_hyp", "aw"

    .align 12
    .space 4096
linux_primary_hyp_stack_top:
