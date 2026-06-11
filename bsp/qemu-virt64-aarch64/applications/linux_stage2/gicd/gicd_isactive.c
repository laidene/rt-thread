#include <rtthread.h>
#include "../../hyp/hyp_log.h"
#include "../data_abort_helper.h"
#include "drivers/virt.h"
#include "gicd_reg.h"


rt_uint32_t linux_gicd_iactiver0[LINUX_GUEST_VCPU_COUNT];   /* INTID 0..31, banked */
rt_uint32_t linux_gicd_iactiver[GICD_ISACTIVER_COUNT - 1U]; /* INTID 32..95, shared */



////////////////////////////////////////////////////////////////////////////////////////////////
static rt_uint32_t *linux_gicd_shadow_active_word(rt_uint64_t offset)
{
    rt_uint32_t index;

    index = (rt_uint32_t)((offset - GICD_ISACTIVER_OFFSET_BASE) >> 2);
    if (index >= GICD_ISACTIVER_COUNT) {
        return RT_NULL;
    }

    if (index == 0U) {
        return &linux_gicd_iactiver0[linux_get_vcpu_id()];
    }

    return &linux_gicd_iactiver[index - 1U];
}
////////////////////////////////////////////////////////////////////////////////////////////////



int linux_gicd_shadow_iactiver_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset, rt_bool_t is_set_reg)
{
    rt_uint32_t srt;
    rt_uint32_t value;
    rt_uint32_t *shadow;
    rt_uint32_t index;
    rt_uint32_t base_intid;
    rt_uint32_t read_value;

    if ((tf->esr & ESR_ISS_SAS_MASK) != ESR_SAS_WORD) {
        return -RT_ERROR;
    }

    index = (rt_uint32_t)((offset - GICD_ISACTIVER_OFFSET_BASE) >> 2);
    shadow = linux_gicd_shadow_active_word(offset);
    if (shadow == RT_NULL) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);
    base_intid = index * 32U;

    if ((tf->esr & ESR_ISS_WNR) != 0) {
        value = (rt_uint32_t)linux_stage2_read_guest_reg(tf, srt);

        if (is_set_reg) {
            *shadow |= value;
        } else {
            *shadow &= ~value;
        }
        linux_gicd_real_write_w1(
            is_set_reg ? offset : (offset - GICD_ISACTIVER_OFFSET_BASE + GICD_ICACTIVER_OFFSET_BASE),
            value,
            base_intid);
    } else {
        read_value = linux_gicd_real_merge_word(offset, *shadow, base_intid);
        linux_stage2_write_guest_reg(tf, srt, read_value);
    }

    tf->elr += 4;

    return 0;
}