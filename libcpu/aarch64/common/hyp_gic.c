/*
 * qemu virt64 el2 启动路径里的最小 gic trap 策略。
 */

#include <rtthread.h>

#define esr_el2_dabt_wnr               (1ul << 6)

#define gicd_base                      0x08000000ul
#define gicd_end                       0x08010000ul

#define gicc_base                      0x08010000ul
#define gich_base                      0x08030000ul

#define gicc_ctlr                      0x000u
#define gicc_pmr                       0x004u
#define gicc_bpr                       0x008u
#define gicc_iar                       0x00cu
#define gicc_eoir                      0x010u
#define gicc_dir                       0x1000u

#define gicc_ctlr_enable               (1u << 0)
#define gicc_ctlr_eoimode              (1u << 9)

/* gich的寄存器偏移 */
#define gich_hcr                       0x000u
#define gich_vtr                       0x004u
#define gich_vmcr                      0x008u
#define gich_misr                      0x010u
#define gich_elrsr0                    0x030u
#define gich_apr                       0x0f0u
#define gich_lr0                       0x100u

#define gich_hcr_en                    (1u << 0)

#define gich_vmcr_enable_grp0          (1u << 0)
#define gich_vmcr_enable_grp1          (1u << 1)
#define gich_vmcr_binpoint_shift       21
#define gich_vmcr_primask_shift        27

#define gich_lr_virtual_id_mask        0x3ffu
#define gich_lr_phys_id_shift          10
#define gich_lr_priority_shift         23
#define gich_lr_pending                (1u << 28)
#define gich_lr_group1                 (1u << 30)
#define gich_lr_hw                     (1u << 31)

#define gic_spurious_intid             1023u
#define linux_irq_el2_phys_timer       26u
#define linux_irq_el1_virt_timer       27u
#define linux_irq_el1_sec_phys_timer   29u
#define linux_irq_el1_phys_timer       30u
#define linux_irq_virtio_blk           48u
#define linux_irq_virtio_console       49u

rt_uint64_t hyp_gicd_write_trap_count rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gicd_write_gpa rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gicd_write_esr rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_irq_trap_count rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_irq_inject_count rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_irq_lr_busy_count rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_irq_unowned_count rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_phys_irq rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_lr rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_lr_index rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_gich_lr_count rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gich_vtr rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gich_elrsr rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gich_hcr rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gich_vmcr rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gich_misr rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gich_lr0 rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gich_lr1 rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gich_lr2 rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gich_lr3 rt_section(".bss.noclean.hyp");

/* 读取 GIC/GICH 这类 MMIO 寄存器，base 是物理 MMIO 基地址。 */
static rt_uint32_t mmio_read32(rt_uint64_t base, rt_uint32_t offset)
{
  volatile rt_uint32_t *reg = (volatile rt_uint32_t *)(base + offset);

  return *reg;
}

/* 写入 GIC/GICH 这类 MMIO 寄存器，调用点必须保证当前地址可访问。 */
static void mmio_write32(rt_uint64_t base, rt_uint32_t offset, rt_uint32_t value)
{
  volatile rt_uint32_t *reg = (volatile rt_uint32_t *)(base + offset);

  *reg = value;
}

/* 判断物理 INTID 是否属于 Linux，只有这些中断会被 EL2 虚拟注入。 */
static rt_bool_t linux_owns_irq(rt_uint32_t intid)
{
  return intid == linux_irq_el2_phys_timer ||
         intid == linux_irq_el1_virt_timer ||
         intid == linux_irq_el1_sec_phys_timer ||
         intid == linux_irq_el1_phys_timer ||
         intid == linux_irq_virtio_blk ||
         intid == linux_irq_virtio_console;
}

/* 保证 GIC/GICH 寄存器访问顺序已经对硬件生效。 */
static void hyp_gic_barrier(void)
{
  __asm__ volatile("dsb sy" ::: "memory");
  __asm__ volatile("isb" ::: "memory");
}

/* 清空 GICH 的 List Registers，避免 Linux 启动前残留虚拟中断。 */
static void hyp_gich_clear_lrs(rt_uint32_t lr_count)
{
  rt_uint32_t i;

  if (lr_count > 32) {
    lr_count = 32;
  }

  for (i = 0; i < lr_count; i++) {
    mmio_write32(gich_base, gich_lr0 + i * sizeof(rt_uint32_t), 0);
  }
}

/* 从 ELRSR 中查找一个空闲 LR，返回 LR 下标；没有空闲槽时返回 -1。 */
static rt_int32_t hyp_gich_find_empty_lr(rt_uint32_t elrsr)
{
  rt_uint32_t i;
  rt_uint32_t lr_count = hyp_gich_lr_count;

  if (lr_count > 32) {
    lr_count = 32;
  }

  for (i = 0; i < lr_count; i++) {
    if (elrsr & (1u << i)) {
      return i;
    }
  }

  return -1;
}

/*
 * 缓存 GICH 当前状态，供 RTT shell 的 hyp irq 命令打印。
 * 注意：shell 在 RTT EL1 环境运行，不能直接访问未映射的 GICH 物理地址。
 */
void hyp_gich_snapshot(void)
{
  hyp_last_gich_hcr = mmio_read32(gich_base, gich_hcr);
  hyp_last_gich_vmcr = mmio_read32(gich_base, gich_vmcr);
  hyp_last_gich_misr = mmio_read32(gich_base, gich_misr);
  hyp_last_gich_elrsr = mmio_read32(gich_base, gich_elrsr0);
  hyp_last_gich_lr0 = mmio_read32(gich_base, gich_lr0 + 0 * sizeof(rt_uint32_t));
  hyp_last_gich_lr1 = mmio_read32(gich_base, gich_lr0 + 1 * sizeof(rt_uint32_t));
  hyp_last_gich_lr2 = mmio_read32(gich_base, gich_lr0 + 2 * sizeof(rt_uint32_t));
  hyp_last_gich_lr3 = mmio_read32(gich_base, gich_lr0 + 3 * sizeof(rt_uint32_t));
}


