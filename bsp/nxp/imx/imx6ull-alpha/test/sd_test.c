#include <dfs_fs.h>
#include <rtthread.h>
#include <rtdevice.h>

#ifdef RT_USING_DFS

static int _sd_mount_try(const char *dev_name, const char *fs_type)
{
    if (rt_device_find(dev_name) == RT_NULL) {
        rt_kprintf("device %s not found.\n", dev_name);
        return -1;
    }

    if (dfs_mount(dev_name, "/", fs_type, 0, RT_NULL) == 0) {
        rt_kprintf("mount %s(%s) on / success.\n", dev_name, fs_type);
        return 0;
    }

    rt_kprintf("mount %s(%s) failed, errno=%d.\n", dev_name, fs_type, rt_get_errno());
    return -1;
}

int _sd_init(void)
{
#if defined(RT_USING_SDIO1) || defined(RT_USING_SDIO2)
    rt_thread_mdelay(500);

#if defined(RT_USING_DFS_ELMFAT)
    if (_sd_mount_try("sd0", "elm") == 0) {
        return 0;
    }

    if (_sd_mount_try("sd1", "elm") == 0) {
        return 0;
    }
#endif

    rt_kprintf("sd_init failed.\n");
#if !defined(RT_USING_DFS_ELMFAT)
    rt_kprintf("RT_USING_DFS_ELMFAT is disabled.\n");
    rt_kprintf("Enable it in menuconfig if you need FAT mount.\n");
#endif
    rt_kprintf("Current block partitions can be checked by `list_blk`.\n");
    return -1;
#else
    rt_kprintf("SDIO is not enabled.\n");
    return -1;
#endif
}
MSH_CMD_EXPORT_ALIAS(_sd_init, sd_init, sd card init);


#else

#endif
