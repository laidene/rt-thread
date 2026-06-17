#include <rtthread.h>
#include "../../hyp/hyp_log.h"
#include "../data_abort_helper.h"
#include "../linux_vgic.h"
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

static void linux_gicd_log_real_intid_state(rt_uint32_t intid)
{
    rt_uint32_t word_index;
    rt_uint32_t bit;
    rt_uint32_t cfg_index;
    rt_uint32_t cfg_shift;
    rt_uint32_t gicd_ctlr;
    rt_uint32_t group;
    rt_uint32_t enable;
    rt_uint32_t pending;
    rt_uint32_t cfg;
    rt_uint32_t cfg_value;
    rt_uint8_t target;

    if (linux_gicd_real_base == 0U) {
        return;
    }

    word_index = intid >> 5;
    bit = intid & 0x1fU;
    cfg_index = intid >> 4;
    cfg_shift = ((intid & 0xfU) * 2U) + 1U;

    enable = *linux_gicd_real_reg32(GICD_ISENABLER_OFFSET_BASE + word_index * 4U);
    pending = *linux_gicd_real_reg32(GICD_ISPENDR_OFFSET_BASE + word_index * 4U);
    group = *linux_gicd_real_reg32(GICD_IGROUPR_OFFSET_BASE + word_index * 4U);
    gicd_ctlr = *linux_gicd_real_reg32(GICD_CTLR_OFFSET);
    target = linux_gicd_real_read_byte(GICD_ITARGETSR_OFFSET_BASE + intid);
    cfg = *linux_gicd_real_reg32(GICD_ICFGR_OFFSET_BASE + cfg_index * 4U);
    cfg_value = (cfg >> cfg_shift) & 0x1U;

    hyp_log_printf("[gicd_real] int=%u en=%u pend=%u group=%u target=%x cfg=%u ctlr=%x raw_en=%x raw_pend=%x raw_cfg=%x\n",
                   intid,
                   (enable >> bit) & 0x1U,
                   (pending >> bit) & 0x1U,
                   (group >> bit) & 0x1U,
                   target,
                   cfg_value,
                   gicd_ctlr,
                   enable,
                   pending,
                   cfg);
}


int linux_gicd_shadow_isenabler_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset, rt_bool_t is_set_reg)
{
    rt_uint32_t srt;
    rt_uint32_t index;
    rt_uint32_t value;
    rt_uint32_t *shadow;
    rt_uint32_t base_int_id;
    rt_uint32_t read_value;
    index = (rt_uint32_t)((offset - GICD_ISENABLER_OFFSET_BASE) >> 2);
    base_int_id = index * 32U;

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
                    linux_gicd_log_real_intid_state(base_int_id + bit);
                    linux_vgic_log_cpuif_state("after-enable");

                } else {
                    hyp_log_printf(
                        "[gicd_disable] vcpu=%u int=%u\n", linux_get_vcpu_id(), base_int_id + bit);
                    linux_gicd_log_real_intid_state(base_int_id + bit);
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
