#ifndef __IMX6ULL_H__
#define __IMX6ULL_H__

#include <rthw.h>
#include <rtthread.h>

#ifdef RT_USING_LWP
    #include <lwp.h>
    #include <ioremap.h>
#endif

/* ============================================================================
 * 1 include sdk header files
 * ============================================================================ */

#include "MCIMX6Y2.h"

#include "fsl_cache.h"
#include "fsl_common.h"
#include "fsl_iomuxc.h"
#include "fsl_gpio.h"
#include "fsl_elcdif.h"
#include "fsl_usdhc.h"
#include "fsl_card.h"
#include "fsl_wdog.h"
#include "fsl_i2c.h"
#include "fsl_ecspi.h"
#include "fsl_snvs_hp.h"
#include "fsl_adc.h"



/* ============================================================================
 * 2  i.MX6ULL Peripheral Register Base Addresses
 * ============================================================================ */

enum _gic_base_offsets {
    kGICDBaseOffset = 0x1000,
    kGICCBaseOffset = 0x2000,
};

#define ARM_GIC_CPU_BASE            0x00A00000

#define IMX6ULL_UART1_BASE          UART1_BASE
#define IMX6ULL_UART2_BASE          UART2_BASE
#define IMX6ULL_UART3_BASE          UART3_BASE
#define IMX6ULL_UART4_BASE          UART4_BASE
#define IMX6ULL_UART5_BASE          UART5_BASE
#define IMX6ULL_UART6_BASE          UART6_BASE
#define IMX6ULL_UART7_BASE          UART7_BASE
#define IMX6ULL_UART8_BASE          UART8_BASE

#define IMX6ULL_WATCHDOG1_BASE      WDOG1_BASE
#define IMX6ULL_WATCHDOG2_BASE      WDOG2_BASE
#define IMX6ULL_WATCHDOG3_BASE      WDOG3_BASE

#define IMX6ULL_GPIO1_BASE          GPIO1_BASE
#define IMX6ULL_GPIO2_BASE          GPIO2_BASE
#define IMX6ULL_GPIO3_BASE          GPIO3_BASE
#define IMX6ULL_GPIO4_BASE          GPIO4_BASE
#define IMX6ULL_GPIO5_BASE          GPIO5_BASE

#define IMX6ULL_SNVS_BASE           SNVS_BASE   /* Real Time Clock */

#define IMX6ULL_SCTL_BASE           0x021DC000u /* System Controller */

#define IMX6ULL_CLCD_BASE           LCDIF_BASE

#define IMX6ULL_GIC_DIST_BASE       (ARM_GIC_CPU_BASE+kGICDBaseOffset)
#define IMX6ULL_GIC_CPU_BASE        (ARM_GIC_CPU_BASE+kGICCBaseOffset)

#define IMX6ULL_IOMUXC_BASE         IOMUXC_BASE
#define IMX6ULL_IOMUXC_SNVS_BASE    IOMUXC_SNVS_BASE
#define IMX6ULL_IOMUXC_GPR_BASE     IOMUXC_GPR_BASE

#define IMX6ULL_CCM_BASE            0x20C4000u
#define IMX6ULL_CCM_ANALOGY_BASE    0x20C8000u
#define IMX6ULL_PMU_BASE            0x20C8110u

#define IMX6ULL_ENET1_BASE          ENET1_BASE
#define IMX6ULL_ENET2_BASE          ENET2_BASE

#define IMX6ULL_GPT1_BASE           GPT1_BASE
#define IMX6ULL_GPT2_BASE           GPT2_BASE

#define IMX6ULL_ECSPI1_BASE         ECSPI1_BASE
#define IMX6ULL_ECSPI2_BASE         ECSPI2_BASE
#define IMX6ULL_ECSPI3_BASE         ECSPI3_BASE
#define IMX6ULL_ECSPI4_BASE         ECSPI4_BASE

#define IMX6ULL_I2C1_BASE           I2C1_BASE
#define IMX6ULL_I2C2_BASE           I2C2_BASE
#define IMX6ULL_I2C3_BASE           I2C3_BASE
#define IMX6ULL_I2C4_BASE           I2C4_BASE

#define IMX6ULL_SDMA_BASE           SDMAARM_BASE

#define IMX6ULL_USDHC1_BASE         USDHC1_BASE
#define IMX6ULL_USDHC2_BASE         USDHC2_BASE

#define IMX6ULL_SRC_BASE            SRC_BASE

#define IMX6ULL_GPMI_BASE           GPMI_BASE
#define IMX6ULL_BCH_BASE            BCH_BASE
#define IMX6ULL_APBH_BASE           APBH_BASE

#define IMX6ULL_CSI_BASE            CSI_BASE

#define IMX6ULL_CAN1_BASE           CAN1_BASE
#define IMX6ULL_CAN2_BASE           CAN2_BASE

#define IMX6ULL_USBPHY1_BASE        0x20C9000u
#define IMX6ULL_USBPHY2_BASE        0x20CA000u

#define IMX6ULL_USB1_BASE           0x2184000u
#define IMX6ULL_USB2_BASE           0x2184200u

#define IMX6ULL_USB_ANALOG_BASE     0x20C81A0u



/* ============================================================================
 * 3  Cortex-A GIC (Generic Interrupt Controller) Adapter
 * ============================================================================ */

#define __REG32(x)          (*((volatile unsigned int *)(x)))
#define GIC_IRQ_START       0
#define ARM_GIC_MAX_NR      1
#define ARM_GIC_NR_IRQS     PMU_IRQ2_IRQn + 1
#define MAX_HANDLERS        PMU_IRQ2_IRQn + 1

rt_inline rt_uint32_t platform_get_gic_dist_base(void)
{
    rt_uint32_t gic_base;
    asm volatile ("mrc p15, 4, %0, c15, c0, 0" : "=r"(gic_base));

    if (gic_base == 0) {
        return IMX6ULL_GIC_DIST_BASE;
    }

    return gic_base + kGICDBaseOffset;
}

rt_inline rt_uint32_t platform_get_gic_cpu_base(void)
{
    rt_uint32_t gic_base;
    asm volatile ("mrc p15, 4, %0, c15, c0, 0" : "=r"(gic_base));

    if (gic_base == 0) {
        return IMX6ULL_GIC_CPU_BASE;
    }

    return gic_base + kGICCBaseOffset;
}



/* ============================================================================
 * 3 i.MX6ULL SDK Adapter
 * ============================================================================ */

 typedef enum {
    CPU_0,
    CPU_1,
    CPU_2,
    CPU_3,
} cpuid_e;

typedef void (*irq_hdlr_t) (void);

extern rt_isr_handler_t rt_hw_interrupt_install(int vector, rt_isr_handler_t handler, void *param, const char *name);
extern void             rt_hw_interrupt_umask(int vector);
extern void             rt_hw_interrupt_mask(int vector);

rt_inline void register_interrupt_routine(uint32_t irq_id, irq_hdlr_t isr)
{
    rt_hw_interrupt_install(irq_id, (rt_isr_handler_t)isr, RT_NULL, "unknown");
}

rt_inline void enable_interrupt(uint32_t irq_id, uint32_t cpu_id, uint32_t priority)
{
    rt_hw_interrupt_umask(irq_id);
}

rt_inline void disable_interrupt(uint32_t irq_id, uint32_t cpu_id)
{
    rt_hw_interrupt_mask(irq_id);
}

#endif  /* __IMX6ULL_H__ */
