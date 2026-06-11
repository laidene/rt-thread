#ifndef __data_abort_helper_h__
#define __data_abort_helper_h__

#include <rtthread.h>

/* 关键词 ISS encoding for an exception from a Data Abort */
#define ESR_ISS_WNR                 (1UL << 6)
#define ESR_ISS_SF                  (1UL << 15)

#define ESR_ISS_SRT_SHIFT           16
#define ESR_ISS_SRT_MASK            (0x1fUL << ESR_ISS_SRT_SHIFT)

#define ESR_ISS_SAS_SHIFT           22
#define ESR_ISS_SAS_MASK            (3UL << ESR_ISS_SAS_SHIFT)
#define ESR_SAS_WORD                (2UL << ESR_ISS_SAS_SHIFT)


struct linux_stage2_trap_frame {
    rt_uint64_t x[31];
    rt_uint64_t esr;
    rt_uint64_t far;
    rt_uint64_t hpfar;
    rt_uint64_t elr;
    rt_uint64_t pad;
};


rt_uint64_t linux_stage2_fault_ipa(rt_uint64_t far, rt_uint64_t hpfar);

rt_uint32_t linux_stage2_data_abort_srt        (rt_uint64_t esr);
rt_uint32_t linux_stage2_data_abort_access_size(rt_uint64_t esr);

rt_uint64_t linux_stage2_read_guest_reg (struct linux_stage2_trap_frame *tf, rt_uint32_t reg);
void        linux_stage2_write_guest_reg(struct linux_stage2_trap_frame *tf, rt_uint32_t reg, rt_uint64_t value);


static inline rt_uint32_t linux_get_vcpu_id(void)
{
    rt_uint64_t mpidr;

    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    mpidr &= 0xffU;

    if (mpidr == 2U) {
        return 0;
    }

    if (mpidr == 3U) {
        return 1;
    }

    return 0;
}

#endif