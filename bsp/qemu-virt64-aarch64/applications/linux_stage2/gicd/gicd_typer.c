#include <rtthread.h>
#include "../../hyp/hyp_log.h"
#include "../data_abort_helper.h"

/*
 * GICD_TYPER base is GICD + 0x4
 * bits [4:0] ITLinesNumber
 * bits [7:5] CPUNumber
 * bit [10] SecurityExtn
 * bits [15:11] LSPI
 */
#define GICD_TYPER_ITLINES(nr_irqs) ((((nr_irqs) / 32U) - 1U) & 0x1fU)
#define GICD_TYPER_CPUS(nr_cpu)     ((((nr_cpu) - 1U) & 0x7U) << 5)
#define GICD_TYPER_SECURITY_EXTN    (1U << 10)


int linux_gicd_shadow_typer_access(struct linux_stage2_trap_frame *tf)
{
    rt_uint32_t srt;
    rt_uint32_t typer;

    if ((tf->esr & ESR_ISS_SAS_MASK) != ESR_SAS_WORD) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);
    typer = GICD_TYPER_ITLINES(96) | GICD_TYPER_CPUS(2) | GICD_TYPER_SECURITY_EXTN;

    if ((tf->esr & ESR_ISS_WNR) == 0) {
        linux_stage2_write_guest_reg(tf, srt, typer);

        hyp_log_printf("[gicd_typer][r] vcpu=%u val=%u\n", linux_get_vcpu_id(), typer);
    }

    tf->elr += 4;

    return 0;
}