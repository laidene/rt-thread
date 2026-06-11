#include <rtthread.h>
#include "../../hyp/hyp_log.h"
#include "../data_abort_helper.h"
#include "drivers/virt.h"
#include "gicd_reg.h"


rt_uint32_t linux_gicd_ispendr0[LINUX_GUEST_VCPU_COUNT]; /* INTID 0..31, banked */
rt_uint32_t linux_gicd_ispendr[GICD_ISPENDR_COUNT - 1U]; /* INTID 32..95, shared */

////////////////////////////////////////////////////////////////////////////////////////////
static rt_uint32_t *linux_gicd_shadow_pending_word(rt_uint64_t offset)
{
    rt_uint32_t index;

    index = (rt_uint32_t)((offset - GICD_ISPENDR_OFFSET_BASE) >> 2);
    if (index >= GICD_ISPENDR_COUNT) {
        return RT_NULL;
    }

    if (index == 0U) {
        return &linux_gicd_ispendr0[linux_get_vcpu_id()];
    }

    return &linux_gicd_ispendr[index - 1U];
}

static void linux_gicd_shadow_update_sgi_bitmap(rt_uint32_t target_cpu, rt_uint32_t bits, rt_bool_t is_set_reg)
{
    rt_uint8_t source_mask = linux_gicd_shadow_bank_target_mask();

    for (rt_uint32_t sgi = 0; sgi < GICD_SGI_INT_COUNT; ++sgi) {
        if ((bits & (1U << sgi)) == 0U) {
            continue;
        }

        if (is_set_reg) {
            linux_gicd_spendsgir[target_cpu][sgi] |= source_mask;
        } else {
            linux_gicd_spendsgir[target_cpu][sgi] = 0U;
        }
    }

    linux_gicd_shadow_sync_sgi_pending(target_cpu);
}
////////////////////////////////////////////////////////////////////////////////////////////


int linux_gicd_shadow_ispendr_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset, rt_bool_t is_set_reg)
{
    rt_uint32_t srt;
    rt_uint32_t index;
    rt_uint32_t value;
    rt_uint32_t *shadow;
    rt_uint32_t target_cpu;
    rt_uint32_t read_value;

    if ((tf->esr & ESR_ISS_SAS_MASK) != ESR_SAS_WORD) {
        return -RT_ERROR;
    }

    index = (rt_uint32_t)((offset - GICD_ISPENDR_OFFSET_BASE) >> 2);
    shadow = linux_gicd_shadow_pending_word(offset);
    if (shadow == RT_NULL) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);

    rt_uint32_t base_intid = index * 32U;
    if ((tf->esr & ESR_ISS_WNR) != 0) {
        value = (rt_uint32_t)linux_stage2_read_guest_reg(tf, srt);

        if (is_set_reg) {
            *shadow |= value;
        } else {
            *shadow &= ~value;
        }

        if (index == 0U) {
            target_cpu = linux_get_vcpu_id();
            linux_gicd_shadow_update_sgi_bitmap(target_cpu, value & 0x0000ffffU, is_set_reg);
        }
        linux_gicd_real_write_w1(
            is_set_reg ? offset : (offset - GICD_ISPENDR_OFFSET_BASE + GICD_ICPENDR_OFFSET_BASE), value, base_intid);

#if 1
        for (rt_uint32_t bit = 0; bit < 32U; ++bit) {
            rt_uint32_t intid = base_intid + bit;

            if ((value & (1U << bit)) == 0U) {
                continue;
            }
            if (linux_gicd_is_open_intid(intid)) {
                hyp_log_printf("vcpu=%u %s int=%u\n",
                               linux_get_vcpu_id(),
                               is_set_reg ? "[gicd_set_pending][w]" : "[gicd_clr_pending][w]",
                               intid);
            }
        }
#endif
    } else {
#if 1
        for (rt_uint32_t bit = 0; bit < 32U; ++bit) {
            rt_uint32_t intid = base_intid + bit;

            if ((*shadow & (1U << bit)) == 0U) {
                continue;
            }
            if (linux_gicd_is_open_intid(intid)) {
                hyp_log_printf("[gicd_state_read] vcpu=%u  int=%u state=%x\n", linux_get_vcpu_id(), intid, *shadow);
            }
        }
#endif
        read_value = linux_gicd_real_merge_word(offset, *shadow, base_intid);
        linux_stage2_write_guest_reg(tf, srt, read_value);
    }

    tf->elr += 4;

    return 0;
}
