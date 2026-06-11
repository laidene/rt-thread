#include <rtthread.h>
#include "linux_gicd_shadow.h"
#include "hyp/hyp_log.h"
#include "data_abort_helper.h"
#include "gicd/gicd_reg.h"


#define GICD_REAL_BASE     0x08000000UL

#define GICD_ICPIDR2_VALUE 0x20U


static int linux_gicd_shadow_icpidr2_access(struct linux_stage2_trap_frame *tf)
{
    rt_uint32_t srt;

    if ((tf->esr & ESR_ISS_SAS_MASK) != ESR_SAS_WORD) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);

    if ((tf->esr & ESR_ISS_WNR) == 0) {
        linux_stage2_write_guest_reg(tf, srt, GICD_ICPIDR2_VALUE);
    }

    tf->elr += 4;

    return 0;
}


/* todo */
void linux_gicd_shadow_prepare(void)
{
    if (linux_gicd_real_base == 0U) {
        linux_gicd_real_base = GICD_REAL_BASE;
    }
}


int linux_gicd_shadow_abort(struct linux_stage2_trap_frame *tf)
{
    rt_uint64_t ipa = linux_stage2_fault_ipa(tf->far, tf->hpfar);
    rt_uint64_t offset = ipa - 0x08000000UL;

    if (offset == GICD_CTLR_OFFSET) {
        return linux_gicd_shadow_ctlr_access(tf);
    }

    if (offset == GICD_TYPER_OFFSET) {
        return linux_gicd_shadow_typer_access(tf);
    }

    if ((offset >= GICD_IGROUPR_OFFSET_BASE) && (offset < (GICD_IGROUPR_OFFSET_BASE + GICD_IGROUPR_COUNT * 4U))) {
        return linux_gicd_shadow_igroupr_access(tf, offset);
    }

    if ((offset >= GICD_ISENABLER_OFFSET_BASE) && (offset < (GICD_ISENABLER_OFFSET_BASE + GICD_ISENABLER_COUNT * 4U))) {
        return linux_gicd_shadow_isenabler_access(tf, offset, RT_TRUE);
    }

    if ((offset >= GICD_ICENABLER_OFFSET_BASE) && (offset < (GICD_ICENABLER_OFFSET_BASE + GICD_ISENABLER_COUNT * 4U))) {
        return linux_gicd_shadow_isenabler_access(
            tf, offset - GICD_ICENABLER_OFFSET_BASE + GICD_ISENABLER_OFFSET_BASE, RT_FALSE);
    }

    if ((offset >= GICD_ISPENDR_OFFSET_BASE) && (offset < (GICD_ISPENDR_OFFSET_BASE + GICD_ISPENDR_COUNT * 4U))) {
        return linux_gicd_shadow_ispendr_access(tf, offset, RT_TRUE);
    }

    if ((offset >= GICD_ICPENDR_OFFSET_BASE) && (offset < (GICD_ICPENDR_OFFSET_BASE + GICD_ISPENDR_COUNT * 4U))) {
        return linux_gicd_shadow_ispendr_access(
            tf, offset - GICD_ICPENDR_OFFSET_BASE + GICD_ISPENDR_OFFSET_BASE, RT_FALSE);
    }

    if ((offset >= GICD_ISACTIVER_OFFSET_BASE) && (offset < (GICD_ISACTIVER_OFFSET_BASE + GICD_ISACTIVER_COUNT * 4U))) {
        return linux_gicd_shadow_iactiver_access(tf, offset, RT_TRUE);
    }

    if ((offset >= GICD_ICACTIVER_OFFSET_BASE) && (offset < (GICD_ICACTIVER_OFFSET_BASE + GICD_ISACTIVER_COUNT * 4U))) {
        return linux_gicd_shadow_iactiver_access(
            tf, offset - GICD_ICACTIVER_OFFSET_BASE + GICD_ISACTIVER_OFFSET_BASE, RT_FALSE);
    }

    if ((offset >= GICD_IPRIORITYR_OFFSET_BASE) &&
        (offset < (GICD_IPRIORITYR_OFFSET_BASE + GICD_IPRIORITYR_INT_COUNT))) {
        return linux_gicd_shadow_ipriorityr_access(tf, offset);
    }

    if ((offset >= GICD_ITARGETSR_OFFSET_BASE) && (offset < (GICD_ITARGETSR_OFFSET_BASE + GICD_ITARGETSR_INT_COUNT))) {
        return linux_gicd_shadow_itargetsr_access(tf, offset);
    }

    if ((offset >= GICD_ICFGR_OFFSET_BASE) && (offset < (GICD_ICFGR_OFFSET_BASE + GICD_ICFGR_COUNT * 4U))) {
        return linux_gicd_shadow_icfgr_access(tf, offset);
    }

    if (offset == GICD_SGIR_OFFSET) {
        return linux_gicd_shadow_sgir_access(tf);
    }

    if ((offset >= GICD_CPENDSGIR_OFFSET_BASE) && (offset < (GICD_CPENDSGIR_OFFSET_BASE + GICD_SGI_INT_COUNT))) {
        return linux_gicd_shadow_spendsgir_access(
            tf, offset - GICD_CPENDSGIR_OFFSET_BASE + GICD_SPENDSGIR_OFFSET_BASE, RT_FALSE);
    }

    if ((offset >= GICD_SPENDSGIR_OFFSET_BASE) && (offset < (GICD_SPENDSGIR_OFFSET_BASE + GICD_SGI_INT_COUNT))) {
        return linux_gicd_shadow_spendsgir_access(tf, offset, RT_TRUE);
    }

    if (offset == GICD_ICPIDR2_OFFSET) {
        return linux_gicd_shadow_icpidr2_access(tf);
    }
    return -RT_ERROR;
}
