#include <rtthread.h>

#include "linux_gicd_shadow.h"
#include "hyp_log.h"

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
#define GICD_ICPIDR2_VALUE          0x20U

/*
 * GICD_IGROUPRn:
 * - base is GICD + 0x080
 * - each n is one 32-bit register
 * - each bit maps to one interrupt ID
 * - bit = 1 means Group 1
 * - bit = 0 means Group 0
 */
#define GICD_IGROUPR_COUNT 3U

/*
 * GICD_ISENABLERn / GICD_ICENABLERn:
 * - each bit maps to one interrupt ID
 * - ISENABLER write 1 sets enable
 * - ICENABLER write 1 clears enable
 */
#define GICD_ISENABLER_COUNT   3U
#define LINUX_GUEST_VCPU_COUNT 2U

/*
 * GICD_ISPENDRn / GICD_ICPENDRn:
 * - each bit maps to one interrupt ID
 * - ISPENDR write 1 sets pending
 * - ICPENDR write 1 clears pending
 */
#define GICD_ISPENDR_COUNT 3U

/*
 * GICD_ISACTIVERn / GICD_ICACTIVERn:
 * - each bit maps to one interrupt ID
 * - ISACTIVER write 1 sets active
 * - ICACTIVER write 1 clears active
 */
#define GICD_ISACTIVER_COUNT 3U

/*
 * GICD_IPRIORITYRn:
 * - base is GICD + 0x400
 * - each register contains four 8-bit priority fields
 */
#define GICD_IPRIORITYR_INT_COUNT  96U
#define GICD_IPRIORITYR_BANKED_INT 32U
#define GICD_IPRIORITYR_SHARED_INT (GICD_IPRIORITYR_INT_COUNT - GICD_IPRIORITYR_BANKED_INT)
#define GICD_IPRIORITYR_INIT_VALUE 0xa0U

/*
 * GICD_ITARGETSRn:
 * - base is GICD + 0x800
 * - each register contains four 8-bit CPU target fields
 */
#define GICD_ITARGETSR_INT_COUNT  96U
#define GICD_ITARGETSR_BANKED_INT 32U
#define GICD_ITARGETSR_SHARED_INT (GICD_ITARGETSR_INT_COUNT - GICD_ITARGETSR_BANKED_INT)
#define GICD_ITARGETSR_INIT_VALUE 0x04U
#define GICD_TARGET_CPU2          0x04U
#define GICD_TARGET_CPU3          0x08U
#define GICD_TARGET_CPU_MASK      (GICD_TARGET_CPU2 | GICD_TARGET_CPU3)

/*
 * GICD_ICFGRn:
 * - base is GICD + 0xc00
 * - each register describes 16 interrupts
 * - each interrupt uses 2 bits
 * - 00 means level-sensitive
 * - 10 means edge-triggered
 */
#define GICD_ICFGR_COUNT        6U
#define GICD_ICFGR_BANKED_WORDS 2U
#define GICD_ICFGR_SHARED_WORDS (GICD_ICFGR_COUNT - GICD_ICFGR_BANKED_WORDS)
#define GICD_ICFGR_EDGE_MASK    0xaaaaaaaaU

/*
 * GICD_SGIR:
 * bits [3:0]   SGIINTID
 * bits [23:16] CPUTargetList
 * bits [25:24] TargetListFilter
 */
#define GICD_SGI_INT_COUNT          16U
#define GICD_SGIR_REG_COUNT         4U
#define GICD_SGIR_SGIINTID_MASK     0x0fU
#define GICD_SGIR_TARGET_LIST_SHIFT 16
#define GICD_SGIR_TARGET_LIST_MASK  (0xffU << GICD_SGIR_TARGET_LIST_SHIFT)
#define GICD_SGIR_FILTER_SHIFT      24
#define GICD_SGIR_FILTER_MASK       (0x3U << GICD_SGIR_FILTER_SHIFT)
#define GICD_SGIR_FILTER_LIST       0U
#define GICD_SGIR_FILTER_OTHERS     1U
#define GICD_SGIR_FILTER_SELF       2U

#define ESR_ISS_WNR                 (1UL << 6)
#define ESR_ISS_SAS_SHIFT           22
#define ESR_ISS_SAS_MASK            (3UL << ESR_ISS_SAS_SHIFT)
#define ESR_ISS_SRT_SHIFT           16
#define ESR_ISS_SRT_MASK            (0x1fUL << ESR_ISS_SRT_SHIFT)
#define ESR_ISS_SF                  (1UL << 15)
#define ESR_SAS_WORD                (2UL << ESR_ISS_SAS_SHIFT)

