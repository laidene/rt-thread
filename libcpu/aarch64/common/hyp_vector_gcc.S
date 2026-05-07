/*
 * Minimal EL2 vector for QEMU virt64 hypervisor bring-up.
 */

#ifndef __ASSEMBLY__
#define __ASSEMBLY__
#endif

#define ESR_EL2_EC_SHIFT               26
#define ESR_EL2_EC_HVC64               0x16
#define ESR_EL2_EC_DABT_LOW            0x24
#define ESR_EL2_DABT_WNR               (1 << 6)

#define HYPERCALL_VERSION_ID           0xc5000000
#define HYPERCALL_DEBUG_ID             0xc5000001
#define HYPERCALL_CPU_ON_ID            0xc5000003
#define HYP_VERSION_0_1                0x00010000
#define HYP_CUSTOM_TEST_VALUE          0x48564321
#define PSCI_0_2_FN64_CPU_ON           0xc4000003
#define PSCI_32_PREFIX                 0x84
#define PSCI_64_PREFIX                 0xc4
#define GICD_BASE                      0x08000000
#define GICD_END                       0x08010000

.text
.align 11
.global hyp_vectors
hyp_vectors:
    .set HYP_VBAR, hyp_vectors
    .org HYP_VBAR

    /* Exception from CurrentEL with SP_EL0 */
    .org (HYP_VBAR + 0x000)
    b hyp_panic
    .org (HYP_VBAR + 0x080)
    b hyp_panic
    .org (HYP_VBAR + 0x100)
    b hyp_panic
    .org (HYP_VBAR + 0x180)
    b hyp_panic

    /* Exception from CurrentEL with SP_ELx */
    .org (HYP_VBAR + 0x200)
    b hyp_panic
    .org (HYP_VBAR + 0x280)
    b hyp_panic
    .org (HYP_VBAR + 0x300)
    b hyp_panic
    .org (HYP_VBAR + 0x380)
    b hyp_panic

    /* Exception from lower EL, AArch64 */
    .org (HYP_VBAR + 0x400)
    b hyp_sync_lower_aarch64    /* EL1/EL0 触发同步异常后，EL2 的统一分发入口 */
    /* ESR_EL2.EC == HVC64 主要是 Linux 发 PSCI 的 hvc，或者你自定义的 hypercall */
    /* ESR_EL2.EC == DABT_LOW EL1 对某个地址读/写时发生数据访问异常 */

    .org (HYP_VBAR + 0x480)
    b hyp_panic
    .org (HYP_VBAR + 0x500)
    b hyp_panic
    .org (HYP_VBAR + 0x580)
    b hyp_panic

    /* Exception from lower EL, AArch32 */
    .org (HYP_VBAR + 0x600)
    b hyp_panic
    .org (HYP_VBAR + 0x680)
    b hyp_panic
    .org (HYP_VBAR + 0x700)
    b hyp_panic
    .org (HYP_VBAR + 0x780)
    b hyp_panic









hyp_sync_lower_aarch64:
    sub     sp, sp, #16
    stp     x16, x17, [sp]

    mrs     x16, esr_el2
    lsr     x17, x16, #ESR_EL2_EC_SHIFT
    cmp     x17, #ESR_EL2_EC_HVC64
    b.eq    hyp_hvc_lower_aarch64
    cmp     x17, #ESR_EL2_EC_DABT_LOW
    b.eq    hyp_dabt_lower_aarch64
    ldp     x16, x17, [sp], #16
    b       hyp_panic




