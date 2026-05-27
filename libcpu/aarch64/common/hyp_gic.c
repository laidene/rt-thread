/*
 * qemu virt64 el2 启动路径里的最小 gic trap 策略。
 */

#include <rtthread.h>
#include <rtcompiler.h>
#include <cache.h>

#define esr_el2_dabt_wnr               (1ul << 6)
#define esr_el2_dabt_sas_shift         22
#define esr_el2_dabt_sas_mask          (3ul << esr_el2_dabt_sas_shift)
#define esr_el2_dabt_srt_shift         16
#define esr_el2_dabt_srt_mask          (0x1ful << esr_el2_dabt_srt_shift)
#define esr_el2_dabt_sse               (1ul << 21)
#define esr_el2_dabt_isv               (1ul << 24)

#define gicd_base                      0x08000000ul
#define gicd_end                       0x08010000ul

#define gicc_base                      0x08010000ul
#define gich_base                      0x08030000ul

#define gicd_ctlr                      0x000u
#define gicd_isenabler                 0x100u
#define gicd_icenabler                 0x180u
#define gicd_ispendr                   0x200u
#define gicd_icpendr                   0x280u
#define gicd_ipriorityr                0x400u
#define gicd_itargetsr                 0x800u
#define gicd_icfgr                     0xc00u
#define gicd_cpu3_target_x4            0x08080808u

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

extern rt_uint32_t linux_gicd_shadow[] rt_weak;

rt_uint64_t hyp_gicd_write_trap_count rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_gicd_write_sync_count rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_gicd_write_denied_count rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gicd_write_gpa rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gicd_write_esr rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gicd_write_value rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gicd_write_offset rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gicd_write_sync_mask rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gicd_write_denied_mask rt_section(".bss.noclean.hyp");
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

static void shadow_write32(rt_uint32_t offset, rt_uint32_t value)
{
  linux_gicd_shadow[offset / sizeof(rt_uint32_t)] = value;
  rt_hw_cpu_dcache_clean(&linux_gicd_shadow[offset / sizeof(rt_uint32_t)], sizeof(rt_uint32_t));
}