static rt_uint32_t linux_gicd_ctlr; /* GICD_CTLR - Distributor Control Register */
static rt_uint32_t linux_gicd_igroupr[GICD_IGROUPR_COUNT];
static rt_uint32_t linux_gicd_isenabler0[LINUX_GUEST_VCPU_COUNT];   /* INTID 0..31, banked */
static rt_uint32_t linux_gicd_isenabler[GICD_ISENABLER_COUNT - 1U]; /* INTID 32..95, shared */
static rt_uint32_t linux_gicd_ispendr0[LINUX_GUEST_VCPU_COUNT];     /* INTID 0..31, banked */
static rt_uint32_t linux_gicd_ispendr[GICD_ISPENDR_COUNT - 1U];     /* INTID 32..95, shared */
static rt_uint32_t linux_gicd_iactiver0[LINUX_GUEST_VCPU_COUNT];    /* INTID 0..31, banked */
static rt_uint32_t linux_gicd_iactiver[GICD_ISACTIVER_COUNT - 1U];  /* INTID 32..95, shared */
static rt_uint8_t linux_gicd_ipriorityr0[LINUX_GUEST_VCPU_COUNT][GICD_IPRIORITYR_BANKED_INT];
static rt_uint8_t linux_gicd_ipriorityr[GICD_IPRIORITYR_SHARED_INT];
static rt_uint8_t linux_gicd_itargetsr[GICD_ITARGETSR_SHARED_INT];
static rt_uint32_t linux_gicd_icfgr0[LINUX_GUEST_VCPU_COUNT][GICD_ICFGR_BANKED_WORDS];
static rt_uint32_t linux_gicd_icfgr[GICD_ICFGR_SHARED_WORDS];
static rt_uint8_t linux_gicd_spendsgir[LINUX_GUEST_VCPU_COUNT][GICD_SGI_INT_COUNT];


static rt_uint32_t linux_stage2_data_abort_srt(rt_uint64_t esr)
{
    return (esr & ESR_ISS_SRT_MASK) >> ESR_ISS_SRT_SHIFT;
}

static rt_uint32_t linux_stage2_data_abort_access_size(rt_uint64_t esr)
{
    rt_uint32_t sas;

    sas = (rt_uint32_t)((esr & ESR_ISS_SAS_MASK) >> ESR_ISS_SAS_SHIFT);

    return 1U << sas;
}

static rt_uint64_t linux_stage2_read_guest_reg(struct linux_stage2_trap_frame *tf, rt_uint32_t reg)
{
    if (reg >= 31U) {
        return 0;
    }

    return tf->x[reg];
}

static void linux_stage2_write_guest_reg(struct linux_stage2_trap_frame *tf, rt_uint32_t reg, rt_uint64_t value)
{
    if (reg >= 31U) {
        return;
    }

    if ((tf->esr & ESR_ISS_SF) == 0) {
        value &= 0xffffffffUL;
    }

    tf->x[reg] = value;
}

static rt_uint32_t linux_gicd_shadow_vcpu_id(void)
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

static rt_uint8_t linux_gicd_shadow_bank_target_mask(void)
{
    return linux_gicd_shadow_vcpu_id() == 0U ? GICD_TARGET_CPU2 : GICD_TARGET_CPU3;
}

static rt_uint32_t linux_gicd_shadow_target_to_vcpu(rt_uint32_t target)
{
    if (target == GICD_TARGET_CPU2) {
        return 0;
    }

    if (target == GICD_TARGET_CPU3) {
        return 1;
    }

    return LINUX_GUEST_VCPU_COUNT;
}

static void linux_gicd_shadow_sync_sgi_pending(rt_uint32_t target_cpu)
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

static void linux_gicd_shadow_update_sgi_bitmap(rt_uint32_t target_cpu, rt_uint32_t bits, rt_bool_t is_set_reg)
{
    rt_uint8_t source_mask = linux_gicd_shadow_bank_target_mask();

    for (rt_uint32_t sgi = 0; sgi < GICD_SGI_INT_COUNT; ++sgi) {
        if ((bits & (1U << sgi)) == 0U) {
            continue;
        }

        if (is_set_reg) {
            linux_gicd_spendsgir[target_cpu][sgi] |= source_mask;
        } else {
            linux_gicd_spendsgir[target_cpu][sgi] = 0U;
        }
    }

    linux_gicd_shadow_sync_sgi_pending(target_cpu);
}

