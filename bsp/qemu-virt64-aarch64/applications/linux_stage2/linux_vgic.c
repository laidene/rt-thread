#include <rtthread.h>

#include "hyp/hyp_log.h"
#include "data_abort_helper.h"
#include "gicd/gicd_reg.h"
#include "linux_vgic.h"

#define GICH_BASE       0x08030000UL
#define GICC_BASE       0x08010000UL

#define GICH_HCR        0x0000UL
#define GICH_VTR        0x0004UL
#define GICH_VMCR       0x0008UL
#define GICH_MISR       0x0010UL
#define GICH_EISR0      0x0020UL
#define GICH_EISR1      0x0024UL
#define GICH_ELRSR0     0x0030UL
#define GICH_ELRSR1     0x0034UL
#define GICH_APR        0x00f0UL
#define GICH_LR_BASE    0x0100UL

#define GICC_CTLR       0x0000UL
#define GICC_PMR        0x0004UL
#define GICC_BPR        0x0008UL
#define GICC_IAR        0x000cUL
#define GICC_EOIR       0x0010UL
#define GICC_RPR        0x0014UL
#define GICC_HPPIR      0x0018UL
#define GICC_DIR        0x1000UL

#define GICH_HCR_EN     (1U << 0)
#define GICH_VTR_LISTREGS_MASK 0x3fU
#define GICH_VMCR_GRP0_EN           (1U << 0)
#define GICH_VMCR_GRP1_EN           (1U << 1)
#define GICC_CTLR_EN    0x1U
#define GICC_CTLR_ENABLE_GRP0       (1U << 0)
#define GICC_CTLR_ENABLE_GRP1       (1U << 1)
#define GICC_CTLR_EOIMODE           (1U << 9)
#define GICC_PMR_ALLOW_ALL 0xffU
#define GICC_INTID_MASK 0x3ffU
#define GICC_IAR_CPUID_SHIFT        10U
#define GICC_IAR_CPUID_MASK         0x7U

#define LINUX_VGIC_TIMER_INTID      27U
#define LINUX_VGIC_VIRTIO0_INTID    48U
#define LINUX_VGIC_VIRTIO1_INTID    49U
#define LINUX_VGIC_DEFAULT_PRIO     0xa0U

#define GICH_LR_VIRTID_SHIFT        0U
#define GICH_LR_PHYSID_SHIFT        10U
#define GICH_LR_PRIO_SHIFT          23U
#define GICH_LR_STATE_MASK          (3U << 28)
#define GICH_LR_STATE_PENDING       (1U << 28)
#define GICH_LR_GROUP1              (1U << 30)
#define GICH_LR_HW                  (1U << 31)

#define LINUX_VGIC_MAX_LRS          64U

struct linux_vgic_snapshot {
    rt_uint32_t valid;
    rt_uint32_t lr_count;
    rt_uint32_t hcr;
    rt_uint32_t vtr;
    rt_uint32_t vmcr;
    rt_uint32_t misr;
    rt_uint32_t eisr0;
    rt_uint32_t eisr1;
    rt_uint32_t elrsr0;
    rt_uint32_t elrsr1;
    rt_uint32_t apr;
    rt_uint32_t gicc_ctlr;
    rt_uint32_t gicc_pmr;
    rt_uint32_t gicc_bpr;
    rt_uint32_t lr[LINUX_VGIC_MAX_LRS];
};

static volatile struct linux_vgic_snapshot linux_vgic_snapshots[LINUX_GUEST_VCPU_COUNT];
static rt_uint32_t linux_vgic_sgi_log_count[LINUX_GUEST_VCPU_COUNT][GICD_SGI_INT_COUNT];
static rt_uint32_t linux_vgic_hw_log_count[LINUX_GUEST_VCPU_COUNT][GICC_INTID_MASK + 1U];
static rt_uint32_t linux_vgic_hw_dup_log_count[LINUX_GUEST_VCPU_COUNT][GICC_INTID_MASK + 1U];

