#include <rtthread.h>
#include "../../hyp/hyp_log.h"
#include "../data_abort_helper.h"
#include "drivers/virt.h"
#include "gicd_reg.h"

rt_uint32_t linux_gicd_isenabler0[LINUX_GUEST_VCPU_COUNT];   /* INTID 0..31, banked */
rt_uint32_t linux_gicd_isenabler[GICD_ISENABLER_COUNT - 1U]; /* INTID 32..95, shared */

static rt_uint32_t *linux_gicd_shadow_enable_word(rt_uint64_t offset)
{
    rt_uint32_t index;

    index = (rt_uint32_t)((offset - GICD_ISENABLER_OFFSET_BASE) >> 2);
    if (index >= GICD_ISENABLER_COUNT) {
        return RT_NULL;
    }

    if (index == 0U) {
        return &linux_gicd_isenabler0[linux_get_vcpu_id()];
    }

    return &linux_gicd_isenabler[index - 1U];
}


int linux_gicd_shadow_isenabler_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset, rt_bool_t is_set_reg)
{
    rt_uint32_t srt;
    rt_uint32_t value;
    rt_uint32_t *shadow;
    rt_uint32_t base_int_id;
    rt_uint32_t read_value;
    base_int_id = (offset - GICD_ISENABLER_OFFSET_BASE) * 8;

    if ((tf->esr & ESR_ISS_SAS_MASK) != ESR_SAS_WORD) {
        return -RT_ERROR;
    }

    shadow = linux_gicd_shadow_enable_word(offset);
    if (shadow == RT_NULL) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);

    if ((tf->esr & ESR_ISS_WNR) != 0) {
        value = (rt_uint32_t)linux_stage2_read_guest_reg(tf, srt);

        if (is_set_reg) {
            *shadow |= value;
        } else {
            *shadow &= ~value;
        }

        /* 写入真实gicd */
        linux_gicd_real_write_w1(
            is_set_reg ? offset : (offset - GICD_ISENABLER_OFFSET_BASE + GICD_ICENABLER_OFFSET_BASE),
            value,
            base_int_id);
#if 1
        for (rt_uint32_t bit = 0; bit < 32U; ++bit) {
            if ((value & (1U << bit)) == 0U) {
                continue;
            }
            if (linux_gicd_is_open_intid(base_int_id + bit)) {
                if (is_set_reg) {
                    hyp_log_printf(
                        "[gicd_enable] vcpu=%u int=%u\n", linux_get_vcpu_id(), base_int_id + bit);

                } else {
                    hyp_log_printf(
                        "[gicd_disable] vcpu=%u int=%u\n", linux_get_vcpu_id(), base_int_id + bit);
                }
            }
        }
#endif
    } else {
#if 1
        for (rt_uint32_t bit = 0; bit < 32U; ++bit) {
            if ((*shadow & (1U << bit)) == 0U) {
                continue;
            }
            if (linux_gicd_is_open_intid(base_int_id + bit)) {
                hyp_log_printf("[gicd_enable] vcpu=%u int=%u val=%u\n", linux_get_vcpu_id(),base_int_id + bit, *shadow);
            }
        }
#endif
        read_value = linux_gicd_real_merge_word(offset, *shadow, base_int_id);
        linux_stage2_write_guest_reg(tf, srt, read_value);
    }

    tf->elr += 4;

    return 0;
}