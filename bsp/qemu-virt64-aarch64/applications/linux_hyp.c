/*
 * EL2 setup for the Linux CPU.
 *
 * 本文件负责为 Linux 所在 cpu 准备最小 el2 运行环境：
 * - 建立 Linux 使用的 stage-2 页表；
 * - 配置 RAM、virtio 和 gic 相关地址的二阶段映射；
 * - 初始化 Linux 可见的 gicd shadow，并预先设置分配给 Linux 的中断；
 * - 打开 hcr_el2/vtcr_el2/vttbr_el2 等寄存器，使 Linux 以 el1 运行。
 */

#include <rtthread.h>
#include <cache.h>
#include <cpuport.h>

#define LINUX_RAM_BASE      0x48000000UL
#define LINUX_RAM_2M_BLOCKS 64UL
#define VIRTIO_MMIO_BASE    0x0a000000UL

#define GICD_BASE           0x08000000UL
#define GICD_SIZE           0x00010000UL
#define GICC_BASE           0x08010000UL
#define GICH_BASE           0x08030000UL
#define GICV_BASE           0x08040000UL

#define HCR_EL2_RW          (1UL << 31)
#define HCR_EL2_IMO         (1UL << 4)
#define HCR_EL2_SWIO        (1UL << 1)
#define HCR_EL2_VM          (1UL << 0)

#define VTCR_EL2_RES1       (1UL << 31)
#define VTCR_EL2_SL0_LVL1   (1UL << 6)
#define VTCR_EL2_IRGN0_WBWA (1UL << 8)
#define VTCR_EL2_ORGN0_WBWA (1UL << 10)
#define VTCR_EL2_SH0_INNER  (3UL << 12)

#define S2_TABLE_DESC       0x3UL
#define S2_BLOCK_NORMAL_RW  0x7fdUL
#define S2_BLOCK_DEVICE_RW  0x4c1UL
#define S2_PAGE_NORMAL_RW   0x7ffUL
#define S2_PAGE_DEVICE_RW   0x4c3UL
#define S2_PAGE_DEVICE_RO   0x443UL

#define S2_TABLE_ENTRIES    512U
#define S2_PAGE_SIZE        0x1000UL
#define S2_BLOCK_SIZE       0x200000UL

#define GICD_CTLR           0x000U
#define GICD_TYPER          0x004U
#define GICD_IIDR           0x008U
#define GICD_ISENABLER0     0x100U
#define GICD_ISENABLER1     0x104U
#define GICD_ICENABLER0     0x180U
#define GICD_ICENABLER1     0x184U
#define GICD_ITARGETSR0     0x800U
#define GICD_ITARGETSR8     0x820U
#define GICD_ITARGETSR48    0x830U
#define GICD_ICFGR48        0xc0cU

static rt_uint64_t linux_stage2_l1_00000000_7fffffff[S2_TABLE_ENTRIES] rt_align(S2_PAGE_SIZE)
  rt_section(".bss.noclean.linux_hyp");
static rt_uint64_t linux_stage2_l2_00000000_3fffffff[S2_TABLE_ENTRIES] rt_align(S2_PAGE_SIZE)
  rt_section(".bss.noclean.linux_hyp");
static rt_uint64_t linux_stage2_l2_40000000_7fffffff[S2_TABLE_ENTRIES] rt_align(S2_PAGE_SIZE)
  rt_section(".bss.noclean.linux_hyp");
static rt_uint64_t linux_stage2_l3_08000000_081fffff[S2_TABLE_ENTRIES] rt_align(S2_PAGE_SIZE)
  rt_section(".bss.noclean.linux_hyp");

rt_uint32_t linux_gicd_shadow[GICD_SIZE / sizeof(rt_uint32_t)] rt_align(S2_PAGE_SIZE)
  rt_section(".bss.noclean.linux_hyp");

static void zero_u64(rt_uint64_t *addr, rt_size_t count)
{
  while (count--) {
    *addr++ = 0;
  }
}

static void zero_u32(rt_uint32_t *addr, rt_size_t count)
{
  while (count--) {
    *addr++ = 0;
  }
}

static rt_uint64_t hyp_pa(const void *va)
{
  /*
   * CPU3 enters this trampoline by physical address before joining the
   * RT-Thread MMU context, so keep the old assembly assumption here:
   * this BSP links the trampoline data at physical addresses.
   */
  return (rt_uint64_t)va;
}