static volatile rt_uint32_t *linux_vgic_reg32(rt_uint64_t offset)
{
    return (volatile rt_uint32_t *)(GICH_BASE + offset);
}

static rt_uint32_t linux_vgic_read32(rt_uint64_t offset)
{
    return *linux_vgic_reg32(offset);
}

static void linux_vgic_write32(rt_uint64_t offset, rt_uint32_t value)
{
    *linux_vgic_reg32(offset) = value;
}

static rt_uint32_t linux_vgic_read_gicc32(rt_uint64_t offset)
{
    return *(volatile rt_uint32_t *)(GICC_BASE + offset);
}

static void linux_vgic_write_gicc32(rt_uint64_t offset, rt_uint32_t value)
{
    *(volatile rt_uint32_t *)(GICC_BASE + offset) = value;
}

static int linux_vgic_find_free_lr(void)
{
    rt_uint32_t lr_count = linux_vgic_lr_count();

    for (rt_uint32_t i = 0; i < lr_count; ++i) {
        if ((linux_vgic_read32(GICH_LR_BASE + i * 4U) & GICH_LR_STATE_MASK) == 0U) {
            return (int)i;
        }
    }

    return -1;
}

static int linux_vgic_find_hw_lr(rt_uint32_t phys_intid)
{
    rt_uint32_t lr_count = linux_vgic_lr_count();

    for (rt_uint32_t i = 0; i < lr_count; ++i) {
        rt_uint32_t lr = linux_vgic_read32(GICH_LR_BASE + i * 4U);

        if (((lr & GICH_LR_HW) != 0U) &&
            ((lr & GICH_LR_STATE_MASK) != 0U) &&
            (((lr >> GICH_LR_PHYSID_SHIFT) & GICC_INTID_MASK) == phys_intid)) {
            return (int)i;
        }
    }

    return -1;
}

static rt_uint32_t linux_vgic_lr_priority(rt_uint32_t priority)
{
    return ((priority >> 3) & 0x1fU) << GICH_LR_PRIO_SHIFT;
}

static rt_uint32_t linux_vgic_lr_group(void)
{
    rt_uint32_t vmcr = linux_vgic_read32(GICH_VMCR);

    if ((vmcr & GICH_VMCR_GRP1_EN) != 0U) {
        return GICH_LR_GROUP1;
    }

    return 0;
}

static rt_uint32_t linux_vgic_make_hw_lr(rt_uint32_t virt_intid, rt_uint32_t phys_intid)
{
    return ((virt_intid & GICC_INTID_MASK) << GICH_LR_VIRTID_SHIFT) |
           ((phys_intid & GICC_INTID_MASK) << GICH_LR_PHYSID_SHIFT) |
           linux_vgic_lr_priority(LINUX_VGIC_DEFAULT_PRIO) |
           GICH_LR_STATE_PENDING |
           linux_vgic_lr_group() |
           GICH_LR_HW;
}

static rt_uint32_t linux_vgic_iar_source_cpu(rt_uint32_t iar)
{
    return (iar >> GICC_IAR_CPUID_SHIFT) & GICC_IAR_CPUID_MASK;
}

static rt_uint32_t linux_vgic_make_sw_lr(rt_uint32_t virt_intid, rt_uint32_t source_cpu)
{
    return ((virt_intid & GICC_INTID_MASK) << GICH_LR_VIRTID_SHIFT) |
           ((source_cpu & GICC_IAR_CPUID_MASK) << GICH_LR_PHYSID_SHIFT) |
           linux_vgic_lr_priority(LINUX_VGIC_DEFAULT_PRIO) |
           GICH_LR_STATE_PENDING |
           linux_vgic_lr_group();
}

static rt_bool_t linux_vgic_is_hw_inject_intid(rt_uint32_t intid)
{
    return (intid == LINUX_VGIC_TIMER_INTID) ||
           (intid == LINUX_VGIC_VIRTIO0_INTID) ||
           (intid == LINUX_VGIC_VIRTIO1_INTID);
}