static void real_rmw32(rt_uint32_t offset, rt_uint32_t value, rt_uint32_t mask)
{
  rt_uint32_t old = mmio_read32(gicd_base, offset);

  mmio_write32(gicd_base, offset, (old & ~mask) | (value & mask));
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

static rt_bool_t linux_gicd_shadow_ready(void)
{
  return linux_gicd_shadow != RT_NULL;
}

static rt_uint32_t linux_owned_irq_mask(rt_uint32_t first_intid, rt_uint32_t width)
{
  rt_uint32_t mask = 0;
  rt_uint32_t bit;

  for (bit = 0; bit < width; bit++) {
    if (linux_owns_irq(first_intid + bit)) {
      mask |= 1u << bit;
    }
  }

  return mask;
}

static rt_uint32_t linux_owned_irq_byte_mask(rt_uint32_t first_intid, rt_uint32_t width)
{
  rt_uint32_t mask = 0;
  rt_uint32_t index;

  for (index = 0; index < width; index++) {
    if (linux_owns_irq(first_intid + index)) {
      mask |= 0xffu << (index * 8u);
    }
  }

  return mask;
}

static rt_uint32_t linux_owned_irq_config_mask(rt_uint32_t first_intid, rt_uint32_t width)
{
  rt_uint32_t mask = 0;
  rt_uint32_t index;

  for (index = 0; index < width; index++) {
    if (linux_owns_irq(first_intid + index)) {
      mask |= 0x3u << (index * 2u);
    }
  }

  return mask;
}

static void hyp_gicd_shadow_rmw32(rt_uint32_t offset, rt_uint32_t value, rt_uint32_t mask)
{
  rt_uint32_t old;

  old = linux_gicd_shadow[offset / sizeof(rt_uint32_t)];
  shadow_write32(offset, (old & ~mask) | (value & mask));
}

static rt_uint64_t hyp_guest_reg_value(rt_uint64_t *regs, rt_uint32_t reg, rt_uint32_t bytes, rt_bool_t sign_extend)
{
  rt_uint64_t value;
  rt_uint32_t bits;

  if (reg == 31) {
    value = 0;
  } else {
    value = regs[reg];
  }

  bits = bytes * 8;
  if (bits >= 64) {
    return value;
  }

  value &= (1ull << bits) - 1ull;
  if (sign_extend && (value & (1ull << (bits - 1)))) {
    value |= ~((1ull << bits) - 1ull);
  }

  return value;
}

static rt_bool_t hyp_gicd_decode_store(rt_uint64_t esr, rt_uint64_t *regs, rt_uint64_t *value, rt_uint32_t *bytes)
{
  rt_uint32_t sas;
  rt_uint32_t srt;

  if (!(esr & esr_el2_dabt_isv)) {
    return RT_FALSE;
  }

  sas = (esr & esr_el2_dabt_sas_mask) >> esr_el2_dabt_sas_shift;
  if (sas > 2) {
    return RT_FALSE;
  }

  srt = (esr & esr_el2_dabt_srt_mask) >> esr_el2_dabt_srt_shift;
  *bytes = 1u << sas;
  *value = hyp_guest_reg_value(regs, srt, *bytes, (esr & esr_el2_dabt_sse) != 0);

  return RT_TRUE;
}

static rt_uint32_t hyp_gicd_expand_write(rt_uint32_t offset, rt_uint64_t value, rt_uint32_t bytes)
{
  rt_uint32_t shift = (offset & 0x3u) * 8u;

  if (bytes >= 4) {
    return (rt_uint32_t)value;
  }

  return ((rt_uint32_t)value & ((1u << (bytes * 8u)) - 1u)) << shift;
}

static rt_uint32_t hyp_gicd_byte_mask(rt_uint32_t offset, rt_uint32_t bytes)
{
  rt_uint32_t mask = 0;
  rt_uint32_t i;

  for (i = 0; i < bytes; i++) {
    mask |= 0xffu << (((offset + i) & 0x3u) * 8u);
  }

  return mask;
}

static rt_bool_t hyp_gicd_sync_group32(rt_uint32_t offset,
                                       rt_uint32_t value,
                                       rt_uint32_t base,
                                       rt_bool_t clear_semantics)
{
  rt_uint32_t index = (offset - base) / sizeof(rt_uint32_t);
  rt_uint32_t first_intid = index * 32u;
  rt_uint32_t allowed = linux_owned_irq_mask(first_intid, 32u);
  rt_uint32_t sync = value & allowed;
  rt_uint32_t denied = value & ~allowed;

  hyp_last_gicd_write_sync_mask = sync;
  hyp_last_gicd_write_denied_mask = denied;

  if (denied != 0) {
    hyp_gicd_write_denied_count++;
  }

  if (sync == 0) {
    return RT_TRUE;
  }

  hyp_gicd_shadow_rmw32(offset, clear_semantics ? 0 : sync, sync);
  mmio_write32(gicd_base, offset, sync);
  hyp_gicd_write_sync_count++;

  return RT_TRUE;
}

static rt_bool_t hyp_gicd_sync_enable(rt_uint32_t offset, rt_uint32_t value, rt_uint32_t base, rt_bool_t clear_semantics)
{
  rt_uint32_t index = (offset - base) / sizeof(rt_uint32_t);
  rt_uint32_t state_offset = gicd_isenabler + index * sizeof(rt_uint32_t);
  rt_uint32_t clear_offset = gicd_icenabler + index * sizeof(rt_uint32_t);
  rt_bool_t handled;

  handled = hyp_gicd_sync_group32(offset, value, base, clear_semantics);
  if (handled && hyp_last_gicd_write_sync_mask != 0) {
    hyp_gicd_shadow_rmw32(state_offset,
                          clear_semantics ? 0 : hyp_last_gicd_write_sync_mask,
                          hyp_last_gicd_write_sync_mask);
    hyp_gicd_shadow_rmw32(clear_offset,
                          clear_semantics ? 0 : hyp_last_gicd_write_sync_mask,
                          hyp_last_gicd_write_sync_mask);
  }

  return handled;
}

static rt_bool_t hyp_gicd_sync_pending(rt_uint32_t offset, rt_uint32_t value, rt_uint32_t base, rt_bool_t clear_semantics)
{
  rt_uint32_t index = (offset - base) / sizeof(rt_uint32_t);
  rt_uint32_t state_offset = gicd_ispendr + index * sizeof(rt_uint32_t);
  rt_uint32_t clear_offset = gicd_icpendr + index * sizeof(rt_uint32_t);
  rt_bool_t handled;

  handled = hyp_gicd_sync_group32(offset, value, base, clear_semantics);
  if (handled && hyp_last_gicd_write_sync_mask != 0) {
    hyp_gicd_shadow_rmw32(state_offset,
                          clear_semantics ? 0 : hyp_last_gicd_write_sync_mask,
                          hyp_last_gicd_write_sync_mask);
    hyp_gicd_shadow_rmw32(clear_offset,
                          clear_semantics ? 0 : hyp_last_gicd_write_sync_mask,
                          hyp_last_gicd_write_sync_mask);
  }

  return handled;
}

static rt_bool_t hyp_gicd_sync_priority(rt_uint32_t offset, rt_uint32_t value, rt_uint32_t byte_mask)
{
  rt_uint32_t first_intid = offset - gicd_ipriorityr;
  rt_uint32_t allowed = linux_owned_irq_byte_mask(first_intid, 4u) & byte_mask;
  rt_uint32_t denied = byte_mask & ~allowed;

  hyp_last_gicd_write_sync_mask = allowed;
  hyp_last_gicd_write_denied_mask = denied;

  if (denied != 0) {
    hyp_gicd_write_denied_count++;
  }

  hyp_gicd_shadow_rmw32(offset, value, allowed);
  if (allowed != 0) {
    real_rmw32(offset, linux_gicd_shadow[offset / sizeof(rt_uint32_t)], allowed);
    hyp_gicd_write_sync_count++;
  }

  return RT_TRUE;
}

static rt_bool_t hyp_gicd_sync_targets(rt_uint32_t offset, rt_uint32_t value, rt_uint32_t byte_mask)
{
  rt_uint32_t first_intid = offset - gicd_itargetsr;
  rt_uint32_t allowed = linux_owned_irq_byte_mask(first_intid, 4u) & byte_mask;
  rt_uint32_t denied = byte_mask & ~allowed;
  rt_uint32_t real_value = gicd_cpu3_target_x4;

  hyp_last_gicd_write_sync_mask = allowed;
  hyp_last_gicd_write_denied_mask = denied;

  if (denied != 0) {
    hyp_gicd_write_denied_count++;
  }

  hyp_gicd_shadow_rmw32(offset, value, allowed);
  if (allowed != 0) {
    real_rmw32(offset, real_value, allowed);
    hyp_gicd_write_sync_count++;
  }

  return RT_TRUE;
}

static rt_bool_t hyp_gicd_sync_config(rt_uint32_t offset, rt_uint32_t value, rt_uint32_t byte_mask)
{
  rt_uint32_t index = (offset - gicd_icfgr) / sizeof(rt_uint32_t);
  rt_uint32_t first_intid = index * 16u;
  rt_uint32_t allowed = linux_owned_irq_config_mask(first_intid, 16u) & byte_mask;
  rt_uint32_t denied = byte_mask & ~allowed;
  hyp_last_gicd_write_sync_mask = allowed;
  hyp_last_gicd_write_denied_mask = denied;

  if ((value & denied) != 0) {
    hyp_gicd_write_denied_count++;
  }

  hyp_gicd_shadow_rmw32(offset, value, allowed);
  if (allowed != 0) {
    real_rmw32(offset, linux_gicd_shadow[offset / sizeof(rt_uint32_t)], allowed);
    hyp_gicd_write_sync_count++;
  }

  return RT_TRUE;
}

static rt_bool_t hyp_gicd_emulate_write(rt_uint32_t offset, rt_uint64_t raw_value, rt_uint32_t bytes)
{
  rt_uint32_t aligned = offset & ~0x3u;
  rt_uint32_t value = hyp_gicd_expand_write(offset, raw_value, bytes);
  rt_uint32_t byte_mask = hyp_gicd_byte_mask(offset, bytes);

  hyp_last_gicd_write_offset = offset;
  hyp_last_gicd_write_value = raw_value;
  hyp_last_gicd_write_sync_mask = 0;
  hyp_last_gicd_write_denied_mask = 0;

  if (aligned == gicd_ctlr) {
    hyp_gicd_shadow_rmw32(aligned, value, byte_mask);
    hyp_gicd_write_sync_count++;
    return RT_TRUE;
  }

  if (aligned >= gicd_isenabler && aligned < gicd_isenabler + 0x80u) {
    return hyp_gicd_sync_enable(aligned, value, gicd_isenabler, RT_FALSE);
  }

  if (aligned >= gicd_icenabler && aligned < gicd_icenabler + 0x80u) {
    return hyp_gicd_sync_enable(aligned, value, gicd_icenabler, RT_TRUE);
  }

  if (aligned >= gicd_ispendr && aligned < gicd_ispendr + 0x80u) {
    return hyp_gicd_sync_pending(aligned, value, gicd_ispendr, RT_FALSE);
  }

  if (aligned >= gicd_icpendr && aligned < gicd_icpendr + 0x80u) {
    return hyp_gicd_sync_pending(aligned, value, gicd_icpendr, RT_TRUE);
  }

  if (aligned >= gicd_ipriorityr && aligned < gicd_ipriorityr + 0x400u) {
    return hyp_gicd_sync_priority(aligned, value, byte_mask);
  }

  if (aligned >= gicd_itargetsr && aligned < gicd_itargetsr + 0x400u) {
    return hyp_gicd_sync_targets(aligned, value, byte_mask);
  }

  if (aligned >= gicd_icfgr && aligned < gicd_icfgr + 0x100u) {
    return hyp_gicd_sync_config(aligned, value, byte_mask);
  }

  hyp_gicd_write_denied_count++;
  return RT_TRUE;
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

rt_uint64_t hyp_handle_gicd_dabt(rt_uint64_t esr, rt_uint64_t gpa, rt_uint64_t *regs)
{
  rt_uint64_t value;
  rt_uint32_t bytes;

  if (!(esr & esr_el2_dabt_wnr)) {
    return 0;
  }

  if (gpa < gicd_base || gpa >= gicd_end) {
    return 0;
  }

  hyp_gicd_write_trap_count++;
  hyp_last_gicd_write_gpa = gpa;
  hyp_last_gicd_write_esr = esr;

  if (!linux_gicd_shadow_ready()) {
    return 0;
  }

  if (!hyp_gicd_decode_store(esr, regs, &value, &bytes)) {
    hyp_gicd_write_denied_count++;
    return 1;
  }

  hyp_gicd_emulate_write((rt_uint32_t)(gpa - gicd_base), value, bytes);
  hyp_gic_barrier();

  return 1;
}