static rt_uint32_t gicd_read32(rt_uint32_t offset)
{
  volatile rt_uint32_t *reg = (volatile rt_uint32_t *)(GICD_BASE + offset);

  return *reg;
}

static void gicd_write32(rt_uint32_t offset, rt_uint32_t value)
{
  volatile rt_uint32_t *reg = (volatile rt_uint32_t *)(GICD_BASE + offset);

  *reg = value;
}

static void gicd_shadow_write32(rt_uint32_t offset, rt_uint32_t value)
{
  linux_gicd_shadow[offset / sizeof(rt_uint32_t)] = value;
}

static unsigned int s2_l2_index(rt_uint64_t ipa)
{
  return (ipa >> 21) & 0x1ff;
}

static unsigned int s2_l3_index(rt_uint64_t ipa)
{
  return (ipa >> 12) & 0x1ff;
}

static void s2_map_ram(void)
{
  rt_uint64_t ipa = LINUX_RAM_BASE;
  rt_size_t i;

  for (i = 0; i < LINUX_RAM_2M_BLOCKS; i++) {
    linux_stage2_l2_40000000_7fffffff[s2_l2_index(ipa)] = ipa | S2_BLOCK_NORMAL_RW;
    ipa += S2_BLOCK_SIZE;
  }
}

static void s2_map_device_block(rt_uint64_t ipa)
{
  linux_stage2_l2_00000000_3fffffff[s2_l2_index(ipa)] = ipa | S2_BLOCK_DEVICE_RW;
}

static void s2_map_gic_pages(rt_uint64_t ipa, rt_uint64_t pa, rt_size_t pages, rt_uint64_t attr)
{
  while (pages--) {
    linux_stage2_l3_08000000_081fffff[s2_l3_index(ipa)] = pa | attr;
    ipa += S2_PAGE_SIZE;
    pa += S2_PAGE_SIZE;
  }
}

/**
 * 准备stage-2页表
 * 重点
 *      1 把mmio的映射为device rw， 让linux可读可写。
 *      2 把内存地址设置为normal rw，让linux可读可写。
 *      3 把gicd读映射到 linux_gicd_shadow,写触发 stage-2 fault 后由 EL2 仿真。
 *      4 把gicc的读写映射到gicv，让linux用虚拟cpu interface处理中断。
 */
static void linux_stage2_setup(void)
{
  rt_uint64_t vtcr;
  rt_uint64_t pa_range;

  zero_u64(linux_stage2_l1_00000000_7fffffff, S2_TABLE_ENTRIES);
  zero_u64(linux_stage2_l2_00000000_3fffffff, S2_TABLE_ENTRIES);
  zero_u64(linux_stage2_l2_40000000_7fffffff, S2_TABLE_ENTRIES);
  zero_u64(linux_stage2_l3_08000000_081fffff, S2_TABLE_ENTRIES);

  linux_stage2_l1_00000000_7fffffff[0] = hyp_pa(linux_stage2_l2_00000000_3fffffff) | S2_TABLE_DESC;
  linux_stage2_l1_00000000_7fffffff[1] = hyp_pa(linux_stage2_l2_40000000_7fffffff) | S2_TABLE_DESC;
  linux_stage2_l2_00000000_3fffffff[s2_l2_index(GICD_BASE)] = hyp_pa(linux_stage2_l3_08000000_081fffff) | S2_TABLE_DESC;

  s2_map_device_block(VIRTIO_MMIO_BASE);
  s2_map_ram();

  s2_map_gic_pages(GICD_BASE, hyp_pa(linux_gicd_shadow), GICD_SIZE / S2_PAGE_SIZE, S2_PAGE_DEVICE_RO);
  s2_map_gic_pages(GICC_BASE, GICV_BASE, 16, S2_PAGE_DEVICE_RW);

  rt_hw_cpu_dcache_clean(linux_stage2_l1_00000000_7fffffff, sizeof(linux_stage2_l1_00000000_7fffffff));
  rt_hw_cpu_dcache_clean(linux_stage2_l2_00000000_3fffffff, sizeof(linux_stage2_l2_00000000_3fffffff));
  rt_hw_cpu_dcache_clean(linux_stage2_l2_40000000_7fffffff, sizeof(linux_stage2_l2_40000000_7fffffff));
  rt_hw_cpu_dcache_clean(linux_stage2_l3_08000000_081fffff, sizeof(linux_stage2_l3_08000000_081fffff));

  rt_hw_sysreg_write(vttbr_el2, hyp_pa(linux_stage2_l1_00000000_7fffffff) | (1UL << 48));

  rt_hw_sysreg_read(id_aa64mmfr0_el1, pa_range);
  pa_range = (pa_range & 0xf) << 16;

  vtcr =
    VTCR_EL2_RES1 | 32 | VTCR_EL2_SL0_LVL1 | VTCR_EL2_IRGN0_WBWA | VTCR_EL2_ORGN0_WBWA | VTCR_EL2_SH0_INNER | pa_range;
  rt_hw_sysreg_write(vtcr_el2, vtcr);

  __asm__ volatile("dsb sy" ::: "memory");
  __asm__ volatile("tlbi vmalls12e1" ::: "memory");
  __asm__ volatile("dsb sy" ::: "memory");
  rt_hw_isb();
}


