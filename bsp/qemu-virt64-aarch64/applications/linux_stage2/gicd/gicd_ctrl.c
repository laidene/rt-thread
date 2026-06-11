#include <rtthread.h>
#include "../../hyp/hyp_log.h"
#include "../data_abort_helper.h"


rt_uint32_t linux_gicd_ctlr;

int linux_gicd_shadow_ctlr_access(struct linux_stage2_trap_frame *tf)
{
    rt_uint32_t srt;

    if ((tf->esr & ESR_ISS_SAS_MASK) != ESR_SAS_WORD) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);

    if ((tf->esr & ESR_ISS_WNR) != 0) {
        rt_uint64_t value = linux_stage2_read_guest_reg(tf, srt);
        hyp_log_printf("[gicd_ctrl][w] vcpu=%u val=%u\n", linux_get_vcpu_id(), value);

        linux_gicd_ctlr = linux_stage2_read_guest_reg(tf, srt) & 0x3U;
    } else {
        hyp_log_printf("[gicd_ctrl][r] vcpu=%u val=%u\n", linux_get_vcpu_id(), linux_gicd_ctlr);

        linux_stage2_write_guest_reg(tf, srt, linux_gicd_ctlr);
    }

    tf->elr += 4;

    return 0;
}
