#ifndef __GICD_CTRL_H__
#define __GICD_CTRL_H__

#include <rtthread.h>
#include "hyp/hyp_log.h"
#include "../data_abort_helper.h"
#include "drivers/virt.h"

#define GICD_CTLR_OFFSET            0x0000UL
#define GICD_TYPER_OFFSET           0x0004UL
#define GICD_IGROUPR_OFFSET_BASE    0x0080UL
#define GICD_ISENABLER_OFFSET_BASE  0x0100UL
#define GICD_ICENABLER_OFFSET_BASE  0x0180UL
#define GICD_ISPENDR_OFFSET_BASE    0x0200UL
#define GICD_ICPENDR_OFFSET_BASE    0x0280UL
#define GICD_ISACTIVER_OFFSET_BASE  0x0300UL
#define GICD_ICACTIVER_OFFSET_BASE  0x0380UL
#define GICD_IPRIORITYR_OFFSET_BASE 0x0400UL
#define GICD_ITARGETSR_OFFSET_BASE  0x0800UL
#define GICD_ICFGR_OFFSET_BASE      0x0c00UL
#define GICD_ICPIDR2_OFFSET         0x0fe8UL
#define GICD_SGIR_OFFSET            0x0f00UL
#define GICD_CPENDSGIR_OFFSET_BASE  0x0f10UL
#define GICD_SPENDSGIR_OFFSET_BASE  0x0f20UL


#define LINUX_GUEST_VCPU_COUNT 2U


#define GICD_IGROUPR_COUNT          ((ARM_GIC_NR_IRQS + 31U) / 32U)
#define GICD_ISENABLER_COUNT        ((ARM_GIC_NR_IRQS + 31U) / 32U)
#define GICD_ISPENDR_COUNT          ((ARM_GIC_NR_IRQS + 31U) / 32U)
#define GICD_ISACTIVER_COUNT        ((ARM_GIC_NR_IRQS + 31U) / 32U)

#define GICD_IPRIORITYR_INT_COUNT   ARM_GIC_NR_IRQS
#define GICD_IPRIORITYR_BANKED_INT  32U
#define GICD_IPRIORITYR_SHARED_INT  (GICD_IPRIORITYR_INT_COUNT - GICD_IPRIORITYR_BANKED_INT)

#define GICD_ITARGETSR_INT_COUNT    ARM_GIC_NR_IRQS
#define GICD_ITARGETSR_BANKED_INT   32U
#define GICD_ITARGETSR_SHARED_INT   (GICD_ITARGETSR_INT_COUNT - GICD_ITARGETSR_BANKED_INT)

#define GICD_TARGET_CPU2            0x04U
#define GICD_TARGET_CPU3            0x08U
#define GICD_TARGET_CPU_MASK        (GICD_TARGET_CPU2 | GICD_TARGET_CPU3)

#define GICD_ICFGR_COUNT        6U
#define GICD_ICFGR_BANKED_WORDS 2U
#define GICD_ICFGR_SHARED_WORDS (GICD_ICFGR_COUNT - GICD_ICFGR_BANKED_WORDS)
#define GICD_ICFGR_EDGE_MASK    0xaaaaaaaaU

#define GICD_SGI_INT_COUNT          16U

extern rt_uint64_t linux_gicd_real_base;

extern rt_uint32_t linux_gicd_ctlr;
extern rt_uint32_t linux_gicd_igroupr       [GICD_IGROUPR_COUNT];

extern rt_uint32_t linux_gicd_isenabler0    [LINUX_GUEST_VCPU_COUNT];    /* INTID 0..31, banked */
extern rt_uint32_t linux_gicd_isenabler     [GICD_ISENABLER_COUNT - 1U]; /* INTID 32..95, shared */

extern rt_uint32_t linux_gicd_ispendr0      [LINUX_GUEST_VCPU_COUNT];    /* INTID 0..31, banked */
extern rt_uint32_t linux_gicd_ispendr       [GICD_ISPENDR_COUNT - 1U];   /* INTID 32..95, shared */

extern rt_uint32_t linux_gicd_iactiver0     [LINUX_GUEST_VCPU_COUNT];    /* INTID 0..31, banked */
extern rt_uint32_t linux_gicd_iactiver      [GICD_ISACTIVER_COUNT - 1U]; /* INTID 32..95, shared */

