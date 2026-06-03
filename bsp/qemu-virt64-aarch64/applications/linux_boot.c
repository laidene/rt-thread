#include <rtthread.h>
#include <cache.h>
#include <cpuport.h>
#include <hypercall.h>
#include <mm_aspace.h>
#include <mmu.h>
#include <psci.h>

#define LINUX_CPU_MPIDR   0x2UL
#define LINUX_IMAGE_ENTRY 0x48200000UL
#define LINUX_DTB_ADDRESS 0x4f000000UL

extern void linux_entry(void);
extern char linux_entry_end[];


static int linux_boot(int argc, char **argv)
{
    RT_UNUSED(argc);
    RT_UNUSED(argv);

    rt_uint32_t ret;
    rt_size_t   entry_size;
    rt_ubase_t  entry_pa;


    entry_size = (rt_ubase_t)linux_entry_end - (rt_ubase_t)linux_entry;
    entry_pa   = (rt_ubase_t)linux_entry;

    rt_hw_cpu_dcache_clean((void *)linux_entry, entry_size);
    rt_hw_icache_invalidate_all();
    rt_hw_dsb();
    rt_hw_isb();

    rt_kprintf("linux_boot: cpu mpidr=0x%lx trampoline=0x%lx image=0x%lx dtb=0x%lx\n",
               LINUX_CPU_MPIDR,
               entry_pa,
               LINUX_IMAGE_ENTRY,
               LINUX_DTB_ADDRESS);

    ret = rt_hw_hypercall(HYPERCALL_CPU_ON, LINUX_CPU_MPIDR, entry_pa, 0, 0, 0, 0, 0);

    return ret == PSCI_RET_SUCCESS ? RT_EOK : -RT_ERROR;
}
MSH_CMD_EXPORT(linux_boot, boot Linux on reserved cpu);