static const char *linux_vgic_hw_inject_name(rt_uint32_t intid)
{
    if (intid == LINUX_VGIC_TIMER_INTID) {
        return "timer";
    }

    if (intid == LINUX_VGIC_VIRTIO0_INTID) {
        return "virtio0";
    }

    if (intid == LINUX_VGIC_VIRTIO1_INTID) {
        return "virtio1";
    }

    return "hw";
}

static rt_bool_t linux_vgic_should_log_sgi(rt_uint32_t vcpu, rt_uint32_t sgi)
{
    rt_uint32_t count;

    if ((vcpu >= LINUX_GUEST_VCPU_COUNT) || (sgi >= GICD_SGI_INT_COUNT)) {
        return RT_FALSE;
    }

    count = ++linux_vgic_sgi_log_count[vcpu][sgi];

    return (count <= 8U) || ((count & 0x3ffU) == 0U);
}

static rt_bool_t linux_vgic_should_log_hw(rt_uint32_t vcpu, rt_uint32_t intid)
{
    rt_uint32_t count;

    if ((vcpu >= LINUX_GUEST_VCPU_COUNT) || (intid > GICC_INTID_MASK)) {
        return RT_FALSE;
    }

    count = ++linux_vgic_hw_log_count[vcpu][intid];

    return (count <= 8U) || ((count & 0x3ffU) == 0U);
}

static rt_bool_t linux_vgic_should_log_hw_dup(rt_uint32_t vcpu, rt_uint32_t intid)
{
    rt_uint32_t count;

    if ((vcpu >= LINUX_GUEST_VCPU_COUNT) || (intid > GICC_INTID_MASK)) {
        return RT_FALSE;
    }

    count = ++linux_vgic_hw_dup_log_count[vcpu][intid];

    return (count <= 4U) || ((count & 0xfffU) == 0U);
}

static void linux_vgic_snapshot_current(void)
{
    rt_uint32_t vcpu = linux_get_vcpu_id();
    rt_uint32_t lr_count;
    volatile struct linux_vgic_snapshot *snapshot;

    if (vcpu >= LINUX_GUEST_VCPU_COUNT) {
        return;
    }

    snapshot = &linux_vgic_snapshots[vcpu];
    lr_count = linux_vgic_lr_count();
    if (lr_count > LINUX_VGIC_MAX_LRS) {
        lr_count = LINUX_VGIC_MAX_LRS;
    }

    snapshot->lr_count = lr_count;
    snapshot->hcr = linux_vgic_read32(GICH_HCR);
    snapshot->vtr = linux_vgic_read32(GICH_VTR);
    snapshot->vmcr = linux_vgic_read32(GICH_VMCR);
    snapshot->misr = linux_vgic_read32(GICH_MISR);
    snapshot->eisr0 = linux_vgic_read32(GICH_EISR0);
    snapshot->eisr1 = linux_vgic_read32(GICH_EISR1);
    snapshot->elrsr0 = linux_vgic_read32(GICH_ELRSR0);
    snapshot->elrsr1 = linux_vgic_read32(GICH_ELRSR1);
    snapshot->apr = linux_vgic_read32(GICH_APR);
    snapshot->gicc_ctlr = linux_vgic_read_gicc32(GICC_CTLR);
    snapshot->gicc_pmr = linux_vgic_read_gicc32(GICC_PMR);
    snapshot->gicc_bpr = linux_vgic_read_gicc32(GICC_BPR);

    for (rt_uint32_t i = 0; i < lr_count; ++i) {
        snapshot->lr[i] = linux_vgic_read32(GICH_LR_BASE + i * 4U);
    }

    snapshot->valid = 1U;
}

rt_uint32_t linux_vgic_lr_count(void)
{
    return (linux_vgic_read32(GICH_VTR) & GICH_VTR_LISTREGS_MASK) + 1U;
}

