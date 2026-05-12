/*
 * Trampoline for booting a Linux Image on a reserved CPU.
 *
 * Keep this file small: C code owns the stage-2 tables and GIC setup,
 * while assembly only handles the EL2 entry/exit sequence.
 * 设置 EL2 栈、vbar_el2、调用 C 初始化、最后 eret 进 Linux
 */

#ifndef __ASSEMBLY__
#define __ASSEMBLY__
#endif

#define LINUX_IMAGE_ENTRY       0x48200000
#define LINUX_DTB_ADDRESS       0x4f000000
#define SPSR_EL1H_DAIF          0x3c5

    .section ".text.linux_trampoline", "ax"
    .align 6
    .global linux_cpu3_trampoline
linux_cpu3_trampoline:
    /*
     * PSCI enters the target CPU at EL2 on QEMU virt with virtualization=on.
     * Keep EL2 owned by our monitor, then enter Linux as an EL1 kernel.
     */
    mrs     x5, CurrentEL
    lsr     x5, x5, #2
    cmp     x5, #2
    b.ne    .linux_enter_current_el /* 跳到 Linux 镜像入口 */

    ldr     x5, =linux_cpu3_hyp_stack_top
    mov     sp, x5

    ldr     x5, =hyp_vectors
    msr     vbar_el2, x5            /* EL2 异常有入口 */
    isb

    bl      linux_el2_prepare

    mov     x5, #SPSR_EL1H_DAIF
    msr     spsr_el2, x5

    ldr     x0, =LINUX_DTB_ADDRESS
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    ldr     x5, =LINUX_IMAGE_ENTRY
    msr     elr_el2, x5
    eret

.linux_enter_current_el:
    ldr     x0, =LINUX_DTB_ADDRESS
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    ldr     x4, =LINUX_IMAGE_ENTRY
    br      x4

    .ltorg

    .global linux_cpu3_trampoline_end
linux_cpu3_trampoline_end:

    .section ".bss.noclean.linux_hyp", "aw"
    .align 12
linux_cpu3_hyp_stack:
    .space 4096
linux_cpu3_hyp_stack_top:
