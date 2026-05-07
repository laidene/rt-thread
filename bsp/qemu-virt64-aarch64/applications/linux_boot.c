/*
 * Boot the reserved CPU3 into the Linux Image loaded by xqemu.bat.
 */

#include <rtthread.h>
#include <cache.h>
#include <cpuport.h>
#include <hypercall.h>
#include <mm_aspace.h>
#include <mmu.h>
#include <psci.h>

#define LINUX_CPU3_MPIDR        0x3UL
#define LINUX_IMAGE_ENTRY       0x48200000UL
#define LINUX_DTB_ADDRESS       0x4f000000UL

extern void linux_cpu3_trampoline(void);
extern char linux_cpu3_trampoline_end[];

static int linux_boot(int argc, char **argv)
{
    rt_uint32_t ret;
    rt_size_t trampoline_size;
    rt_ubase_t trampoline_pa;

    RT_UNUSED(argc);
    RT_UNUSED(argv);

    trampoline_size = (rt_ubase_t)linux_cpu3_trampoline_end - (rt_ubase_t)linux_cpu3_trampoline;
    trampoline_pa = (rt_ubase_t)rt_kmem_v2p((void *)linux_cpu3_trampoline);

    if ((void *)trampoline_pa == ARCH_MAP_FAILED)
    {
        rt_kprintf("linux_boot: trampoline v2p failed\n");
        return -RT_ERROR;
    }

    rt_hw_cpu_dcache_clean((void *)linux_cpu3_trampoline, trampoline_size);
    rt_hw_icache_invalidate_all();
    rt_hw_dsb();
    rt_hw_isb();

    rt_kprintf("linux_boot: cpu3 mpidr=0x%lx trampoline=0x%lx image=0x%lx dtb=0x%lx\n",
            LINUX_CPU3_MPIDR, trampoline_pa, LINUX_IMAGE_ENTRY, LINUX_DTB_ADDRESS);

    ret = rt_hv_cpu_on(LINUX_CPU3_MPIDR, trampoline_pa, 0);
    rt_kprintf("linux_boot: HYP CPU_ON returned %d\n", (int)ret);

    return ret == PSCI_RET_SUCCESS ? RT_EOK : -RT_ERROR;
}
MSH_CMD_EXPORT(linux_boot, boot Linux on reserved CPU3);