void linux_vgic_init_cpu(void)
{
    rt_uint32_t lr_count = linux_vgic_lr_count();

    linux_vgic_write_gicc32(GICC_CTLR, 0);
    linux_vgic_write_gicc32(GICC_PMR, GICC_PMR_ALLOW_ALL);
    linux_vgic_write_gicc32(GICC_BPR, 0);
    linux_vgic_write_gicc32(GICC_CTLR, GICC_CTLR_ENABLE_GRP0 | GICC_CTLR_ENABLE_GRP1 | GICC_CTLR_EOIMODE);

    linux_vgic_write32(GICH_HCR, 0);

    for (rt_uint32_t i = 0; i < lr_count; ++i) {
        linux_vgic_write32(GICH_LR_BASE + i * 4U, 0);
    }

    linux_vgic_write32(GICH_APR, 0);
    linux_vgic_write32(GICH_HCR, GICH_HCR_EN);
    __asm__ volatile("dsb sy\n\tisb" ::: "memory");
    linux_vgic_snapshot_current();

    hyp_log_printf("[vgic] init vcpu=%u lr_count=%u hcr=%x vtr=%x gicc_ctlr=%x gicc_pmr=%x\n",
                   linux_get_vcpu_id(),
                   lr_count,
                   linux_vgic_read32(GICH_HCR),
                   linux_vgic_read32(GICH_VTR),
                   linux_vgic_read_gicc32(GICC_CTLR),
                   linux_vgic_read_gicc32(GICC_PMR));
    linux_vgic_log_cpuif_state("after-init");
}

void linux_vgic_log_physical_irq(void)
{
    rt_uint32_t iar = linux_vgic_read_gicc32(GICC_IAR);
    rt_uint32_t intid = iar & GICC_INTID_MASK;

    hyp_log_printf("[vgic] physical irq vcpu=%u iar=%x intid=%u\n",
                   linux_get_vcpu_id(),
                   iar,
                   intid);
}

void linux_vgic_log_cpuif_state(const char *tag)
{
    rt_uint32_t hppir = linux_vgic_read_gicc32(GICC_HPPIR);
    rt_uint32_t rpr = linux_vgic_read_gicc32(GICC_RPR);

    hyp_log_printf("[gicc] %s vcpu=%u ctlr=%x pmr=%x bpr=%x rpr=%x hppir=%x hppir_intid=%u\n",
                   tag,
                   linux_get_vcpu_id(),
                   linux_vgic_read_gicc32(GICC_CTLR),
                   linux_vgic_read_gicc32(GICC_PMR),
                   linux_vgic_read_gicc32(GICC_BPR),
                   rpr,
                   hppir,
                   hppir & GICC_INTID_MASK);
}

