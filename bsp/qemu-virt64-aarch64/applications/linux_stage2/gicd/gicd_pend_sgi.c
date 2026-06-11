#include <rtthread.h>
#include "../../hyp/hyp_log.h"
#include "../data_abort_helper.h"
#include "drivers/virt.h"
#include "gicd_reg.h"


rt_uint8_t linux_gicd_spendsgir[LINUX_GUEST_VCPU_COUNT][GICD_SGI_INT_COUNT];


/////////////////////////////////////////////////////////////////////////////////
static rt_uint8_t *linux_gicd_shadow_spendsgir_byte(rt_uint64_t offset)
{
    rt_uint32_t sgi_id;

    sgi_id = (rt_uint32_t)(offset - GICD_SPENDSGIR_OFFSET_BASE);
    if (sgi_id >= GICD_SGI_INT_COUNT) {
        return RT_NULL;
    }

    return &linux_gicd_spendsgir[linux_get_vcpu_id()][sgi_id];
}
/////////////////////////////////////////////////////////////////////////////////


int linux_gicd_shadow_spendsgir_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset, rt_bool_t is_set_reg)
{
    rt_uint32_t srt;
    rt_uint32_t size;
    rt_uint32_t shift;
    rt_uint64_t value;
    rt_uint8_t *byte_ptr;
    rt_uint8_t byte_value;
    rt_uint32_t target_cpu;

    size = linux_stage2_data_abort_access_size(tf->esr);
    if ((size != 1U) && (size != 2U) && (size != 4U)) {
        return -RT_ERROR;
    }

    if (((offset - GICD_SPENDSGIR_OFFSET_BASE) + size) > GICD_SGI_INT_COUNT) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);
    target_cpu = linux_get_vcpu_id();

    if ((tf->esr & ESR_ISS_WNR) != 0) {
        value = linux_stage2_read_guest_reg(tf, srt);

        for (rt_uint32_t i = 0; i < size; ++i) {
            byte_ptr = linux_gicd_shadow_spendsgir_byte(offset + i);
            if (byte_ptr == RT_NULL) {
                return -RT_ERROR;
            }

            shift = i * 8U;
            byte_value = (rt_uint8_t)((value >> shift) & 0xffU) & GICD_TARGET_CPU_MASK;

            if (is_set_reg) {
                *byte_ptr |= byte_value;
            } else {
                *byte_ptr &= (rt_uint8_t)(~byte_value);
            }
        }

        linux_gicd_shadow_sync_sgi_pending(target_cpu);
    } else {
        value = 0;

        for (rt_uint32_t i = 0; i < size; ++i) {
            byte_ptr = linux_gicd_shadow_spendsgir_byte(offset + i);
            if (byte_ptr == RT_NULL) {
                return -RT_ERROR;
            }

            shift = i * 8U;
            value |= ((rt_uint64_t)(*byte_ptr) << shift);
        }

        linux_stage2_write_guest_reg(tf, srt, value);
    }

    tf->elr += 4;

    return 0;
}
