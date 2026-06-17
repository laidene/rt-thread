#include <rtthread.h>

#include "linux_stage2/linux_stage2.h"
#include "linux_stage2/linux_vgic.h"
#include "hyp/hyp_log.h"

#define HCR_EL2_VM     (1UL << 0)
#define HCR_EL2_IMO    (1UL << 4)
#define HCR_EL2_RW     (1UL << 31)
#define HCR_EL2_STAGE2 (HCR_EL2_RW | HCR_EL2_IMO | HCR_EL2_VM)


/**
 * Hypervisor Configuration Register
 */
static inline void hyp_write_hcr_el2(rt_uint64_t value)
{
    __asm__ volatile("msr hcr_el2, %0" ::"r"(value) : "memory");
}

/**
 * Architectural Feature Trap Register
 */
static inline void hyp_write_cptr_el2(rt_uint64_t value)
{
    __asm__ volatile("msr cptr_el2, %0" ::"r"(value) : "memory");
}

/**
 * Exception Link Register
 */
static inline void hyp_write_elr_el2(rt_uint64_t value)
{
    __asm__ volatile("msr elr_el2, %0" ::"r"(value) : "memory");
}


/**
 * Counter-timer Virtual Offset Register
 */
static inline void hyp_write_cntvoff_el2(rt_uint64_t value)
{
    __asm__ volatile("msr cntvoff_el2, %0" ::"r"(value) : "memory");
}


/**
 * Counter-timer Hypervisor Control Register
 */
static inline rt_uint64_t hyp_read_cnthctl_el2(void)
{
    rt_uint64_t value;

    __asm__ volatile("mrs %0, cnthctl_el2" : "=r"(value));

    return value;
}

static inline void hyp_write_cnthctl_el2(rt_uint64_t value)
{
    __asm__ volatile("msr cnthctl_el2, %0" ::"r"(value) : "memory");
}



static inline void hyp_isb(void)
{
    __asm__ volatile("isb" ::: "memory");
}




void linux_hyp_enter_el1(rt_uint64_t entry)
{
    hyp_write_hcr_el2(HCR_EL2_RW);
    hyp_write_cptr_el2(0);
    hyp_write_cntvoff_el2(0);
    hyp_write_cnthctl_el2(hyp_read_cnthctl_el2() | 0x3);

    linux_stage2_enable();
    linux_vgic_init_cpu();

    hyp_write_elr_el2(entry);
    hyp_write_hcr_el2(HCR_EL2_STAGE2);
    hyp_isb();
}