/**
 * gicd_ctlr 读写全部虚拟 不会作用到实际的gicd
 * ctrl reg
 */
static int linux_gicd_shadow_ctlr_access(struct linux_stage2_trap_frame *tf)
{
    rt_uint32_t srt;

    if ((tf->esr & ESR_ISS_SAS_MASK) != ESR_SAS_WORD) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);

    if ((tf->esr & ESR_ISS_WNR) != 0) {
        rt_uint64_t value = linux_stage2_read_guest_reg(tf, srt);
        hyp_log_puts("[gicd_ctrl][w] value=");
        hyp_log_put_hex(value);
        hyp_log_putc('\n');
        linux_gicd_ctlr = linux_stage2_read_guest_reg(tf, srt) & 0x3U;
    } else {
        hyp_log_puts("[gicd_ctrl][r]");
        linux_stage2_write_guest_reg(tf, srt, linux_gicd_ctlr);
    }

    tf->elr += 4;

    return 0;
}

/**
 * gicd_typer 只读寄存器 写死96个中断和2个cpu
 * bit[10]  security_extn
 * bit[4:0] itlines_number
 * bit[7:5] cpu_number
 */
static int linux_gicd_shadow_typer_access(struct linux_stage2_trap_frame *tf)
{
    rt_uint32_t srt;
    rt_uint32_t typer;

    if ((tf->esr & ESR_ISS_SAS_MASK) != ESR_SAS_WORD) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);
    typer = GICD_TYPER_ITLINES(96) | GICD_TYPER_CPUS(2) | GICD_TYPER_SECURITY_EXTN;

    if ((tf->esr & ESR_ISS_WNR) == 0) {
        hyp_log_puts("[gicd_typer][r] value=");
        hyp_log_put_hex(typer);
        hyp_log_putc('\n');

        linux_stage2_write_guest_reg(tf, srt, typer);
    }

    tf->elr += 4;

    return 0;
}

/**
 * gicd_icpidr
 * todo
 */
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

/** 
 * gicd_igoupr
 * int group reg
 * 每个bit都表示一个中断是分组0还是分组1
 */
static int linux_gicd_shadow_igroupr_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset)
{
    rt_uint32_t srt;
    rt_uint32_t index;

    if ((tf->esr & ESR_ISS_SAS_MASK) != ESR_SAS_WORD) {
        return -RT_ERROR;
    }

    index = (rt_uint32_t)((offset - GICD_IGROUPR_OFFSET_BASE) >> 2);
    if (index >= GICD_IGROUPR_COUNT) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);

    if ((tf->esr & ESR_ISS_WNR) != 0) {
        hyp_log_puts("[gicd_irq_group][w] value=");
        hyp_log_put_hex(linux_stage2_read_guest_reg(tf, srt));
        hyp_log_putc('\n');
        linux_gicd_igroupr[index] = (rt_uint32_t)linux_stage2_read_guest_reg(tf, srt);
    } else {
        hyp_log_puts("[gicd_irq_group][r] value=");
        hyp_log_put_hex(linux_gicd_igroupr[index]);
        hyp_log_putc('\n');
        linux_stage2_write_guest_reg(tf, srt, linux_gicd_igroupr[index]);
    }

    tf->elr += 4;

    return 0;
}


static rt_uint32_t *linux_gicd_shadow_enable_word(rt_uint64_t offset)
{
    rt_uint32_t index;

    index = (rt_uint32_t)((offset - GICD_ISENABLER_OFFSET_BASE) >> 2);
    if (index >= GICD_ISENABLER_COUNT) {
        return RT_NULL;
    }

    if (index == 0U) {
        return &linux_gicd_isenabler0[linux_gicd_shadow_vcpu_id()];
    }

    return &linux_gicd_isenabler[index - 1U];
}

/**
 * gicd_isenable
 * is_enable_reg
 * cpu0禁用所有spi
 * cpu0禁用所有spi和ppi
 * cpu0开启spi0-6 ppi0x1b
 * cpu1禁用所有spi和ppi
 * cpu1开启spi0-6 ppi0x1b
 * 
 */
