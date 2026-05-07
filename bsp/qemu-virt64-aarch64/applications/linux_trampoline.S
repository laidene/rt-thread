/*
 * Trampoline for booting a Linux Image on a reserved CPU.
 */

#ifndef __ASSEMBLY__
#define __ASSEMBLY__
#endif

#define LINUX_IMAGE_ENTRY       0x48200000
#define LINUX_DTB_ADDRESS       0x4f000000
#define LINUX_RAM_BASE          0x48000000
#define LINUX_RAM_2M_BLOCKS     64
#define VIRTIO_MMIO_BASE        0x0a000000
#define GIC_2M_BASE             0x08000000
#define GICD_BASE               0x08000000
#define GICC_BASE               0x08010000
#define GICH_BASE               0x08030000
#define HCR_EL2_RW              (1 << 31)
#define HCR_EL2_SWIO            (1 << 1)
#define HCR_EL2_VM              (1 << 0)
#define SPSR_EL1H_DAIF          0x3c5
#define VTCR_EL2_RES1           (1 << 31)
#define VTCR_EL2_SL0_LVL1       (1 << 6)
#define VTCR_EL2_IRGN0_WBWA     (1 << 8)
#define VTCR_EL2_ORGN0_WBWA     (1 << 10)
#define VTCR_EL2_SH0_INNER      (3 << 12)
#define S2_TABLE_DESC           0x3
#define S2_BLOCK_NORMAL_RW      0x7fd
#define S2_BLOCK_DEVICE_RW      0x4c1
#define S2_PAGE_DEVICE_RW       0x4c3



/* linux的启动入口,先搭建stage-2 */
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

    mrs     x5, cnthctl_el2
    orr     x5, x5, #0x3
    msr     cnthctl_el2, x5         /* 放开 EL1 访问物理计时器 */
    msr     cntvoff_el2, xzr

    bl      linux_stage2_setup

    movz    x5, #(HCR_EL2_RW >> 16), lsl #16
    orr     x5, x5, #HCR_EL2_SWIO
    orr     x5, x5, #HCR_EL2_VM
    msr     hcr_el2, x5             /* 再写 HCR_EL2 打开虚拟化翻译 */

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

/* 真正建 Stage-2 */
/* 4 块内存缓冲区，运行时被手工填成 Stage-2 页表 */
linux_stage2_setup:
    ldr     x6, =linux_stage2_l1
    mov     x7, x6
    mov     x8, #0x4000

.linux_stage2_clear:
    str     xzr, [x7], #8
    subs    x8, x8, #8
    b.ne    .linux_stage2_clear

    ldr     x6, =linux_stage2_l1
    ldr     x7, =linux_stage2_l2_low
    ldr     x8, =linux_stage2_l2_high
    ldr     x9, =linux_stage2_l3_gic

    orr     x10, x7, #S2_TABLE_DESC
    str     x10, [x6, #0]
    orr     x10, x8, #S2_TABLE_DESC
    str     x10, [x6, #8]

    orr     x10, x9, #S2_TABLE_DESC
    str     x10, [x7, #(64 * 8)]

    ldr     x10, =VIRTIO_MMIO_BASE
    lsr     x11, x10, #21
    and     x11, x11, #0x1ff
    movz    x12, #S2_BLOCK_DEVICE_RW
    orr     x13, x10, x12
    str     x13, [x7, x11, lsl #3]

    ldr     x10, =LINUX_RAM_BASE
    mov     x11, #LINUX_RAM_2M_BLOCKS
    movz    x12, #S2_BLOCK_NORMAL_RW

.linux_stage2_map_ram:
    lsr     x13, x10, #21
    and     x13, x13, #0x1ff
    orr     x14, x10, x12
    str     x14, [x8, x13, lsl #3]
    add     x10, x10, #0x200000
    subs    x11, x11, #1
    b.ne    .linux_stage2_map_ram

    /*
     * Keep GICD writable for now so Linux can finish its early interrupt
     * controller bring-up.  Once console output is stable again we can
     * re-introduce finer-grained GICD trapping incrementally.
     */
    ldr     x10, =GICD_BASE
    mov     x11, #16
    movz    x12, #S2_PAGE_DEVICE_RW     /* 如果设置为只读权限就会被stage-2拦截 */

.linux_stage2_map_gicd:
    lsr     x13, x10, #12
    and     x13, x13, #0x1ff
    orr     x14, x10, x12
    str     x14, [x9, x13, lsl #3]
    add     x10, x10, #0x1000
    subs    x11, x11, #1
    b.ne    .linux_stage2_map_gicd

    ldr     x10, =GICC_BASE
    mov     x11, #16
    movz    x12, #S2_PAGE_DEVICE_RW

.linux_stage2_map_gicc:
    lsr     x13, x10, #12
    and     x13, x13, #0x1ff
    orr     x14, x10, x12
    str     x14, [x9, x13, lsl #3]
    add     x10, x10, #0x1000
    subs    x11, x11, #1
    b.ne    .linux_stage2_map_gicc

    ldr     x10, =GICH_BASE
    mov     x11, #32
    movz    x12, #S2_PAGE_DEVICE_RW

.linux_stage2_map_gich_gicv:
    lsr     x13, x10, #12
    and     x13, x13, #0x1ff
    orr     x14, x10, x12
    str     x14, [x9, x13, lsl #3]
    add     x10, x10, #0x1000
    subs    x11, x11, #1
    b.ne    .linux_stage2_map_gich_gicv

    ldr     x6, =linux_stage2_l1
    movz    x7, #1, lsl #48
    orr     x6, x6, x7
    msr     vttbr_el2, x6

    mrs     x7, id_aa64mmfr0_el1
    and     x7, x7, #0xf
    lsl     x7, x7, #16
    movz    x6, #(VTCR_EL2_RES1 >> 16), lsl #16
    orr     x6, x6, #32
    orr     x6, x6, #VTCR_EL2_SL0_LVL1
    orr     x6, x6, #VTCR_EL2_IRGN0_WBWA
    orr     x6, x6, #VTCR_EL2_ORGN0_WBWA
    orr     x6, x6, #VTCR_EL2_SH0_INNER
    orr     x6, x6, x7
    msr     vtcr_el2, x6

    dsb     sy
    tlbi    vmalls12e1
    dsb     sy
    isb
    ret

    .global linux_cpu3_trampoline_end
linux_cpu3_trampoline_end:

    .section ".bss.noclean.linux_hyp", "aw"
    .align 12
linux_stage2_l1:
    .space 4096
linux_stage2_l2_low:
    .space 4096
linux_stage2_l2_high:
    .space 4096
linux_stage2_l3_gic:
    .space 4096
linux_cpu3_hyp_stack:
    .space 4096
linux_cpu3_hyp_stack_top:
