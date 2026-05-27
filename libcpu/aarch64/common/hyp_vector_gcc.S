/*
 * qemu virt64 最小 el2 异常向量。
 *
 * 本文件负责：
 * - 接管 更低级的 el aarch64 同步异常；
 * - 处理 rt-thread 自定义 hvc 和 linux psci hvc 代理；
 * - 仿真 linux 对 gicd shadow 区域触发的写访问异常。
 *
 * 这里尽量只保留入口、现场保存和最小分发逻辑；gic 虚拟化策略
 * 放在 hyp_gic.c 中实现。
 */

/* ec类型 */
#define esr_el2_ec_shift                26
#define esr_el2_ec_type_hvc64           0x16
#define esr_el2_ec_type_dabt_low        0x24

/* 自定义的hvc funciton id */
#define hypercall_version_id            0xc5000000
#define hypercall_debug_id              0xc5000001
#define hypercall_cpu_on_id             0xc5000003

/* 自定义测试返回值 */
#define hyp_version_0_1                 0x00010000
#define hyp_custom_test_value           0x48564321

/* 标准的psci hvc/smc funcrion id */
#define psci_0_2_fn64_cpu_on            0xc4000003

/* PSCI 的 function id 编码里 最高8位区别SMC32/HVC32 风格调用 还是 SMC64/HVC64 风格调用  */
#define psci_32_prefix                  0x84
#define psci_64_prefix                  0xc4

.macro vector_slot offset, target
    .org (hyp_vbar + \offset)
    b \target
.endm

.macro eret_zero_x1_x3
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    eret
.endm

.macro guest_save_regs
    sub     sp, sp, #256
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

.macro guest_restore_regs
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
    add     sp, sp, #256
.endm

.text
.align 11
.global hyp_vectors
hyp_vectors:
    .set hyp_vbar, hyp_vectors
    .org hyp_vbar

    /* 当前 el，使用 sp_el0。 */
    vector_slot 0x000, hyp_stop
    vector_slot 0x080, hyp_stop
    vector_slot 0x100, hyp_stop
    vector_slot 0x180, hyp_stop

    /* 当前 el，使用 sp_elx。 */
    vector_slot 0x200, hyp_stop
    vector_slot 0x280, hyp_stop
    vector_slot 0x300, hyp_stop
    vector_slot 0x380, hyp_stop

    /* lower el，aarch64。 */
    vector_slot 0x400, hyp_sync_lower_aarch64
    vector_slot 0x480, hyp_irq_lower_aarch64 /*  把物理 IRQ 路由到 EL2 然后 EL2 的 IRQ 入口 */
    vector_slot 0x500, hyp_stop
    vector_slot 0x580, hyp_stop

    /* lower el，aarch32。 */
    vector_slot 0x600, hyp_stop
    vector_slot 0x680, hyp_stop
    vector_slot 0x700, hyp_stop
    vector_slot 0x780, hyp_stop

hyp_sync_lower_aarch64:
    sub     sp, sp, #16
    stp     x16, x17, [sp]

    mrs     x16, esr_el2
    lsr     x17, x16, #esr_el2_ec_shift
    cmp     x17, #esr_el2_ec_type_hvc64
    b.eq    hyp_hvc_lower_aarch64
    cmp     x17, #esr_el2_ec_type_dabt_low
    b.eq    hyp_dabt_lower_aarch64

    ldp     x16, x17, [sp], #16
    b       hyp_stop

hyp_hvc_lower_aarch64:
    add     sp, sp, #16

    movz    w16, #(hypercall_version_id >> 16), lsl #16
    cmp     w0, w16
    b.eq    hyp_hvc_version

    movz    w16, #(hypercall_debug_id >> 16), lsl #16
    add     w16, w16, #(hypercall_debug_id & 0xffff)
    cmp     w0, w16
    b.eq    hyp_hvc_custom_test

    movz    w16, #(hypercall_cpu_on_id >> 16), lsl #16
    add     w16, w16, #(hypercall_cpu_on_id & 0xffff)
    cmp     w0, w16
    b.eq    hyp_hvc_cpu_on

    /*
     * linux 使用 hvc 作为 psci 方法。el2 接管 hvc trap，并用 smc
     * 继续转发给 qemu/el3。function id 放在 x0/w0.
     */
    lsr     w16, w0, #24
    cmp     w16, #psci_32_prefix
    b.eq    hyp_hvc_psci_proxy
    cmp     w16, #psci_64_prefix
    b.eq    hyp_hvc_psci_proxy

    b       hyp_hvc_not_supported

hyp_hvc_version:
    movz    w0, #(hyp_version_0_1 >> 16), lsl #16
    eret_zero_x1_x3

hyp_hvc_custom_test:
    movz    w0, #(hyp_custom_test_value >> 16), lsl #16
    movk    w0, #(hyp_custom_test_value & 0xffff)
    eret_zero_x1_x3

.global hyp_hvc_cpu_on
hyp_hvc_cpu_on:
    /*
     * x1 = 目标 mpidr
     * x2 = 入口地址
     * x3 = 上下文参数
     */
    movz    w0, #(psci_0_2_fn64_cpu_on >> 16), lsl #16
    add     w0, w0, #(psci_0_2_fn64_cpu_on & 0xffff)
    smc     #0

    /* x0 保留 psci 返回值。 */
    eret_zero_x1_x3

.global hyp_hvc_psci_proxy
hyp_hvc_psci_proxy:
    smc     #0
    eret

hyp_hvc_not_supported:
    mov     x0, #-1
    eret_zero_x1_x3

hyp_dabt_lower_aarch64:
    ldp     x16, x17, [sp], #16
    guest_save_regs

    mrs     x0, esr_el2
    mrs     x1, hpfar_el2
    lsl     x1, x1, #8
    mrs     x2, far_el2
    and     x2, x2, #0xfff
    orr     x1, x1, x2
    mov     x2, sp

    bl      hyp_handle_gicd_dabt
    cbz     x0, hyp_dabt_restore_stop

    mrs     x16, elr_el2
    add     x16, x16, #4
    msr     elr_el2, x16

    guest_restore_regs
    eret

hyp_dabt_restore_stop:
    guest_restore_regs
    b       hyp_stop

hyp_irq_lower_aarch64:
    /*
     * Linux EL1 的物理 IRQ 被 HCR_EL2.IMO 路由到这里。
     * 先保存 Linux 通用寄存器，再调用 C 层读取真实 GICC 并写 GICH_LR。
     */
    guest_save_regs

    bl      hyp_handle_irq_lower_aarch64

    guest_restore_regs
    eret

hyp_stop:
    wfe
    b       hyp_stop