static int linux_gicd_shadow_isenabler_access(struct linux_stage2_trap_frame *tf,
                                              rt_uint64_t offset,
                                              rt_bool_t is_set_reg)
{
    rt_uint32_t srt;
    rt_uint32_t value;
    rt_uint32_t *shadow;
    rt_uint32_t base_int_id;
    base_int_id = (offset - GICD_ISENABLER_OFFSET_BASE) * 8; /* 1个字节对应8个中断 */

    if ((tf->esr & ESR_ISS_SAS_MASK) != ESR_SAS_WORD) {
        return -RT_ERROR;
    }

    shadow = linux_gicd_shadow_enable_word(offset);
    if (shadow == RT_NULL) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);

    if ((tf->esr & ESR_ISS_WNR) != 0) {
        value = (rt_uint32_t)linux_stage2_read_guest_reg(tf, srt);

        if (is_set_reg) {
            *shadow |= value;       // write 1 set enable 
        } else {
            *shadow &= ~value;      // write 1 set enable 
        }

        for (rt_uint32_t bit = 0; bit < 32U; ++bit) {
            if ((value & (1U << bit)) == 0U) {
                continue;
            }
            hyp_log_put_hex(linux_gicd_shadow_vcpu_id());
            hyp_log_puts(is_set_reg ? "[gicd_enable][w] intid=" : "[gicd_disable][w] intid=");
            hyp_log_put_hex(base_int_id + bit);
            hyp_log_putc('\n');
        }

    } else {
        hyp_log_put_hex(linux_gicd_shadow_vcpu_id());
        hyp_log_puts(is_set_reg ? "[gicd_enable][r] value=" : "[gicd_disable][r] value=");
        hyp_log_put_hex(*shadow);
        hyp_log_putc('\n');

        for (rt_uint32_t bit = 0; bit < 32U; ++bit) {
            if ((*shadow & (1U << bit)) == 0U) {
                continue;
            }

            hyp_log_puts("[gicd_enable_state] intid=");
            hyp_log_put_hex(base_int_id + bit);
            hyp_log_puts(" enabled=1\n");
        }
        linux_stage2_write_guest_reg(tf, srt, *shadow);
    }

    tf->elr += 4;

    return 0;
}

static rt_uint8_t *linux_gicd_shadow_priority_byte(rt_uint32_t intid)
{
    if (intid < GICD_IPRIORITYR_BANKED_INT) {
        return &linux_gicd_ipriorityr0[linux_gicd_shadow_vcpu_id()][intid];
    }

    if (intid < GICD_IPRIORITYR_INT_COUNT) {
        return &linux_gicd_ipriorityr[intid - GICD_IPRIORITYR_BANKED_INT];
    }

    return RT_NULL;
}

static int linux_gicd_shadow_ipriorityr_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset)
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
        }
    } else {
        value = 0;

        for (rt_uint32_t i = 0; i < size; ++i) {
            byte_ptr = linux_gicd_shadow_priority_byte(intid + i);
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
static int linux_gicd_shadow_itargetsr_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset)
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

    intid = (rt_uint32_t)(offset - GICD_ITARGETSR_OFFSET_BASE);/* 1个字节对应1个中断，所以偏移刚好是中断号 */
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

#if 0
            hyp_log_puts("[gicd_itargetsr][w] intid=");
            hyp_log_put_hex(intid+i);
            hyp_log_puts(" cpu_mask=");
            hyp_log_put_hex(byte_value & GICD_TARGET_CPU_MASK);
            hyp_log_putc('\n');

#endif
        }
    } else {
        value = 0;

        for (rt_uint32_t i = 0; i < size; ++i) {
            shift = i * 8U;

            if ((intid + i) < GICD_ITARGETSR_BANKED_INT) {
                value |= ((rt_uint64_t)linux_gicd_shadow_bank_target_mask() << shift);
#if 0
                hyp_log_puts("[gicd_itargetsr][r] intid=");
                hyp_log_put_hex(intid+i);
                hyp_log_puts(" cpu_mask=");
                hyp_log_put_hex(linux_gicd_shadow_bank_target_mask() & GICD_TARGET_CPU_MASK);
                hyp_log_putc('\n');
#endif
                continue;
            }

            byte_ptr = linux_gicd_shadow_target_byte(intid + i);
            if (byte_ptr == RT_NULL) {
                return -RT_ERROR;
            }
            hyp_log_puts("[gicd_itargetsr][r] intid=");
            hyp_log_put_hex(intid+i);
            hyp_log_puts(" cpu_mask=");
            hyp_log_put_hex(*byte_ptr & GICD_TARGET_CPU_MASK);
            hyp_log_putc('\n');

            value |= ((rt_uint64_t)(*byte_ptr) << shift);
        }

        linux_stage2_write_guest_reg(tf, srt, value);
    }

    tf->elr += 4;

    return 0;
}


