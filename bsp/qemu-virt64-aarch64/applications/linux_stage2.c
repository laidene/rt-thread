#include <rtthread.h>

#include "hyp_log.h"
#include "linux_stage2.h"

#define S2_BLOCK_SIZE        0x00200000UL
#define S2_PAGE_SIZE         0x00001000UL

#define S2_LINUX_RAM_BASE    0x48000000UL
#define S2_VIRTIO_BASE       0x0a000000UL
#define S2_GICV_PA           0x08040000UL
#define S2_GICD_BASE         0x08000000UL
#define S2_GICD_SIZE         0x00010000UL


/* desc_type [1:0] */
#define S2_DESC_BLOCK        0x1UL
#define S2_DESC_TABLE        0x3UL

#define S2_MEMATTR_DEVICE    (0x1UL << 2)
#define S2_MEMATTR_NORMAL    (0xfUL << 2)
#define S2_AP_RW             (0x3UL << 6)
#define S2_SH_INNER          (0x3UL << 8)
#define S2_AF                (0x1UL << 10)

#define S2_DEVICE_BLOCK_ATTR (S2_MEMATTR_DEVICE | S2_AP_RW | S2_SH_INNER | S2_AF | S2_DESC_BLOCK)
#define S2_DEVICE_PAGE_ATTR  (S2_MEMATTR_DEVICE | S2_AP_RW | S2_SH_INNER | S2_AF | S2_DESC_TABLE)
#define S2_NORMAL_BLOCK_ATTR (S2_MEMATTR_NORMAL | S2_AP_RW | S2_SH_INNER | S2_AF | S2_DESC_BLOCK)

/*
 * VTCR_EL2 for Linux stage-2:
 *
 * T0SZ  = 24      IPA size = 64 - 24 = 40 bits                 虚拟地址的位宽
 * SL0   = 0b01    start walk from level 1, for 4KB granule
 *      L0 index = IPA[47:39]
 *      L1 index = IPA[38:30]
 *      L2 index = IPA[29:21]
 *      L3 index = IPA[20:12]
 *      offset   = IPA[11:0]
 * IRGN0 = 0b01    inner WBWA cacheable table walk
 * ORGN0 = 0b01    outer WBWA cacheable table walk
 * SH0   = 0b11    inner shareable
 * TG0   = 0b00    4KB granule
 * PS    = 0b010   40-bit physical address size
 */
#define VTCR_EL2_T0SZ_40BIT_IPA (24UL << 0)
#define VTCR_EL2_SL0_L1         (1UL << 6)
#define VTCR_EL2_IRGN0_WBWA     (1UL << 8)
#define VTCR_EL2_ORGN0_WBWA     (1UL << 10)
#define VTCR_EL2_SH0_INNER      (3UL << 12)
#define VTCR_EL2_TG0_4KB        (0UL << 14)
#define VTCR_EL2_PS_40BIT       (2UL << 16)

#define VTCR_EL2_VALUE              \
      (VTCR_EL2_T0SZ_40BIT_IPA  |   \
       VTCR_EL2_SL0_L1          |   \
       VTCR_EL2_IRGN0_WBWA      |   \
       VTCR_EL2_ORGN0_WBWA      |   \
       VTCR_EL2_SH0_INNER       |   \
       VTCR_EL2_TG0_4KB         |   \
       VTCR_EL2_PS_40BIT)

static rt_uint64_t linux_stage2_l1_00000000_7fffffff[512] __attribute__((aligned(4096), section(".bss.noclean.linux_stage2")));
static rt_uint64_t linux_stage2_l2_00000000_3fffffff[512] __attribute__((aligned(4096), section(".bss.noclean.linux_stage2")));
static rt_uint64_t linux_stage2_l2_40000000_7fffffff[512] __attribute__((aligned(4096), section(".bss.noclean.linux_stage2")));
static rt_uint64_t linux_stage2_l3_08000000_081fffff[512] __attribute__((aligned(4096), section(".bss.noclean.linux_stage2")));


/**
 * Virtualization Translation Control Register
 */
static inline void hyp_write_vtcr_el2(rt_uint64_t value)
{
    __asm__ volatile("msr vtcr_el2, %0" ::"r"(value) : "memory");
}

/**
 * Virtualization Translation Table Base Register
 */
static inline void hyp_write_vttbr_el2(rt_uint64_t value)
{
    __asm__ volatile("msr vttbr_el2, %0" ::"r"(value) : "memory");
}

static inline void hyp_stage2_tlb_invalidate(void)
{
    __asm__ volatile(
        "dsb ish\n\t"
        "tlbi vmalls12e1is\n\t"
        "dsb ish\n\t"
        "isb"
        ::
        : "memory");
}

