/*
 * Minimal EL2 vector for QEMU virt64 hypervisor bring-up.
 */

#ifndef __ASSEMBLY__
#define __ASSEMBLY__
#endif

#define ESR_EL2_EC_SHIFT               26
#define ESR_EL2_EC_HVC64               0x16

#define HYPERCALL_VERSION_ID           0xc5000000
#define HYPERCALL_DEBUG_ID             0xc5000001
#define HYPERCALL_CPU_ON_ID            0xc5000003
#define HYP_VERSION_0_1                0x00010000
#define HYP_CUSTOM_TEST_VALUE          0x48564321
#define PSCI_0_2_FN64_CPU_ON           0xc4000003

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
    b hyp_sync_lower_aarch64
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
    mrs     x16, esr_el2
    lsr     x17, x16, #ESR_EL2_EC_SHIFT
    cmp     x17, #ESR_EL2_EC_HVC64
    b.ne    hyp_panic

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

hyp_hvc_not_supported:
    mov     x0, #-1
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    eret

hyp_panic:
    wfe
    b       hyp_panic