static rt_uint32_t *linux_gicd_shadow_icfgr_word(rt_uint64_t offset)
{
    rt_uint32_t index;

    index = (rt_uint32_t)((offset - GICD_ICFGR_OFFSET_BASE) >> 2);
    if (index >= GICD_ICFGR_COUNT) {
        return RT_NULL;
    }

    if (index < GICD_ICFGR_BANKED_WORDS) {
        return &linux_gicd_icfgr0[linux_gicd_shadow_vcpu_id()][index];
    }

    return &linux_gicd_icfgr[index - GICD_ICFGR_BANKED_WORDS];
}

static rt_uint32_t linux_gicd_shadow_icfgr_byte_mask(rt_uint64_t offset, rt_uint32_t size)
{
    rt_uint32_t byte_shift;
    rt_uint32_t mask;

    byte_shift = (rt_uint32_t)(offset & 0x3U) * 8U;

    switch (size) {
    case 1U:
        mask = 0x000000ffU;
        break;
    case 2U:
        mask = 0x0000ffffU;
        break;
    case 4U:
        mask = 0xffffffffU;
        break;
    default:
        return 0U;
    }

    mask <<= byte_shift;

    return mask;
}

static int linux_gicd_shadow_icfgr_access(struct linux_stage2_trap_frame *tf, rt_uint64_t offset)
{
    rt_uint32_t srt;
    rt_uint32_t size;
    rt_uint32_t *shadow;
    rt_uint32_t byte_shift;
    rt_uint32_t field_mask;
    rt_uint32_t write_mask;
    rt_uint32_t value32;
    rt_uint32_t read_mask;
    rt_uint64_t read_value;

    size = linux_stage2_data_abort_access_size(tf->esr);
    if ((size != 1U) && (size != 2U) && (size != 4U)) {
        return -RT_ERROR;
    }

    if (((offset - GICD_ICFGR_OFFSET_BASE) + size) > (GICD_ICFGR_COUNT * 4U)) {
        return -RT_ERROR;
    }

    if (((offset & 0x3U) + size) > 4U) {
        return -RT_ERROR;
    }

    shadow = linux_gicd_shadow_icfgr_word(offset);
    if (shadow == RT_NULL) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);
    byte_shift = (rt_uint32_t)(offset & 0x3U) * 8U;

    if ((tf->esr & ESR_ISS_WNR) != 0) {
        if ((((offset - GICD_ICFGR_OFFSET_BASE) >> 2) == 0U)) {
            tf->elr += 4;
            return 0;
        }

        value32 = (rt_uint32_t)linux_stage2_read_guest_reg(tf, srt);
        value32 <<= byte_shift;

        write_mask = linux_gicd_shadow_icfgr_byte_mask(offset, size);
        field_mask = write_mask & GICD_ICFGR_EDGE_MASK;

        *shadow &= ~write_mask;
        *shadow |= (value32 & field_mask);
    } else {
        read_mask = linux_gicd_shadow_icfgr_byte_mask(0, size);
        read_value = (rt_uint64_t)((*shadow >> byte_shift) & read_mask);
        linux_stage2_write_guest_reg(tf, srt, read_value);
    }

    tf->elr += 4;

    return 0;
}

static rt_uint32_t *linux_gicd_shadow_pending_word(rt_uint64_t offset)
{
    rt_uint32_t index;

    index = (rt_uint32_t)((offset - GICD_ISPENDR_OFFSET_BASE) >> 2);
    if (index >= GICD_ISPENDR_COUNT) {
        return RT_NULL;
    }

    if (index == 0U) {
        return &linux_gicd_ispendr0[linux_gicd_shadow_vcpu_id()];
    }

    return &linux_gicd_ispendr[index - 1U];
}

