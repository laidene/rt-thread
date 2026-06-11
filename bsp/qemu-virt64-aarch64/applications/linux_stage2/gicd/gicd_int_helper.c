#include <rtthread.h>
#include "../../hyp/hyp_log.h"
#include "../data_abort_helper.h"
#include "drivers/virt.h"
#include "gicd_reg.h"

#define GICD_IPRIORITYR_INIT_VALUE  0xa0U
#define GICD_ITARGETSR_INIT_VALUE   0x04U


rt_uint64_t linux_gicd_real_base;

static const rt_uint32_t linux_gicd_open_intids[] = {
    0x30U,
    0x31U,
};


rt_bool_t linux_gicd_is_open_intid(rt_uint32_t intid)
{
    for (rt_uint32_t i = 0; i < sizeof(linux_gicd_open_intids) / sizeof(linux_gicd_open_intids[0]); ++i) {
        if (linux_gicd_open_intids[i] == intid) {
            return RT_TRUE;
        }
    }

    return RT_FALSE;
}


/* 拿到白名单中能够影响的位 */
rt_uint32_t linux_gicd_open_intid_word_mask(rt_uint32_t base_intid)
{
    rt_uint32_t mask = 0;

    for (rt_uint32_t i = 0; i < sizeof(linux_gicd_open_intids) / sizeof(linux_gicd_open_intids[0]); ++i) {
        rt_uint32_t intid = linux_gicd_open_intids[i];

        if ((intid >= base_intid) && (intid < (base_intid + 32U))) {
            mask |= 1U << (intid - base_intid);
        }
    }

    return mask;
}


/**
 *  写入真实gicd
 * offset 字节偏移
 * value  要写入的值
 * mask   能够写入的掩码
 */
rt_uint32_t linux_gicd_real_merge_rw_word(rt_uint64_t offset, rt_uint32_t value, rt_uint32_t mask)
{
    rt_uint32_t real;

    if (linux_gicd_real_base == 0U) {
        return value;
    }

    if (mask == 0U) {
        return value;
    }

    real = *linux_gicd_real_reg32(offset);
    real &= ~mask;
    real |= value & mask;
    *linux_gicd_real_reg32(offset) = real;
    __asm__ volatile("dsb sy" ::: "memory");

    return real;
}

/**
 * 读取真实gicd，返回值只呈现白名单的状态 非白名单的为shadow值
 */
rt_uint32_t linux_gicd_real_merge_word(rt_uint64_t offset, rt_uint32_t shadow, rt_uint32_t base_intid)
{
    rt_uint32_t mask = linux_gicd_open_intid_word_mask(base_intid);
    rt_uint32_t real;

    if (linux_gicd_real_base == 0U) {
        return shadow;
    }

    if (mask == 0U) {
        return shadow;
    }

    real = *linux_gicd_real_reg32(offset);

    return (shadow & ~mask) | (real & mask);
}


/* 直接写入真实gicd 非白名单写入0，用于写1生效写0无效的寄存器 */
void linux_gicd_real_write_w1(rt_uint64_t offset, rt_uint32_t value, rt_uint32_t base_intid)
{
    rt_uint32_t mask = linux_gicd_open_intid_word_mask(base_intid);

    if (linux_gicd_real_base == 0U) {
        return;
    }

    value &= mask;
    if (value == 0U) {
        return;
    }

    *linux_gicd_real_reg32(offset) = value;
    __asm__ volatile("dsb sy" ::: "memory");
}


void linux_gicd_shadow_init(void)
{
    rt_uint32_t i;

    linux_gicd_ctlr = 0;

    for (int i = 0; i < GICD_IGROUPR_COUNT; ++i) {
        linux_gicd_igroupr[i] = 0xffffffffU;
    }

    for (int i = 0; i < LINUX_GUEST_VCPU_COUNT; ++i) {
        linux_gicd_isenabler0[i] = 0x00000000U;
        linux_gicd_ispendr0[i]   = 0x00000000U;
        linux_gicd_iactiver0[i]  = 0x00000000U;
    }

    for (int i = 0; i < (GICD_ISENABLER_COUNT - 1U); ++i) {
        linux_gicd_isenabler[i] = 0x00000000U;
    }

    for (i = 0; i < (GICD_ISPENDR_COUNT - 1U); ++i) {
        linux_gicd_ispendr[i] = 0x00000000U;
    }

    for (i = 0; i < (GICD_ISACTIVER_COUNT - 1U); ++i) {
        linux_gicd_iactiver[i] = 0x00000000U;
    }

    for (i = 0; i < LINUX_GUEST_VCPU_COUNT; ++i) {
        for (rt_uint32_t j = 0; j < GICD_IPRIORITYR_BANKED_INT; ++j) {
            linux_gicd_ipriorityr0[i][j] = GICD_IPRIORITYR_INIT_VALUE;
        }
    }

    for (i = 0; i < GICD_IPRIORITYR_SHARED_INT; ++i) {
        linux_gicd_ipriorityr[i] = GICD_IPRIORITYR_INIT_VALUE;
    }

    for (i = 0; i < GICD_ITARGETSR_SHARED_INT; ++i) {
        linux_gicd_itargetsr[i] = GICD_ITARGETSR_INIT_VALUE;
    }

    for (i = 0; i < LINUX_GUEST_VCPU_COUNT; ++i) {
        linux_gicd_icfgr0[i][0] = GICD_ICFGR_EDGE_MASK;
        linux_gicd_icfgr0[i][1] = 0x00000000U;
    }

    for (i = 0; i < GICD_ICFGR_SHARED_WORDS; ++i) {
        linux_gicd_icfgr[i] = 0x00000000U;
    }

    for (i = 0; i < LINUX_GUEST_VCPU_COUNT; ++i) {
        for (rt_uint32_t sgi = 0; sgi < GICD_SGI_INT_COUNT; ++sgi) {
            linux_gicd_spendsgir[i][sgi] = 0x00U;
        }
    }
}
