#include <rtthread.h>
#include "../../hyp/hyp_log.h"
#include "../data_abort_helper.h"
#include "drivers/virt.h"
#include "gicd_reg.h"

/* bsp/qemu-virt64-aarch64/drivers/virt.h ARM_GIC_NR_IRQS */


rt_uint32_t linux_gicd_igroupr[GICD_IGROUPR_COUNT];

#define LINUX_GICD_OPEN_INTID0 0x30U
#define LINUX_GICD_OPEN_INTID1 0x31U




int linux_gicd_shadow_igroupr_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset)
{
    rt_uint32_t srt;
    rt_uint32_t index;
    rt_uint32_t value;
    rt_uint32_t base_intid;
    rt_uint32_t open_mask;

    if ((tf->esr & ESR_ISS_SAS_MASK) != ESR_SAS_WORD) {
        return -RT_ERROR;
    }

    index = (rt_uint32_t)((offset - GICD_IGROUPR_OFFSET_BASE) >> 2);
    if (index >= GICD_IGROUPR_COUNT) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);
    base_intid = index * 32U;
    open_mask = linux_gicd_open_intid_word_mask(base_intid);

    if ((tf->esr & ESR_ISS_WNR) != 0) {
        value = (rt_uint32_t)linux_stage2_read_guest_reg(tf, srt);
        linux_gicd_igroupr[index] = value;
        linux_gicd_real_merge_rw_word(offset, value, open_mask);
    } else {
        value = linux_gicd_real_merge_word(offset, linux_gicd_igroupr[index], base_intid);
        linux_stage2_write_guest_reg(tf, srt, value);
    }

    tf->elr += 4;

    return 0;
}