static void hyp_clean_dcache(void *addr, rt_size_t size)
{
    rt_uint8_t *ptr = addr;
    rt_uint8_t *end = ptr + size;

    while (ptr < end) {
        __asm__ volatile("dc cvac, %0" ::"r"(ptr) : "memory");
        ptr += 64;
    }

    __asm__ volatile("dsb ish\n\tisb" ::: "memory");
}

static void linux_stage2_zero_tables(void)
{
    for (int i = 0; i < 512; ++i) {
        linux_stage2_l1_00000000_7fffffff[i] = 0;
        linux_stage2_l2_00000000_3fffffff[i] = 0;
        linux_stage2_l2_40000000_7fffffff[i] = 0;
        linux_stage2_l3_08000000_081fffff[i] = 0;
    }
}

static void linux_stage2_map_gicv(void)
{
    rt_uint64_t pa = S2_GICV_PA;

    /*
     * Guest GICC IPA 0x08010000-0x0801ffff maps to real GICV.
     * Guest GICD IPA 0x08000000 deliberately stays unmapped.
     */
    for (int idx = 16; idx < 32; ++idx) {
        linux_stage2_l3_08000000_081fffff[idx] = pa | S2_DEVICE_PAGE_ATTR;
        pa += S2_PAGE_SIZE;
    }
}

static void linux_stage2_map_ram(void)
{
    rt_uint64_t pa = S2_LINUX_RAM_BASE;

    for (int idx = 64; idx < 128; ++idx) {
        linux_stage2_l2_40000000_7fffffff[idx] = pa | S2_NORMAL_BLOCK_ATTR;
        pa += S2_BLOCK_SIZE;
    }
}

void linux_stage2_init(void)
{
    linux_stage2_zero_tables();

    linux_stage2_l1_00000000_7fffffff[0] = (rt_uint64_t)linux_stage2_l2_00000000_3fffffff | S2_DESC_TABLE;
    linux_stage2_l1_00000000_7fffffff[1] = (rt_uint64_t)linux_stage2_l2_40000000_7fffffff | S2_DESC_TABLE;

    linux_stage2_l2_00000000_3fffffff[64] = (rt_uint64_t)linux_stage2_l3_08000000_081fffff | S2_DESC_TABLE;
    linux_stage2_l2_00000000_3fffffff[80] = S2_VIRTIO_BASE | S2_DEVICE_BLOCK_ATTR;

    linux_stage2_map_gicv();
    linux_stage2_map_ram();

    hyp_clean_dcache(linux_stage2_l1_00000000_7fffffff, sizeof(linux_stage2_l1_00000000_7fffffff));
    hyp_clean_dcache(linux_stage2_l2_00000000_3fffffff, sizeof(linux_stage2_l2_00000000_3fffffff));
    hyp_clean_dcache(linux_stage2_l2_40000000_7fffffff, sizeof(linux_stage2_l2_40000000_7fffffff));
    hyp_clean_dcache(linux_stage2_l3_08000000_081fffff, sizeof(linux_stage2_l3_08000000_081fffff));

    hyp_write_vtcr_el2(VTCR_EL2_VALUE);
    hyp_write_vttbr_el2((rt_uint64_t)linux_stage2_l1_00000000_7fffffff);
    hyp_stage2_tlb_invalidate();
}

static rt_uint64_t linux_stage2_fault_ipa(rt_uint64_t far, rt_uint64_t hpfar)
{
    return ((hpfar & 0xfffffffff0UL) << 8) | (far & 0xfffUL);
}


/**
 * esr   = Exception Syndrome Register
 *      EC    = Data Abort from lower EL
 *      ISS   = data abort syndrome
 *      WnR   = 0/1，读或写
 *      SRT   = guest 目标寄存器编号
 *      SRT   = guest 目标寄存器编号
 *      SAS   = 访问宽度
 *      DFSC  = fault 类型
 * far   = Fault Address Register
 *       对于 data abort，它通常保存 fault 发生时的 guest VA，也就是 Linux 当时访问的虚拟地址
 * hpfar = Hypervisor IPA Fault Address Register
 *      它保存 stage-2 fault 对应的 IPA 高位。通常用它和 FAR_EL2 的低 12 位组合，恢复 IPA
 * elr   = Exception Link Register
 *      它保存异常返回地址，也就是 Linux 触发 fault 的那条指令地址
 */
void linux_stage2_abort(rt_uint64_t esr, rt_uint64_t far, rt_uint64_t hpfar, rt_uint64_t elr)
{
    rt_uint64_t ipa = linux_stage2_fault_ipa(far, hpfar);
    rt_uint64_t gicd_offset = ipa - S2_GICD_BASE;

    hyp_log_stage2_abort(ipa, gicd_offset, esr, far, hpfar, elr);
}