extern rt_uint8_t  linux_gicd_ipriorityr0   [LINUX_GUEST_VCPU_COUNT][GICD_IPRIORITYR_BANKED_INT];
extern rt_uint8_t  linux_gicd_ipriorityr    [GICD_IPRIORITYR_SHARED_INT];

extern rt_uint8_t  linux_gicd_itargetsr     [GICD_ITARGETSR_SHARED_INT];

extern rt_uint32_t linux_gicd_icfgr0        [LINUX_GUEST_VCPU_COUNT][GICD_ICFGR_BANKED_WORDS];
extern rt_uint32_t linux_gicd_icfgr         [GICD_ICFGR_SHARED_WORDS];

extern rt_uint8_t  linux_gicd_spendsgir     [LINUX_GUEST_VCPU_COUNT][GICD_SGI_INT_COUNT];

void linux_gicd_shadow_init(void);


rt_bool_t    linux_gicd_is_open_intid(rt_uint32_t intid);
rt_uint32_t  linux_gicd_open_intid_word_mask(rt_uint32_t base_intid);
rt_uint32_t  linux_gicd_real_merge_rw_word(rt_uint64_t offset, rt_uint32_t value, rt_uint32_t mask);
rt_uint32_t  linux_gicd_real_merge_word(rt_uint64_t offset, rt_uint32_t shadow, rt_uint32_t base_intid);


static inline  void linux_gicd_shadow_sync_sgi_pending(rt_uint32_t target_cpu)
{
    rt_uint32_t pending_bits = 0;

    for (rt_uint32_t sgi = 0; sgi < GICD_SGI_INT_COUNT; ++sgi) {
        if (linux_gicd_spendsgir[target_cpu][sgi] != 0U) {
            pending_bits |= (1U << sgi);
        }
    }

    linux_gicd_ispendr0[target_cpu] &= ~0x0000ffffU;
    linux_gicd_ispendr0[target_cpu] |= pending_bits;
}


static inline rt_uint8_t linux_gicd_shadow_bank_target_mask(void)
{
    return linux_get_vcpu_id() == 0U ? GICD_TARGET_CPU2 : GICD_TARGET_CPU3;
}


void linux_gicd_real_write_w1(rt_uint64_t offset, rt_uint32_t value, rt_uint32_t base_intid);


static inline volatile rt_uint32_t *linux_gicd_real_reg32(rt_uint64_t offset)
{
    return (volatile rt_uint32_t *)(linux_gicd_real_base + offset);
}


static inline volatile rt_uint8_t *linux_gicd_real_reg8(rt_uint64_t offset)
{
    return (volatile rt_uint8_t *)(linux_gicd_real_base + offset);
}

static inline void linux_gicd_real_write_byte(rt_uint64_t offset, rt_uint8_t value)
{
    if (linux_gicd_real_base == 0U) {
        return;
    }

    *linux_gicd_real_reg8(offset) = value;
    __asm__ volatile("dsb sy" ::: "memory");
}

static inline rt_uint8_t linux_gicd_real_read_byte(rt_uint64_t offset)
{
    if (linux_gicd_real_base == 0U) {
        return 0;
    }

    return *linux_gicd_real_reg8(offset);
}





int linux_gicd_shadow_ctlr_access     (struct linux_stage2_trap_frame *tf);
int linux_gicd_shadow_typer_access    (struct linux_stage2_trap_frame *tf);

int linux_gicd_shadow_igroupr_access  (struct linux_stage2_trap_frame *tf, rt_uint64_t offset);

int linux_gicd_shadow_isenabler_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset, rt_bool_t is_set_reg);
int linux_gicd_shadow_ispendr_access  (struct linux_stage2_trap_frame *tf, rt_uint64_t offset, rt_bool_t is_set_reg);
int linux_gicd_shadow_iactiver_access (struct linux_stage2_trap_frame *tf, rt_uint64_t offset, rt_bool_t is_set_reg);

int linux_gicd_shadow_ipriorityr_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset);

int linux_gicd_shadow_itargetsr_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset);

int linux_gicd_shadow_icfgr_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset);

int linux_gicd_shadow_sgir_access(struct linux_stage2_trap_frame *tf);

int linux_gicd_shadow_spendsgir_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset, rt_bool_t is_set_reg);

#endif
