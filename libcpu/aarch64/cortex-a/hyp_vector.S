#ifndef __ASSEMBLY__
    #define __ASSEMBLY__
#endif

/* 异常原因类型相关 */
#define ESR_EL2_EC_SHIFT               26
#define ESR_EL2_EC_HVC64               0x16


#define HYPERCALL_START                 0xc5000000
#define HYPERCALL_DEBUG_ID              (HYPERCALL_START + 3)
#define HYPERCALL_CPU_ON_ID             (HYPERCALL_START + 4)


#define PSCI_VERSION_VALUE          0x00010001
#define PSCI_VERSION                0x84000000
#define PSCI_CPU_ON_64              0xc4000003
#define PSCI_RET_DENIED             -3

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

    movz    w16, #(HYPERCALL_CPU_ON_ID >> 16), lsl #16
    add     w16, w16, #(HYPERCALL_CPU_ON_ID & 0xffff)
    cmp     w0, w16
    b.eq    hyp_hvc_cpu_on

    movz    w16, #(PSCI_VERSION >> 16), lsl #16
    add     w16, w16, #(PSCI_VERSION & 0xffff)
    cmp     w0, w16
    b.eq    hyp_hvc_version

    movz    w16, #(PSCI_CPU_ON_64 >> 16), lsl #16
    add     w16, w16, #(PSCI_CPU_ON_64 & 0xffff)
    cmp     w0, w16
    b.eq    hyp_hvc_linux_cpu_on

    /* never reach if no err */
    b       hyp_hvc_not_supported


/*
 * x1 = target MPIDR
 * x2 = entry point
 * x3 = context_id
 */
hyp_hvc_cpu_on:
    movz    w0, #(PSCI_CPU_ON_64 >> 16), lsl #16
    add     w0, w0, #(PSCI_CPU_ON_64 & 0xffff)
    smc     #0
    eret_zero_x1_x3

/*
   * Linux PSCI CPU_ON:
   * x0 = 0xc4000003
   * x1 = target MPIDR
   * x2 = Linux secondary entry
   * x3 = context
   */
  hyp_hvc_linux_cpu_on:
      /* Linux 只允许唤醒物理 CPU3 */
      cmp     x1, #3
      b.eq    1f

      mov     x0, #PSCI_RET_DENIED
      eret_zero_x1_x3

  1:
      /* 保存 Linux 给的原始 secondary entry/context */
      ldr     x16, =linux_secondary_raw_entry
      str     x2, [x16]

      ldr     x16, =linux_secondary_raw_context
      str     x3, [x16]

      /*
       * 调真实 PSCI CPU_ON，但 entry 换成自己的 EL2 linux_secondary_entry。
       * x1 仍然是 target MPIDR = 3
       */
      movz    w0, #(PSCI_CPU_ON_64 >> 16), lsl #16
      add     w0, w0, #(PSCI_CPU_ON_64 & 0xffff)

      ldr     x2, =linux_secondary_entry
      mov     x3, xzr
      smc     #0

      eret_zero_x1_x3

hyp_hvc_version:
    movz    w0, #(PSCI_VERSION_VALUE >> 16), lsl #16
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    eret

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