static int linux_gicd_shadow_ispendr_access(struct linux_stage2_trap_frame *tf,
                                            rt_uint64_t offset,
                                            rt_bool_t is_set_reg)
{
    rt_uint32_t srt;
    rt_uint32_t index;
    rt_uint32_t value;
    rt_uint32_t *shadow;
    rt_uint32_t target_cpu;

    if ((tf->esr & ESR_ISS_SAS_MASK) != ESR_SAS_WORD) {
        return -RT_ERROR;
    }

    index = (rt_uint32_t)((offset - GICD_ISPENDR_OFFSET_BASE) >> 2);
    shadow = linux_gicd_shadow_pending_word(offset);
    if (shadow == RT_NULL) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);

    if ((tf->esr & ESR_ISS_WNR) != 0) {
        value = (rt_uint32_t)linux_stage2_read_guest_reg(tf, srt);

        if (is_set_reg) {
            *shadow |= value;
        } else {
            *shadow &= ~value;
        }

        if (index == 0U) {
            target_cpu = linux_gicd_shadow_vcpu_id();
            linux_gicd_shadow_update_sgi_bitmap(target_cpu, value & 0x0000ffffU, is_set_reg);
        }
    } else {
        linux_stage2_write_guest_reg(tf, srt, *shadow);
    }

    tf->elr += 4;

    return 0;
}

static rt_uint32_t *linux_gicd_shadow_active_word(rt_uint64_t offset)
{
    rt_uint32_t index;

    index = (rt_uint32_t)((offset - GICD_ISACTIVER_OFFSET_BASE) >> 2);
    if (index >= GICD_ISACTIVER_COUNT) {
        return RT_NULL;
    }

    if (index == 0U) {
        return &linux_gicd_iactiver0[linux_gicd_shadow_vcpu_id()];
    }

    return &linux_gicd_iactiver[index - 1U];
}

static int linux_gicd_shadow_iactiver_access(struct linux_stage2_trap_frame *tf,
                                             rt_uint64_t offset,
                                             rt_bool_t is_set_reg)
{
    rt_uint32_t srt;
    rt_uint32_t value;
    rt_uint32_t *shadow;

    if ((tf->esr & ESR_ISS_SAS_MASK) != ESR_SAS_WORD) {
        return -RT_ERROR;
    }

    shadow = linux_gicd_shadow_active_word(offset);
    if (shadow == RT_NULL) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);

    if ((tf->esr & ESR_ISS_WNR) != 0) {
        value = (rt_uint32_t)linux_stage2_read_guest_reg(tf, srt);

        if (is_set_reg) {
            *shadow |= value;
        } else {
            *shadow &= ~value;
        }
    } else {
        linux_stage2_write_guest_reg(tf, srt, *shadow);
    }

    tf->elr += 4;

    return 0;
}

static rt_uint8_t *linux_gicd_shadow_spendsgir_byte(rt_uint64_t offset)
{
    rt_uint32_t sgi_id;

    sgi_id = (rt_uint32_t)(offset - GICD_SPENDSGIR_OFFSET_BASE);
    if (sgi_id >= GICD_SGI_INT_COUNT) {
        return RT_NULL;
    }

    return &linux_gicd_spendsgir[linux_gicd_shadow_vcpu_id()][sgi_id];
}

static int linux_gicd_shadow_spendsgir_access(struct linux_stage2_trap_frame *tf,
                                              rt_uint64_t offset,
                                              rt_bool_t is_set_reg)
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
    target_cpu = linux_gicd_shadow_vcpu_id();

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

