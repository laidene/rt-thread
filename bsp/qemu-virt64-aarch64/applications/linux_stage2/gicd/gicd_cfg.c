#include <rtthread.h>
#include "../../hyp/hyp_log.h"
#include "../data_abort_helper.h"
#include "drivers/virt.h"
#include "gicd_reg.h"


rt_uint32_t linux_gicd_icfgr0[LINUX_GUEST_VCPU_COUNT][GICD_ICFGR_BANKED_WORDS];
rt_uint32_t linux_gicd_icfgr[GICD_ICFGR_SHARED_WORDS];


/////////////////////////////////////////////////////////////////////////
static rt_uint32_t *linux_gicd_shadow_icfgr_word(rt_uint64_t offset)
{
    rt_uint32_t index;

    index = (rt_uint32_t)((offset - GICD_ICFGR_OFFSET_BASE) >> 2);
    if (index >= GICD_ICFGR_COUNT) {
        return RT_NULL;
    }

    if (index < GICD_ICFGR_BANKED_WORDS) {
        return &linux_gicd_icfgr0[linux_get_vcpu_id()][index];
    }

    return &linux_gicd_icfgr[index - GICD_ICFGR_BANKED_WORDS];
}

static rt_uint32_t linux_gicd_open_icfgr_mask(rt_uint32_t first_intid, rt_uint32_t count)
{
    rt_uint32_t mask = 0;

    for (rt_uint32_t i = 0; i < count; ++i) {
        rt_uint32_t intid = first_intid + i;

        if (linux_gicd_is_open_intid(intid)) {
            mask |= 0x3U << ((intid & 0xfU) * 2U);
        }
    }

    return mask;
}

static rt_uint32_t linux_gicd_shadow_icfgr_byte_mask(rt_uint64_t offset, rt_uint32_t size)
{
    rt_uint32_t byte_shift;
    rt_uint32_t mask;

    byte_shift = (rt_uint32_t)(offset & 0x3U) * 8U;

    switch (size) {
    case 1U:
        mask = 0x000000ffU;
        break;
    case 2U:
        mask = 0x0000ffffU;
        break;
    case 4U:
        mask = 0xffffffffU;
        break;
    default:
        return 0U;
    }

    mask <<= byte_shift;

    return mask;
}

////////////////////////////////////////////////////////////////////////


int linux_gicd_shadow_icfgr_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset)
{
    rt_uint32_t srt;
    rt_uint32_t size;
    rt_uint32_t *shadow;
    rt_uint32_t byte_shift;
    rt_uint32_t field_mask;
    rt_uint32_t write_mask;
    rt_uint32_t value32;
    rt_uint32_t read_mask;
    rt_uint32_t reg_index;
    rt_uint32_t first_intid;
    rt_uint32_t access_int_count;
    rt_uint32_t intid;
    rt_uint32_t cfg_shift;
    rt_uint32_t cfg_value;
    rt_uint32_t open_cfg_mask;
    rt_uint64_t read_value;

    size = linux_stage2_data_abort_access_size(tf->esr);
    if ((size != 1U) && (size != 2U) && (size != 4U)) {
        return -RT_ERROR;
    }

    if (((offset - GICD_ICFGR_OFFSET_BASE) + size) > (GICD_ICFGR_COUNT * 4U)) {
        return -RT_ERROR;
    }

    if (((offset & 0x3U) + size) > 4U) {
        return -RT_ERROR;
    }

    shadow = linux_gicd_shadow_icfgr_word(offset);
    if (shadow == RT_NULL) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);
    byte_shift = (rt_uint32_t)(offset & 0x3U) * 8U;
    reg_index = (rt_uint32_t)((offset - GICD_ICFGR_OFFSET_BASE) >> 2);
    first_intid = reg_index * 16U + (rt_uint32_t)(offset & 0x3U) * 4U;
    access_int_count = size * 4U;
    open_cfg_mask = linux_gicd_open_icfgr_mask(first_intid, access_int_count);

    if ((tf->esr & ESR_ISS_WNR) != 0) {
        if (reg_index == 0U) {
            tf->elr += 4;
            return 0;
        }

        value32 = (rt_uint32_t)linux_stage2_read_guest_reg(tf, srt);
        value32 <<= byte_shift;

        write_mask = linux_gicd_shadow_icfgr_byte_mask(offset, size);
        field_mask = write_mask & GICD_ICFGR_EDGE_MASK;

        *shadow &= ~write_mask;
        *shadow |= (value32 & field_mask);
        linux_gicd_real_merge_rw_word(GICD_ICFGR_OFFSET_BASE + reg_index * 4U, *shadow, open_cfg_mask);
#if 1
        for (rt_uint32_t i = 0; i < access_int_count; ++i) {
            intid = first_intid + i;
            if (linux_gicd_is_open_intid(intid)) {
                cfg_shift = ((intid & 0xfU) * 2U) + 1U;
                cfg_value = (*shadow >> cfg_shift) & 0x1U;
                hyp_log_printf("[gicd_cfg][w] intid=%u val=%u\n", intid, cfg_value);
            }
        }
#endif
    } else {
        if (open_cfg_mask != 0U) {
            *shadow &= ~open_cfg_mask;
            if (linux_gicd_real_base != 0U) {
                *shadow |= *linux_gicd_real_reg32(GICD_ICFGR_OFFSET_BASE + reg_index * 4U) & open_cfg_mask;
            }
        }
        read_mask = linux_gicd_shadow_icfgr_byte_mask(0, size);
        read_value = (rt_uint64_t)((*shadow >> byte_shift) & read_mask);
        linux_stage2_write_guest_reg(tf, srt, read_value);

#if 1
        for (rt_uint32_t i = 0; i < access_int_count; ++i) {
            intid = first_intid + i;
            if (linux_gicd_is_open_intid(intid)) {
                cfg_shift = ((intid & 0xfU) * 2U) + 1U;
                cfg_value = (*shadow >> cfg_shift) & 0x1U;
                hyp_log_printf("[gicd_cfg][r] intid=%u val=%u\n", intid, cfg_value);
            }
        }
#endif
    }

    tf->elr += 4;

    return 0;
}