/**
 * 配置 CPU3 运行 Linux 需要的 GICD 状态。
 *
 * 重点：
 * 1. 在真实 GICD 中提前使能 CPU3 timer PPI：10/11/13/14。
 * 2. 在真实 GICD 中把 virtio SPI 48/49 路由到 CPU3，并配置为 edge-triggered。
 * 3. 把 GICD_TYPER/GICD_IIDR 和 Linux 需要的 enable/target/config 初始状态
 *    写入 linux_gicd_shadow，供 Linux 访问它看到的 GICD。
 */
static void linux_gic_prepare(void)
{
  rt_uint32_t offset;

  zero_u32(linux_gicd_shadow, GICD_SIZE / sizeof(rt_uint32_t));

  gicd_shadow_write32(GICD_TYPER, gicd_read32(GICD_TYPER));
  gicd_shadow_write32(GICD_IIDR, gicd_read32(GICD_IIDR));
  for (offset = GICD_ITARGETSR0; offset < GICD_ITARGETSR8; offset += sizeof(rt_uint32_t)) {
    gicd_shadow_write32(offset, gicd_read32(offset));
  }

  /* Keep the real distributor programmed for Linux-owned IRQs. */
  gicd_write32(GICD_ISENABLER0, 0x6c00U << 16); /* CPU3 timer PPIs 10/11/13/14 */
  gicd_write32(GICD_ITARGETSR48, 0x0808U);      /* SPI 48/49 -> CPU3 */
  gicd_write32(GICD_ICFGR48, 0x0000000aU);      /* SPI 48/49 edge-triggered */
  gicd_write32(GICD_ISENABLER1, 0x3U << 16);    /* Enable SPI 48/49 */

  gicd_shadow_write32(GICD_CTLR, 0);
  gicd_shadow_write32(GICD_ISENABLER0, 0x6c00U << 16);
  gicd_shadow_write32(GICD_ICENABLER0, 0x6c00U << 16);
  gicd_shadow_write32(GICD_ITARGETSR48, 0x0808U);
  gicd_shadow_write32(GICD_ICFGR48, 0x0000000aU);
  gicd_shadow_write32(GICD_ISENABLER1, 0x3U << 16);
  gicd_shadow_write32(GICD_ICENABLER1, 0x3U << 16);

  rt_hw_cpu_dcache_clean(linux_gicd_shadow, sizeof(linux_gicd_shadow));
}

extern void hyp_gich_prepare(void);


/*
 * 为el2进入linux做准备
 * 重点:
 *      1 允许el1访问counter
 *      2 设置stage-2页表
 *      3 提前配置好虚拟串口和虚拟块设备中断
 *      4 el2接管el1中断
 */
void linux_el2_prepare(void)
{
  rt_uint64_t cnthctl;

  rt_hw_sysreg_read(cnthctl_el2, cnthctl);
  rt_hw_sysreg_write(cnthctl_el2, cnthctl | 0x3);
  rt_hw_sysreg_write(cntvoff_el2, 0);

  linux_stage2_setup();
  linux_gic_prepare();
  hyp_gich_prepare();

  /* 物理 IRQ 先进入 EL2 */
  rt_hw_sysreg_write(hcr_el2, HCR_EL2_RW | HCR_EL2_IMO | HCR_EL2_SWIO | HCR_EL2_VM);
  rt_hw_isb();
}
