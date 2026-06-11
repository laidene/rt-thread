#include <rtthread.h>
#include "../../hyp/hyp_log.h"
#include "../data_abort_helper.h"
#include "drivers/virt.h"
#include "gicd_reg.h"


rt_uint8_t linux_gicd_itargetsr[GICD_ITARGETSR_SHARED_INT];


////////////////////////////////////////////////////////////////////////////////
static rt_uint8_t *linux_gicd_shadow_target_byte(rt_uint32_t intid)
{
    if (intid < GICD_ITARGETSR_BANKED_INT) {
        return RT_NULL;
    }

    if (intid < GICD_ITARGETSR_INT_COUNT) {
        return &linux_gicd_itargetsr[intid - GICD_ITARGETSR_BANKED_INT];
    }

    return RT_NULL;
}
////////////////////////////////////////////////////////////////////////////////


/**
 * int targets reg
 * 这个寄存器只能用于spi的指定发送
 * 他只能读取spi和ppi来确认自己的掩码
 * 寄存器用来看到中断的目标cpu
 * 对于sgi和ppi linux以为自己是cpu0 实际是cpu2
 * 所以对于读取sgi和ppi我们要给到实际的掩码
 *
 * linux 主核会把所有spi指定到自己的cpu
 */
int linux_gicd_shadow_itargetsr_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset)
{
    rt_uint32_t srt;
    rt_uint32_t size;
    rt_uint32_t intid;
    rt_uint32_t shift;
    rt_uint64_t value;
    rt_uint8_t *byte_ptr;
    rt_uint8_t byte_value;

    size = linux_stage2_data_abort_access_size(tf->esr);
    if ((size != 1U) && (size != 2U) && (size != 4U)) {
        return -RT_ERROR;
    }

    intid = (rt_uint32_t)(offset - GICD_ITARGETSR_OFFSET_BASE); /* 1个字节对应1个中断，所以偏移刚好是中断号 */
    if ((intid + size) > GICD_ITARGETSR_INT_COUNT) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);

    if ((tf->esr & ESR_ISS_WNR) != 0) {
        value = linux_stage2_read_guest_reg(tf, srt);

        for (rt_uint32_t i = 0; i < size; ++i) {
            /* 忽略写入sgi和ppi */
            if ((intid + i) < GICD_ITARGETSR_BANKED_INT) {
                continue;
            }

            byte_ptr = linux_gicd_shadow_target_byte(intid + i);
            if (byte_ptr == RT_NULL) {
                return -RT_ERROR;
            }

            shift = i * 8U;
            byte_value = (rt_uint8_t)((value >> shift) & 0xffU);
            *byte_ptr = byte_value & GICD_TARGET_CPU_MASK;
            if (linux_gicd_is_open_intid(intid + i)) {
                linux_gicd_real_write_byte(GICD_ITARGETSR_OFFSET_BASE + intid + i, *byte_ptr);
            }

#if 1
            if (linux_gicd_is_open_intid(intid + i)) {
                hyp_log_printf("[gicd_itargetsr][w] intid=%u cpu_mask=%x\n",
                               intid + i,
                               byte_value & GICD_TARGET_CPU_MASK);
            }
#endif
        }
    } else {
        value = 0;

        for (rt_uint32_t i = 0; i < size; ++i) {
            shift = i * 8U;

            if ((intid + i) < GICD_ITARGETSR_BANKED_INT) {
                value |= ((rt_uint64_t)linux_gicd_shadow_bank_target_mask() << shift);
#if 1
                if (intid + i == 0) {
                    hyp_log_printf("[gicd_itargetsr][r] vcpu=%u intid=%u cpu_mask=%x\n",
                                   linux_get_vcpu_id(),
                                   intid + i,
                                   linux_gicd_shadow_bank_target_mask() & GICD_TARGET_CPU_MASK);
                }
#endif
                continue;
            }

            byte_ptr = linux_gicd_shadow_target_byte(intid + i);
            if (byte_ptr == RT_NULL) {
                return -RT_ERROR;
            }
            if (linux_gicd_is_open_intid(intid + i)) {
                *byte_ptr = linux_gicd_real_read_byte(GICD_ITARGETSR_OFFSET_BASE + intid + i) & GICD_TARGET_CPU_MASK;
            }

            value |= ((rt_uint64_t)(*byte_ptr) << shift);
        }

        linux_stage2_write_guest_reg(tf, srt, value);
    }

    tf->elr += 4;

    return 0;
}
