#ifndef __ASSEMBLY__
    #define __ASSEMBLY__
#endif

/* 异常原因类型相关 */
#define ESR_EL2_EC_SHIFT               26
#define ESR_EL2_EC_HVC64               0x16
#define ESR_EL2_EC_DABT_LOW            0x24


#define HYPERCALL_START                 0xc5000000
#define HYPERCALL_DEBUG_ID              (HYPERCALL_START + 3)
#define HYPERCALL_CPU_ON_ID             (HYPERCALL_START + 4)


#define PSCI_VERSION_VALUE          0x00010001
#define PSCI_VERSION                0x84000000
#define PSCI_CPU_ON_64              0xc4000003
#define PSCI_RET_DENIED             -3

#define HYP_CUSTOM_TEST_VALUE           0xdeadcccc

#define LINUX_S2_TF_SIZE                288
#define LINUX_S2_TF_ESR                 248
#define LINUX_S2_TF_FAR                 256
#define LINUX_S2_TF_HPFAR               264
#define LINUX_S2_TF_ELR                 272

.macro vector_slot offset, target
    .org (hyp_vbar + \offset)
    b \target
.endm

    .extern hyp_log_exception
    .extern linux_stage2_abort
    .extern linux_secondary_entry
    .extern linux_vgic_log_physical_irq
    .extern linux_vgic_handle_physical_irq

.text
.align 11
.global hyp_vectors
hyp_vectors:
    .set hyp_vbar, hyp_vectors
    .org hyp_vbar

    /* Current EL, using SP_EL0 */

    vector_slot 0x000, hyp_current_sync /* Synchronous */
    vector_slot 0x080, hyp_current_irq /* IRQ/vIRQ */
    vector_slot 0x100, hyp_current_fiq /* FIQ/vFIQ */
    vector_slot 0x180, hyp_current_serror /* SError/vSError */

    /* Current EL, using SP_ELx */
    vector_slot 0x200, hyp_current_sync
    vector_slot 0x280, hyp_current_irq
    vector_slot 0x300, hyp_current_fiq
    vector_slot 0x380, hyp_current_serror

    /* Lower EL, AArch64  */
    vector_slot 0x400, hyp_sync_lower_aarch64
    vector_slot 0x480, hyp_lower_irq
    vector_slot 0x500, hyp_lower_fiq
    vector_slot 0x580, hyp_lower_serror

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

.macro log_hyp_exception tag
    /* 保存寄存器 */
    sub     sp, sp, #64
    stp     x0, x1, [sp, #0]
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    str     x30,    [sp, #48]

    ldr     x0, =\tag
    mrs     x1, esr_el2
    mrs     x2, far_el2
    mrs     x3, hpfar_el2
    mrs     x4, elr_el2
    mrs     x5, spsr_el2
    bl      hyp_log_exception

    /* 恢复寄存器 */
    ldp     x0, x1, [sp, #0]
    ldp     x2, x3, [sp, #16]
    ldp     x4, x5, [sp, #32]
    ldr     x30,    [sp, #48]
    add     sp, sp, #64
.endm

.macro save_linux_s2_tf
    sub     sp, sp, #LINUX_S2_TF_SIZE
    stp     x0, x1, [sp, #0]
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    stp     x6, x7, [sp, #48]
    stp     x8, x9, [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    str     x30, [sp, #240]
.endm

.macro restore_linux_s2_tf
    ldp     x0, x1, [sp, #0]
    ldp     x2, x3, [sp, #16]
    ldp     x4, x5, [sp, #32]
    ldp     x6, x7, [sp, #48]
    ldp     x8, x9, [sp, #64]
    ldp     x10, x11, [sp, #80]
    ldp     x12, x13, [sp, #96]
    ldp     x14, x15, [sp, #112]
    ldp     x16, x17, [sp, #128]
    ldp     x18, x19, [sp, #144]
    ldp     x20, x21, [sp, #160]
    ldp     x22, x23, [sp, #176]
    ldp     x24, x25, [sp, #192]
    ldp     x26, x27, [sp, #208]
    ldp     x28, x29, [sp, #224]
    ldr     x30, [sp, #240]
    add     sp, sp, #LINUX_S2_TF_SIZE
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

    cmp     x17, #ESR_EL2_EC_DABT_LOW
    b.eq    hyp_dabt_lower_aarch64

    /* never reach if no err */
    ldp     x16, x17, [sp], #16
    log_hyp_exception hyp_log_lower_sync_unhandled
    b       hyp_stop

hyp_dabt_lower_aarch64:
    ldp     x16, x17, [sp], #16
    save_linux_s2_tf

    mrs     x16, esr_el2
    str     x16, [sp, #LINUX_S2_TF_ESR]
    mrs     x16, far_el2
    str     x16, [sp, #LINUX_S2_TF_FAR]
    mrs     x16, hpfar_el2
    str     x16, [sp, #LINUX_S2_TF_HPFAR]
    mrs     x16, elr_el2
    str     x16, [sp, #LINUX_S2_TF_ELR]

    mov     x0, sp
    bl      linux_stage2_abort

    cbnz    x0, hyp_s2_unhandled

    ldr     x16, [sp, #LINUX_S2_TF_ELR]
    msr     elr_el2, x16
    restore_linux_s2_tf
    eret

hyp_s2_unhandled:
    add     sp, sp, #LINUX_S2_TF_SIZE
    b       hyp_stop    /* 停住 */

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
    log_hyp_exception hyp_log_hvc_not_supported
    sub     sp, sp, #16
    str     x30, [sp]
    bl      hyp_log_hvc_args
    ldr     x30, [sp]
    add     sp, sp, #16
    mov     x0, #-1
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    eret

hyp_current_sync:
    log_hyp_exception hyp_log_current_sync
    b       hyp_stop

hyp_current_irq:
    log_hyp_exception hyp_log_current_irq
    b       hyp_stop

hyp_current_fiq:
    log_hyp_exception hyp_log_current_fiq
    b       hyp_stop

hyp_current_serror:
    log_hyp_exception hyp_log_current_serror
    b       hyp_stop

hyp_lower_irq:
    save_linux_s2_tf
    bl      linux_vgic_handle_physical_irq
    cbnz    x0, hyp_lower_irq_unhandled
    restore_linux_s2_tf
    eret

hyp_lower_irq_unhandled:
    add     sp, sp, #LINUX_S2_TF_SIZE
    log_hyp_exception hyp_log_lower_irq
    b       hyp_stop

hyp_lower_fiq:
    log_hyp_exception hyp_log_lower_fiq
    b       hyp_stop

hyp_lower_serror:
    log_hyp_exception hyp_log_lower_serror
    b       hyp_stop

hyp_stop:
    wfe
    b       hyp_stop

    .section ".rodata"
hyp_log_lower_sync_unhandled:
    .asciz "[hyp] lower sync unhandled"
hyp_log_hvc_not_supported:
    .asciz "[hyp] hvc not supported"
hyp_log_current_sync:
    .asciz "[hyp] current sync"
hyp_log_current_irq:
    .asciz "[hyp] current irq"
hyp_log_current_fiq:
    .asciz "[hyp] current fiq"
hyp_log_current_serror:
    .asciz "[hyp] current serror"
hyp_log_lower_irq:
    .asciz "[hyp] lower irq"
hyp_log_lower_fiq:
    .asciz "[hyp] lower fiq"
hyp_log_lower_serror:
    .asciz "[hyp] lower serror"