int linux_vgic_handle_physical_irq(void)
{
    rt_uint32_t iar = linux_vgic_read_gicc32(GICC_IAR);
    rt_uint32_t intid = iar & GICC_INTID_MASK;
    rt_uint32_t lr;
    int lr_index;

    if (intid < GICD_SGI_INT_COUNT) {
        lr_index = linux_vgic_find_free_lr();
        if (lr_index < 0) {
            linux_vgic_snapshot_current();
            hyp_log_printf("[vgic] no free lr vcpu=%u iar=%x intid=%u\n",
                           linux_get_vcpu_id(),
                           iar,
                           intid);
            return -RT_ERROR;
        }

        rt_uint32_t source_cpu = linux_vgic_iar_source_cpu(iar);
        rt_uint32_t vcpu = linux_get_vcpu_id();

        lr = linux_vgic_make_sw_lr(intid, source_cpu);
        linux_vgic_write32(GICH_LR_BASE + (rt_uint32_t)lr_index * 4U, lr);
        linux_vgic_write_gicc32(GICC_EOIR, iar);
        linux_vgic_write_gicc32(GICC_DIR, iar);
        __asm__ volatile("dsb sy\n\tisb" ::: "memory");
        linux_vgic_snapshot_current();

        if (linux_vgic_should_log_sgi(vcpu, intid)) {
            hyp_log_printf("[vgic] inject sgi vcpu=%u intid=%u src=%u iar=%x lr%u=%x\n",
                           vcpu,
                           intid,
                           source_cpu,
                           iar,
                           (rt_uint32_t)lr_index,
                           linux_vgic_read32(GICH_LR_BASE + (rt_uint32_t)lr_index * 4U));
        }
        return 0;
    }

    if (!linux_vgic_is_hw_inject_intid(intid)) {
        linux_vgic_snapshot_current();
        hyp_log_printf("[vgic] unexpected physical irq vcpu=%u iar=%x intid=%u\n",
                       linux_get_vcpu_id(),
                       iar,
                       intid);
        return -RT_ERROR;
    }

    {
        int existing_lr = linux_vgic_find_hw_lr(intid);
        rt_uint32_t vcpu = linux_get_vcpu_id();

        if (existing_lr >= 0) {
            linux_vgic_write_gicc32(GICC_EOIR, iar);
            __asm__ volatile("dsb sy\n\tisb" ::: "memory");
            linux_vgic_snapshot_current();
            if (linux_vgic_should_log_hw_dup(vcpu, intid)) {
                hyp_log_printf("[vgic] skip duplicate %s vcpu=%u iar=%x lr%u=%x\n",
                               linux_vgic_hw_inject_name(intid),
                               vcpu,
                               iar,
                               (rt_uint32_t)existing_lr,
                               linux_vgic_read32(GICH_LR_BASE + (rt_uint32_t)existing_lr * 4U));
            }
            return 0;
        }
    }

    lr_index = linux_vgic_find_free_lr();
    if (lr_index < 0) {
        linux_vgic_snapshot_current();
        hyp_log_printf("[vgic] no free lr vcpu=%u iar=%x intid=%u\n",
                       linux_get_vcpu_id(),
                       iar,
                       intid);
        return -RT_ERROR;
    }

    lr = linux_vgic_make_hw_lr(intid, intid);

    linux_vgic_write32(GICH_LR_BASE + (rt_uint32_t)lr_index * 4U, lr);
    linux_vgic_write_gicc32(GICC_EOIR, iar);
    __asm__ volatile("dsb sy\n\tisb" ::: "memory");
    linux_vgic_snapshot_current();

    if (linux_vgic_should_log_hw(linux_get_vcpu_id(), intid)) {
        hyp_log_printf("[vgic] inject %s vcpu=%u iar=%x lr%u=%x\n",
                       linux_vgic_hw_inject_name(intid),
                       linux_get_vcpu_id(),
                       iar,
                       (rt_uint32_t)lr_index,
                       linux_vgic_read32(GICH_LR_BASE + (rt_uint32_t)lr_index * 4U));
    }

    return 0;
}

void linux_vgic_dump(void)
{
    for (rt_uint32_t vcpu = 0; vcpu < LINUX_GUEST_VCPU_COUNT; ++vcpu) {
        volatile struct linux_vgic_snapshot *snapshot = &linux_vgic_snapshots[vcpu];

        if (snapshot->valid == 0U) {
            rt_kprintf("vgic vcpu%u: no snapshot\n", vcpu);
            continue;
        }

        rt_kprintf("vgic vcpu%u hcr=0x%08x vtr=0x%08x vmcr=0x%08x misr=0x%08x apr=0x%08x\n",
                   vcpu,
                   snapshot->hcr,
                   snapshot->vtr,
                   snapshot->vmcr,
                   snapshot->misr,
                   snapshot->apr);
        rt_kprintf("vgic vcpu%u eisr0=0x%08x eisr1=0x%08x elrsr0=0x%08x elrsr1=0x%08x\n",
                   vcpu,
                   snapshot->eisr0,
                   snapshot->eisr1,
                   snapshot->elrsr0,
                   snapshot->elrsr1);
        rt_kprintf("vgic vcpu%u gicc_ctlr=0x%08x pmr=0x%08x bpr=0x%08x\n",
                   vcpu,
                   snapshot->gicc_ctlr,
                   snapshot->gicc_pmr,
                   snapshot->gicc_bpr);

        for (rt_uint32_t i = 0; i < snapshot->lr_count; ++i) {
            rt_kprintf("vgic vcpu%u lr%u=0x%08x\n", vcpu, i, snapshot->lr[i]);
        }
    }
}
