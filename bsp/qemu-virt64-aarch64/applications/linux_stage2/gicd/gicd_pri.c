#include <rtthread.h>
#include "../../hyp/hyp_log.h"
#include "../data_abort_helper.h"
#include "drivers/virt.h"
#include "gicd_reg.h"



rt_uint8_t linux_gicd_ipriorityr0[LINUX_GUEST_VCPU_COUNT][GICD_IPRIORITYR_BANKED_INT];
rt_uint8_t linux_gicd_ipriorityr [GICD_IPRIORITYR_SHARED_INT];

/////////////////////////////////////////////////////////////////////////////////
static rt_uint8_t *linux_gicd_shadow_priority_byte(rt_uint32_t intid)
{
    if (intid < GICD_IPRIORITYR_BANKED_INT) {
        return &linux_gicd_ipriorityr0[linux_get_vcpu_id()][intid];
    }

    if (intid < GICD_IPRIORITYR_INT_COUNT) {
        return &linux_gicd_ipriorityr[intid - GICD_IPRIORITYR_BANKED_INT];
    }

    return RT_NULL;
}
///////////////////////////////////////////////////////////////////////////////////




int linux_gicd_shadow_ipriorityr_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset)
{
    rt_uint32_t srt;
    rt_uint32_t size;
    rt_uint32_t intid;
    rt_uint32_t shift;
    rt_uint64_t value;
    rt_uint8_t *byte_ptr;

    size = linux_stage2_data_abort_access_size(tf->esr);
    if ((size != 1U) && (size != 2U) && (size != 4U)) {
        return -RT_ERROR;
    }

    intid = (rt_uint32_t)(offset - GICD_IPRIORITYR_OFFSET_BASE);
    if ((intid + size) > GICD_IPRIORITYR_INT_COUNT) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);

    if ((tf->esr & ESR_ISS_WNR) != 0) {
        value = linux_stage2_read_guest_reg(tf, srt);

        for (rt_uint32_t i = 0; i < size; ++i) {
            byte_ptr = linux_gicd_shadow_priority_byte(intid + i);
            if (byte_ptr == RT_NULL) {
                return -RT_ERROR;
            }

            shift = i * 8U;
            *byte_ptr = (rt_uint8_t)((value >> shift) & 0xffU);
            if (linux_gicd_is_open_intid(intid + i)) {
                linux_gicd_real_write_byte(GICD_IPRIORITYR_OFFSET_BASE + intid + i, *byte_ptr);
            }
#if 1
            if (linux_gicd_is_open_intid(intid + i)) {
                hyp_log_printf("[gicd_set_pri] vcpu=%u int=%u val=%x\n",
                               linux_get_vcpu_id(),
                               intid + i,
                               (rt_uint32_t)((value >> shift) & 0xffU));
            }
#endif
        }

    } else {
        value = 0;

        for (rt_uint32_t i = 0; i < size; ++i) {
            byte_ptr = linux_gicd_shadow_priority_byte(intid + i);
            if (byte_ptr == RT_NULL) {
                return -RT_ERROR;
            }

            shift = i * 8U;
            if (linux_gicd_is_open_intid(intid + i)) {
                *byte_ptr = linux_gicd_real_read_byte(GICD_IPRIORITYR_OFFSET_BASE + intid + i);
            }
            value |= ((rt_uint64_t)(*byte_ptr) << shift);
#if 1
            if (linux_gicd_is_open_intid(intid + i)) {
                hyp_log_printf("[gicd_read_pri] vcpu=%u int=%u val=%x\n", linux_get_vcpu_id(), intid + i, *byte_ptr);
            }
#endif
        }

        linux_stage2_write_guest_reg(tf, srt, value);
    }

    tf->elr += 4;

    return 0;
}