/*
 * 初始化 GICv2 虚拟化接口。
 *
 * 1. 先关闭 GICH，读取 LR 数量，清空 LR 和 APR。
 * 2. 配置 GICH_VMCR：打开虚拟 Group0/Group1，设置虚拟 PMR/BPR。
 * 3. 启用 GICH，让 EL2 可以通过 LR 注入虚拟中断。
 * 4. 配置真实 GICC：设置 PMR/BPR，打开 CPU interface 和 EOImode，
 *    让 CPU3 的物理 IRQ 先进入 EL2。
 */
void hyp_gich_prepare(void)
{
  rt_uint32_t vtr;
  rt_uint32_t lr_count;
  rt_uint32_t vmcr;

  mmio_write32(gich_base, gich_hcr, 0);     /* 关闭gich虚拟化接口 */

  vtr = mmio_read32(gich_base, gich_vtr);
  lr_count = (vtr & 0x3fu) + 1u;            /* 硬件支持多少个 GICH_LR */
  if (lr_count > 32) {
    lr_count = 32;
  }
  hyp_last_gich_vtr = vtr;
  hyp_gich_lr_count = lr_count;
  hyp_gich_clear_lrs(lr_count);             /* 清空所有lr */
  mmio_write32(gich_base, gich_apr, 0);     /* 把gich虚拟状态重置干净，否则gicv可能认为某些中断还处于active状态 */

  vmcr = gich_vmcr_enable_grp0 |
         gich_vmcr_enable_grp1 |
         ((0xf0u >> 3) << gich_vmcr_primask_shift) |
         (7u << gich_vmcr_binpoint_shift);          /* bit[0] EnableGrp0 = 1   bit[1] EnableGrp1 = 1 Priority Mask = 0xf0  Binary Point = 7  */
  mmio_write32(gich_base, gich_vmcr, vmcr);         /* 允许虚拟 Group0和Group1 中断 设置优先级屏蔽值0xf0 拟 CPU interface 的优先级分组 */

  mmio_write32(gich_base, gich_hcr, gich_hcr_en);   /* 打开gich */

  mmio_write32(gicc_base, gicc_pmr, 0xf0);          /* 设置真实中断优先级屏蔽值 */
  mmio_write32(gicc_base, gicc_bpr, 7);             /* 设置真实优先级分组 */
  mmio_write32(gicc_base, gicc_ctlr, gicc_ctlr_enable | gicc_ctlr_eoimode); /* 打开真实GICC，并启用EOImode，让EL2接收物理IRQ并控制deactivate时机 */

  hyp_gic_barrier();
  hyp_gich_snapshot();
}

/*
 * 处理从 Linux EL1 路由到 EL2 的物理 IRQ。
 * EL2 读取真实 GICC_IAR，确认该中断属于 Linux 后，写入一个空闲 GICH_LR，
 * 让 Linux 通过 GICV 看到同号虚拟中断。
 */
rt_uint64_t hyp_handle_irq_lower_aarch64(void)
{
  rt_uint32_t iar;
  rt_uint32_t intid;
  rt_uint32_t elrsr;
  rt_uint32_t lr;
  rt_uint32_t vmcr;
  rt_int32_t lr_index;

  hyp_irq_trap_count++;

  iar = mmio_read32(gicc_base, gicc_iar);
  intid = iar & gich_lr_virtual_id_mask;
  hyp_last_phys_irq = intid;

  if (intid == gic_spurious_intid) {
    return 1;
  }

  if (!linux_owns_irq(intid)) {
    hyp_irq_unowned_count++;
    mmio_write32(gicc_base, gicc_eoir, iar);
    mmio_write32(gicc_base, gicc_dir, iar);
    hyp_gic_barrier();
    return 1;
  }

  elrsr = mmio_read32(gich_base, gich_elrsr0);
  hyp_last_gich_elrsr = elrsr;
  lr_index = hyp_gich_find_empty_lr(elrsr);
  if (lr_index < 0) {
    hyp_irq_lr_busy_count++;
    mmio_write32(gicc_base, gicc_eoir, iar);
    mmio_write32(gicc_base, gicc_dir, iar);
    hyp_gic_barrier();
    return 1;
  }

  vmcr = mmio_read32(gich_base, gich_vmcr);
  lr = (intid & gich_lr_virtual_id_mask) |
       ((intid & gich_lr_virtual_id_mask) << gich_lr_phys_id_shift) |
       ((0xa0u >> 3) << gich_lr_priority_shift) |
       gich_lr_pending |
       gich_lr_hw;
  if (vmcr & gich_vmcr_enable_grp1) {
    lr |= gich_lr_group1;
  }

  mmio_write32(gich_base, gich_lr0 + lr_index * sizeof(rt_uint32_t), lr);
  hyp_last_lr = lr;
  hyp_last_lr_index = lr_index;
  hyp_irq_inject_count++;

  mmio_write32(gicc_base, gicc_eoir, iar);
  hyp_gic_barrier();
  hyp_gich_snapshot();

  return 1;
}

rt_uint64_t hyp_handle_gicd_dabt(rt_uint64_t esr, rt_uint64_t gpa)
{
  if (!(esr & esr_el2_dabt_wnr)) {
    return 0;
  }

  if (gpa < gicd_base || gpa >= gicd_end) {
    return 0;
  }

  hyp_gicd_write_trap_count++;
  hyp_last_gicd_write_gpa = gpa;
  hyp_last_gicd_write_esr = esr;

  return 1;
}
