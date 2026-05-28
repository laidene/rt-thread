#ifndef __ASSEMBLY__
    #define __ASSEMBLY__
#endif

/* 异常原因类型相关 */
#define ESR_EL2_EC_SHIFT               26
#define ESR_EL2_EC_HVC64               0x16


#define HYPERCALL_START                 0xc5000000
#define HYPERCALL_DEBUG_ID              (HYPERCALL_START + 3)


#define HYP_CUSTOM_TEST_VALUE           0xdeadcccc

.macro vector_slot offset, target
    .org (hyp_vbar + \offset)
    b \target
.endm

.text
.align 11
.global hyp_vectors
hyp_vectors:
    .set hyp_vbar, hyp_vectors
    .org hyp_vbar

    /* Current EL, using SP_EL0 */

    vector_slot 0x000, hyp_stop /* Synchronous */
    vector_slot 0x080, hyp_stop /* IRQ/vIRQ */
    vector_slot 0x100, hyp_stop /* FIQ/vFIQ */
    vector_slot 0x180, hyp_stop /* SError/vSError */

    /* Current EL, using SP_ELx */
    vector_slot 0x200, hyp_stop
    vector_slot 0x280, hyp_stop
    vector_slot 0x300, hyp_stop
    vector_slot 0x380, hyp_stop

    /* Lower EL, AArch64  */
    vector_slot 0x400, hyp_sync_lower_aarch64
    vector_slot 0x480, hyp_stop
    vector_slot 0x500, hyp_stop
    vector_slot 0x580, hyp_stop

    /* Lower EL, AArch32 */
    vector_slot 0x600, hyp_stop
    vector_slot 0x680, hyp_stop
    vector_slot 0x700, hyp_stop
    vector_slot 0x780, hyp_stop











.macro eret_zero_x1_x3
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    eret
.endm

/* ELR_EL2 指向触发异常的那条指令 */
/* ESR_EL2 能说明异常原因 */
/* FAR_EL2/HPFAR_EL2 可给出 fault 地址 */
hyp_sync_lower_aarch64:
    sub     sp, sp, #16
    stp     x16, x17, [sp]

    mrs     x16, esr_el2
    lsr     x17, x16, #ESR_EL2_EC_SHIFT

    cmp     x17, #ESR_EL2_EC_HVC64
    b.eq    hyp_hvc_lower_aarch64

    /* never reach if no err */
    ldp     x16, x17, [sp], #16
    b       hyp_stop

hyp_hvc_lower_aarch64:
    add     sp, sp, #16

    movz    w16, #(HYPERCALL_DEBUG_ID >> 16), lsl #16
    add     w16, w16, #(HYPERCALL_DEBUG_ID & 0xffff)
    cmp     w0, w16
    b.eq    hyp_hvc_custom_test
    b       hyp_hvc_not_supported


hyp_hvc_custom_test:
    movz    w0, #(HYP_CUSTOM_TEST_VALUE >> 16), lsl #16
    movk    w0, #(HYP_CUSTOM_TEST_VALUE & 0xffff)
    eret_zero_x1_x3

hyp_hvc_not_supported:
    mov     x0, #-1
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    eret

hyp_stop:
    wfe
    b       hyp_stop