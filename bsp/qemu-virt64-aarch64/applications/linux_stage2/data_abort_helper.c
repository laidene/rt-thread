#include "data_abort_helper.h"



rt_uint64_t linux_stage2_fault_ipa(rt_uint64_t far, rt_uint64_t hpfar)
{
    return ((hpfar & 0xfffffffff0UL) << 8) | (far & 0xfffUL);
}


rt_uint32_t linux_stage2_data_abort_srt(rt_uint64_t esr)
{
    return (esr & ESR_ISS_SRT_MASK) >> ESR_ISS_SRT_SHIFT;
}

rt_uint32_t linux_stage2_data_abort_access_size(rt_uint64_t esr)
{
    rt_uint32_t sas;

    sas = (rt_uint32_t)((esr & ESR_ISS_SAS_MASK) >> ESR_ISS_SAS_SHIFT);

    return 1U << sas;
}

rt_uint64_t linux_stage2_read_guest_reg(struct linux_stage2_trap_frame *tf, rt_uint32_t reg)
{
    if (reg >= 31U) {
        return 0;
    }

    return tf->x[reg];
}

void linux_stage2_write_guest_reg(struct linux_stage2_trap_frame *tf, rt_uint32_t reg, rt_uint64_t value)
{
    if (reg >= 31U) {
        return;
    }

    if ((tf->esr & ESR_ISS_SF) == 0) {
        value &= 0xffffffffUL;
    }

    tf->x[reg] = value;
}