/* hypercall */
hyp_hvc_lower_aarch64:
    add     sp, sp, #16

    movz    w16, #(HYPERCALL_VERSION_ID >> 16), lsl #16
    cmp     w0, w16
    b.eq    hyp_hvc_version

    movz    w16, #(HYPERCALL_DEBUG_ID >> 16), lsl #16
    add     w16, w16, #(HYPERCALL_DEBUG_ID & 0xffff)
    cmp     w0, w16
    b.eq    hyp_hvc_custom_test

    movz    w16, #(HYPERCALL_CPU_ON_ID >> 16), lsl #16
    add     w16, w16, #(HYPERCALL_CPU_ON_ID & 0xffff)
    cmp     w0, w16
    b.eq    hyp_hvc_cpu_on

    /*
     * Linux is given PSCI method = "hvc".  EL2 owns the HVC trap and
     * proxies standard PSCI calls down to QEMU/EL3 with SMC.
     */
    lsr     w16, w0, #24
    cmp     w16, #PSCI_32_PREFIX
    b.eq    hyp_hvc_psci_proxy
    cmp     w16, #PSCI_64_PREFIX
    b.eq    hyp_hvc_psci_proxy

    b       hyp_hvc_not_supported

hyp_hvc_version:
    movz    w0, #(HYP_VERSION_0_1 >> 16), lsl #16
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    eret

hyp_hvc_custom_test:
    movz    w0, #(HYP_CUSTOM_TEST_VALUE >> 16), lsl #16
    movk    w0, #(HYP_CUSTOM_TEST_VALUE & 0xffff)
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    eret

.global hyp_hvc_cpu_on
hyp_hvc_cpu_on:
    /*
     * x1 = target MPIDR
     * x2 = entry point
     * x3 = context_id
     */
    movz    w0, #(PSCI_0_2_FN64_CPU_ON >> 16), lsl #16
    add     w0, w0, #(PSCI_0_2_FN64_CPU_ON & 0xffff)
    smc     #0

    /* x0 keeps the PSCI return value */
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    eret

.global hyp_hvc_psci_proxy
hyp_hvc_psci_proxy:
    smc     #0
    eret

hyp_hvc_not_supported:
    mov     x0, #-1
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    eret




/* 数据访问的问题 */
hyp_dabt_lower_aarch64:
    sub     sp, sp, #64
    stp     x16, x17, [sp, #0]
    stp     x18, x19, [sp, #16]
    stp     x20, x21, [sp, #32]
    stp     x22, x23, [sp, #48]

    /*
     * Stage-2 permission fault on the real GIC distributor.  For now this
     * drops guest GICD writes instead of letting Linux modify global GICD
     * state directly.  Reads should be handled by the read-only mapping.
     */
    tbz     x16, #6, hyp_dabt_restore_panic

    mrs     x18, hpfar_el2
    lsl     x18, x18, #8
    mrs     x19, far_el2
    and     x19, x19, #0xfff
    orr     x18, x18, x19

    ldr     x19, =GICD_BASE
    cmp     x18, x19
    b.lo    hyp_dabt_restore_panic
    ldr     x19, =GICD_END
    cmp     x18, x19
    b.hs    hyp_dabt_restore_panic

    ldr     x19, =hyp_gicd_write_trap_count
    ldr     x20, [x19]
    add     x20, x20, #1
    str     x20, [x19]

    ldr     x19, =hyp_last_gicd_write_ipa
    str     x18, [x19]
    ldr     x19, =hyp_last_gicd_write_esr
    str     x16, [x19]

    mrs     x20, elr_el2
    add     x20, x20, #4
    msr     elr_el2, x20

    ldp     x16, x17, [sp, #0]
    ldp     x18, x19, [sp, #16]
    ldp     x20, x21, [sp, #32]
    ldp     x22, x23, [sp, #48]
    add     sp, sp, #64
    ldp     x16, x17, [sp], #16
    eret

hyp_dabt_restore_panic:
    ldp     x16, x17, [sp, #0]
    ldp     x18, x19, [sp, #16]
    ldp     x20, x21, [sp, #32]
    ldp     x22, x23, [sp, #48]
    add     sp, sp, #64
    ldp     x16, x17, [sp], #16
    b       hyp_panic






hyp_panic:
    wfe
    b       hyp_panic

    .section ".bss.noclean.hyp", "aw"
    .align 3
    .global hyp_gicd_write_trap_count
hyp_gicd_write_trap_count:
    .quad   0
    .global hyp_last_gicd_write_ipa
hyp_last_gicd_write_ipa:
    .quad   0
    .global hyp_last_gicd_write_esr
hyp_last_gicd_write_esr:
    .quad   0