static int linux_gicd_shadow_sgir_access(struct linux_stage2_trap_frame *tf)
{
    rt_uint32_t srt;
    rt_uint32_t value;
    rt_uint32_t sgi_id;
    rt_uint32_t target_mask;
    rt_uint32_t filter;
    rt_uint32_t source_mask;

    if ((tf->esr & ESR_ISS_SAS_MASK) != ESR_SAS_WORD) {
        return -RT_ERROR;
    }

    srt = linux_stage2_data_abort_srt(tf->esr);

    if ((tf->esr & ESR_ISS_WNR) == 0) {
        linux_stage2_write_guest_reg(tf, srt, 0);
        tf->elr += 4;
        return 0;
    }

    value = (rt_uint32_t)linux_stage2_read_guest_reg(tf, srt);
    sgi_id = value & GICD_SGIR_SGIINTID_MASK;
    filter = (value & GICD_SGIR_FILTER_MASK) >> GICD_SGIR_FILTER_SHIFT;
    source_mask = linux_gicd_shadow_bank_target_mask();

    if (sgi_id >= GICD_SGI_INT_COUNT) {
        return -RT_ERROR;
    }

    switch (filter) {
    case GICD_SGIR_FILTER_LIST:
        target_mask = ((value & GICD_SGIR_TARGET_LIST_MASK) >> GICD_SGIR_TARGET_LIST_SHIFT) & GICD_TARGET_CPU_MASK;
        break;
    case GICD_SGIR_FILTER_OTHERS:
        target_mask = GICD_TARGET_CPU_MASK & ~source_mask;
        break;
    case GICD_SGIR_FILTER_SELF:
        target_mask = source_mask;
        break;
    default:
        target_mask = 0U;
        break;
    }

    for (rt_uint32_t target = GICD_TARGET_CPU2; target <= GICD_TARGET_CPU3; target <<= 1) {
        rt_uint32_t cpu;

        if ((target_mask & target) == 0U) {
            continue;
        }

        cpu = linux_gicd_shadow_target_to_vcpu(target);
        if (cpu >= LINUX_GUEST_VCPU_COUNT) {
            continue;
        }

        linux_gicd_spendsgir[cpu][sgi_id] |= (rt_uint8_t)source_mask;
        linux_gicd_shadow_sync_sgi_pending(cpu);
    }

    tf->elr += 4;

    return 0;
}

void linux_gicd_shadow_init(void)
{
    rt_uint32_t i;

    linux_gicd_ctlr = 0;

    for (i = 0; i < GICD_IGROUPR_COUNT; ++i) {
        linux_gicd_igroupr[i] = 0xffffffffU;
    }

    for (i = 0; i < LINUX_GUEST_VCPU_COUNT; ++i) {
        linux_gicd_isenabler0[i] = 0x00000000U;
        linux_gicd_ispendr0[i] = 0x00000000U;
        linux_gicd_iactiver0[i] = 0x00000000U;
    }

    for (i = 0; i < (GICD_ISENABLER_COUNT - 1U); ++i) {
        linux_gicd_isenabler[i] = 0x00000000U;
    }

    for (i = 0; i < (GICD_ISPENDR_COUNT - 1U); ++i) {
        linux_gicd_ispendr[i] = 0x00000000U;
    }

    for (i = 0; i < (GICD_ISACTIVER_COUNT - 1U); ++i) {
        linux_gicd_iactiver[i] = 0x00000000U;
    }

    for (i = 0; i < LINUX_GUEST_VCPU_COUNT; ++i) {
        for (rt_uint32_t j = 0; j < GICD_IPRIORITYR_BANKED_INT; ++j) {
            linux_gicd_ipriorityr0[i][j] = GICD_IPRIORITYR_INIT_VALUE;
        }
    }

    for (i = 0; i < GICD_IPRIORITYR_SHARED_INT; ++i) {
        linux_gicd_ipriorityr[i] = GICD_IPRIORITYR_INIT_VALUE;
    }

    for (i = 0; i < GICD_ITARGETSR_SHARED_INT; ++i) {
        linux_gicd_itargetsr[i] = GICD_ITARGETSR_INIT_VALUE;
    }

    for (i = 0; i < LINUX_GUEST_VCPU_COUNT; ++i) {
        linux_gicd_icfgr0[i][0] = GICD_ICFGR_EDGE_MASK;
        linux_gicd_icfgr0[i][1] = 0x00000000U;
    }

    for (i = 0; i < GICD_ICFGR_SHARED_WORDS; ++i) {
        linux_gicd_icfgr[i] = 0x00000000U;
    }

    for (i = 0; i < LINUX_GUEST_VCPU_COUNT; ++i) {
        for (rt_uint32_t sgi = 0; sgi < GICD_SGI_INT_COUNT; ++sgi) {
            linux_gicd_spendsgir[i][sgi] = 0x00U;
        }
    }
}


static rt_uint64_t linux_stage2_fault_ipa(rt_uint64_t far, rt_uint64_t hpfar)
{
    return ((hpfar & 0xfffffffff0UL) << 8) | (far & 0xfffUL);
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

    if (offset == GICD_ICPIDR2_OFFSET) {
        return linux_gicd_shadow_icpidr2_access(tf);
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


    return -RT_ERROR;
}
