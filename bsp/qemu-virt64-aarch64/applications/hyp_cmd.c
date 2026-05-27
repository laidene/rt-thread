/*
 * Minimal hypervisor bring-up probes.
 *
 * 本文件提供用于调试 el2/hypervisor 启动路径的 finsh 命令：
 * - `hyp` 用来查看当前异常级、测试 hvc 调用、查看 gicd trap 统计信息；
 * - `mpidr` 用来显示当前 cpu 的 mpidr 和 affinity。
 */

#include <rtthread.h>
#include <armv8.h>
#include <cpu.h>
#include <cpuport.h>
#include <hypercall.h>

#define HYP_CUSTOM_TEST_VALUE          0x48564321
#define GICD_ISENABLER0_IDX           (0x100 / 4)
#define GICD_ISENABLER1_IDX           (0x104 / 4)
#define GICD_ITARGETSR48_IDX          (0x830 / 4)
#define GICD_ICFGR48_IDX              (0xc0c / 4)

extern rt_uint64_t hyp_gicd_write_trap_count;
extern rt_uint64_t hyp_gicd_write_sync_count;
extern rt_uint64_t hyp_gicd_write_denied_count;
extern rt_uint64_t hyp_last_gicd_write_gpa;
extern rt_uint64_t hyp_last_gicd_write_esr;
extern rt_uint64_t hyp_last_gicd_write_value;
extern rt_uint64_t hyp_last_gicd_write_offset;
extern rt_uint64_t hyp_last_gicd_write_sync_mask;
extern rt_uint64_t hyp_last_gicd_write_denied_mask;
extern rt_uint64_t hyp_irq_trap_count;
extern rt_uint64_t hyp_irq_inject_count;
extern rt_uint64_t hyp_irq_lr_busy_count;
extern rt_uint64_t hyp_irq_unowned_count;
extern rt_uint64_t hyp_last_phys_irq;
extern rt_uint64_t hyp_last_lr;
extern rt_uint64_t hyp_last_lr_index;
extern rt_uint64_t hyp_gich_lr_count;
extern rt_uint64_t hyp_last_gich_vtr;
extern rt_uint64_t hyp_last_gich_elrsr;
extern rt_uint64_t hyp_last_gich_hcr;
extern rt_uint64_t hyp_last_gich_vmcr;
extern rt_uint64_t hyp_last_gich_misr;
extern rt_uint64_t hyp_last_gich_lr0;
extern rt_uint64_t hyp_last_gich_lr1;
extern rt_uint64_t hyp_last_gich_lr2;
extern rt_uint64_t hyp_last_gich_lr3;
extern rt_uint32_t linux_gicd_shadow[];

/*
 * hypervisor 调试命令。
 * hyp irq 打印的是 EL2 缓存的 GICH 快照，避免 RTT shell 在 EL1 直接访问
 * 未映射的 GICH 物理寄存器导致 data abort。
 */
static int hyp(int argc, char **argv)
{
  if (argc < 2 || !rt_strcmp(argv[1], "el")) {
    rt_kprintf("current EL: EL%lu\n", rt_hw_get_current_el());
    return RT_EOK;
  }

  if (!rt_strcmp(argv[1], "hvc")) {
    rt_uint32_t version = 0;
    rt_err_t err = rt_hv_version(&version);

    rt_kprintf("hvc version call: err=%d version=0x%08x\n", err, version);
    return err;
  }

  if (!rt_strcmp(argv[1], "custom")) {
    rt_uint32_t ret;

    ret = rt_hw_hypercall(HYPERCALL_DEBUG, 0x12345678, 1, 2, 3, 4, 5, 6);
    rt_kprintf("custom hvc call: ret=0x%08x\n", ret);

    return ret == HYP_CUSTOM_TEST_VALUE ? RT_EOK : -RT_ERROR;
  }

  if (!rt_strcmp(argv[1], "gicd")) {
    rt_kprintf("gicd trap=%lu sync=%lu denied=%lu\n",
               hyp_gicd_write_trap_count,
               hyp_gicd_write_sync_count,
               hyp_gicd_write_denied_count);
    rt_kprintf("last_gpa=0x%lx offset=0x%lx value=0x%lx esr=0x%lx sync_mask=0x%lx denied_mask=0x%lx\n",
               hyp_last_gicd_write_gpa,
               hyp_last_gicd_write_offset,
               hyp_last_gicd_write_value,
               hyp_last_gicd_write_esr,
               hyp_last_gicd_write_sync_mask,
               hyp_last_gicd_write_denied_mask);
    rt_kprintf("shadow ctlr=0x%08x isen0=0x%08x isen1=0x%08x tgt48=0x%08x icfgr48=0x%08x\n",
               linux_gicd_shadow[0],
               linux_gicd_shadow[GICD_ISENABLER0_IDX],
               linux_gicd_shadow[GICD_ISENABLER1_IDX],
               linux_gicd_shadow[GICD_ITARGETSR48_IDX],
               linux_gicd_shadow[GICD_ICFGR48_IDX]);
    return RT_EOK;
  }

  if (!rt_strcmp(argv[1], "irq")) {
    rt_kprintf("irq trap=%lu inject=%lu lr_busy=%lu unowned=%lu\n",
               hyp_irq_trap_count,
               hyp_irq_inject_count,
               hyp_irq_lr_busy_count,
               hyp_irq_unowned_count);
    rt_kprintf("last_phys_irq=%lu last_lr=0x%08lx lr_index=%lu lr_count=%lu gich_vtr=0x%08lx elrsr=0x%08lx\n",
               hyp_last_phys_irq,
               hyp_last_lr,
               hyp_last_lr_index,
               hyp_gich_lr_count,
               hyp_last_gich_vtr,
               hyp_last_gich_elrsr);
    rt_kprintf("gich hcr=0x%08lx vmcr=0x%08lx misr=0x%08lx lr0=0x%08lx lr1=0x%08lx lr2=0x%08lx lr3=0x%08lx\n",
               hyp_last_gich_hcr,
               hyp_last_gich_vmcr,
               hyp_last_gich_misr,
               hyp_last_gich_lr0,
               hyp_last_gich_lr1,
               hyp_last_gich_lr2,
               hyp_last_gich_lr3);
    return RT_EOK;
  }

  rt_kprintf("Usage: hyp el | hvc | custom | gicd | irq\n");

  return -RT_ERROR;
}
MSH_CMD_EXPORT(hyp, minimal hypervisor probes);

static int mpidr(int argc, char **argv)
{
  rt_uint64_t mpidr;

  RT_UNUSED(argc);
  RT_UNUSED(argv);

  rt_hw_sysreg_read(mpidr_el1, mpidr);
  rt_kprintf("mpidr=0x%lx affinity=0x%lx\n", mpidr, mpidr & MPIDR_AFFINITY_MASK);

  return RT_EOK;
}
MSH_CMD_EXPORT(mpidr, show current cpu mpidr);
