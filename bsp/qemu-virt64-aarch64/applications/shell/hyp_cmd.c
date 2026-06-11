#include <rtthread.h>
#include <armv8.h>
#include <hypercall.h>

#include "hyp/hyp_log.h"

#define HYP_CUSTOM_TEST_VALUE 0xdeadcccc

static int hyp(int argc, char **argv)
{
    if (argc < 2) {
        rt_kprintf("Usage: hyp custom | log [clear]\n");
        return -RT_ERROR;
    }

    if (!rt_strcmp(argv[1], "custom")) {
        rt_uint32_t ret;

        ret = rt_hw_hypercall(HYPERCALL_DEBUG_ID, 0x12345678, 1, 2, 3, 4, 5, 6);
        rt_kprintf("custom hvc call: ret=0x%08x\n", ret);

        return ret == HYP_CUSTOM_TEST_VALUE ? RT_EOK : -RT_ERROR;
    }

    if (!rt_strcmp(argv[1], "log")) {
        if ((argc >= 3) && !rt_strcmp(argv[2], "clear")) {
            hyp_log_clear();
            return RT_EOK;
        }

        hyp_log_dump();
        return RT_EOK;
    }

    rt_kprintf("Usage: hyp custom | log [clear]\n");

    return -RT_ERROR;
}
MSH_CMD_EXPORT(hyp, hypervisor cmd);
