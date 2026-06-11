#include <rtthread.h>
#include "../../hyp/hyp_log.h"
#include "../data_abort_helper.h"
#include "drivers/virt.h"
#include "gicd_reg.h"

#define GICD_SGIR_SGIINTID_MASK     0x0fU
#define GICD_SGIR_TARGET_LIST_SHIFT 16
#define GICD_SGIR_TARGET_LIST_MASK  (0xffU << GICD_SGIR_TARGET_LIST_SHIFT)
#define GICD_SGIR_FILTER_SHIFT      24
#define GICD_SGIR_FILTER_MASK       (0x3U << GICD_SGIR_FILTER_SHIFT)
#define GICD_SGIR_FILTER_LIST       0U
#define GICD_SGIR_FILTER_OTHERS     1U
#define GICD_SGIR_FILTER_SELF       2U

///////////////////////////////////////////////////////////////////////////
static rt_uint32_t linux_gicd_shadow_target_to_sgir_list(rt_uint32_t target_mask)
{
    rt_uint32_t list = 0;

    if ((target_mask & GICD_TARGET_CPU2) != 0U) {
        list |= 1U << 2;
    }

    if ((target_mask & GICD_TARGET_CPU3) != 0U) {
        list |= 1U << 3;
    }

    return list;
}


static rt_uint32_t linux_gicd_shadow_target_to_vcpu(rt_uint32_t target)
{
    if (target == GICD_TARGET_CPU2) {
        return 0;
    }

    if (target == GICD_TARGET_CPU3) {
        return 1;
    }

    return LINUX_GUEST_VCPU_COUNT;
}
///////////////////////////////////////////////////////////////////////////

int linux_gicd_shadow_sgir_access(struct linux_stage2_trap_frame *tf)
{
    rt_uint32_t srt;
    rt_uint32_t value;
    rt_uint32_t sgi_id;
    rt_uint32_t target_mask;
    rt_uint32_t filter;
    rt_uint32_t source_mask;

    if ((tf->esr & ESR_ISS_SAS_MASK) != ESR_SAS_WORD) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);

    if ((tf->esr & ESR_ISS_WNR) == 0) {
        linux_stage2_write_guest_reg(tf, srt, 0);
        tf->elr += 4;
        return 0;
    }

    value = (rt_uint32_t)linux_stage2_read_guest_reg(tf, srt);
    sgi_id = value & GICD_SGIR_SGIINTID_MASK;
    filter = (value & GICD_SGIR_FILTER_MASK) >> GICD_SGIR_FILTER_SHIFT;
    source_mask = linux_gicd_shadow_bank_target_mask();

    if (sgi_id >= GICD_SGI_INT_COUNT) {
        return -RT_ERROR;
    }

    switch (filter) {
    case GICD_SGIR_FILTER_LIST:
        target_mask = ((value & GICD_SGIR_TARGET_LIST_MASK) >> GICD_SGIR_TARGET_LIST_SHIFT) & GICD_TARGET_CPU_MASK;
        break;
    case GICD_SGIR_FILTER_OTHERS:
        target_mask = GICD_TARGET_CPU_MASK & ~source_mask;
        break;
    case GICD_SGIR_FILTER_SELF:
        target_mask = source_mask;
        break;
    default:
        target_mask = 0U;
        break;
    }

    if ((linux_gicd_real_base != 0U) && (target_mask != 0U)) {
        rt_uint32_t real_sgir;

        real_sgir = sgi_id | (linux_gicd_shadow_target_to_sgir_list(target_mask) << GICD_SGIR_TARGET_LIST_SHIFT);
        *linux_gicd_real_reg32(GICD_SGIR_OFFSET) = real_sgir;
        __asm__ volatile("dsb sy" ::: "memory");
    }

    for (rt_uint32_t target = GICD_TARGET_CPU2; target <= GICD_TARGET_CPU3; target <<= 1) {
        rt_uint32_t cpu;

        if ((target_mask & target) == 0U) {
            continue;
        }

        cpu = linux_gicd_shadow_target_to_vcpu(target);
        if (cpu >= LINUX_GUEST_VCPU_COUNT) {
            continue;
        }

        linux_gicd_spendsgir[cpu][sgi_id] |= (rt_uint8_t)source_mask;
        linux_gicd_shadow_sync_sgi_pending(cpu);
    }

    tf->elr += 4;

    return 0;
}