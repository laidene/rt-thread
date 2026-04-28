#include "drv_common.h"



#define IRQ_SECURE_PHY_TIMER    29                          /* Secure physical timer event */
#define IRQ_NOSECURE_PHY_TIMER  30                          /* No-Secure physical timer event */
#define IRQ_SYS_TICK            IRQ_SECURE_PHY_TIMER

#define SC_CNTCR_ENABLE     (1 << 0)
#define SC_CNTCR_HDBG       (1 << 1)
#define SC_CNTCR_FREQ0      (1 << 8)
#define SC_CNTCR_FREQ1      (1 << 9)


#define isb() __asm__ __volatile__ ("isb" : : : "memory")
#define dsb() __asm__ __volatile__ ("dsb" : : : "memory")
#define dmb() __asm__ __volatile__ ("dmb" : : : "memory")

#define SCTR_BASE_ADDR          0x021DC000
#define CONFIG_SC_TIMER_CLK     8000000


static int g_sys_freq;
#define TICK_PERIOD (g_sys_freq / RT_TICK_PER_SECOND)


/* System Counter */
struct sctr_regs {
    rt_uint32_t cntcr;
    rt_uint32_t cntsr;
    rt_uint32_t cntcv1;
    rt_uint32_t cntcv2;
    rt_uint32_t resv1[4];
    rt_uint32_t cntfid0;
    rt_uint32_t cntfid1;
    rt_uint32_t cntfid2;
    rt_uint32_t resv2[1001];
    rt_uint32_t counterid[1];
};



/************************************* system timer register ops *************************************************/

static inline void enable_cntp(void)
{
    rt_uint32_t cntv_ctl;
    cntv_ctl = 1;
    asm volatile ("mcr p15, 0, %0, c14, c2, 1" :: "r"(cntv_ctl));
    isb();
}
static inline void disable_cntp(void)
{
    rt_uint32_t cntv_ctl;
    cntv_ctl = 0;
    asm volatile ("mcr p15, 0, %0, c14, c2, 1" :: "r"(cntv_ctl));
    isb();
}

static inline rt_uint32_t read_cntfrq(void)
{
    rt_uint32_t val;
    asm volatile ("mrc p15, 0, %0, c14, c0, 0" : "=r"(val));
    return val;
}

static inline void write_cntp_tval(rt_uint32_t val)
{
    asm volatile ("mcr p15, 0, %0, c14, c2, 0" :: "r"(val));
    isb();
    return;
}


static inline void        write_cntp_cval(rt_uint64_t val)
{
    asm volatile ("mcrr p15, 2, %Q0, %R0, c14" :: "r" (val));
    isb();
    return;
}
static inline rt_uint64_t  read_cntp_cval(void)
{
    rt_uint64_t val;
    asm volatile ("mrrc p15, 2, %Q0, %R0, c14" : "=r" (val));
    return (val);
}

/************************************* system timer register ops *************************************************/




/************************************* system counter register ops *************************************************/


static void s_imx6ull_enable_clk_in_waitmode(void)
{
    volatile unsigned int *CCM_CLPCR;
    CCM_CLPCR = (void*)0x20C4054;                   /* CCM_CLPCR */
    *CCM_CLPCR &= ~((1 << 5) | 0x3);                /* LPM=0 ARM_CLK_DIS_ON_LPM=0 */
}

static void s_system_counter_init(void)
{
    /* imx6ull, enable system counter */

    unsigned long val, freq;
    struct sctr_regs *sctr = (struct sctr_regs *)SCTR_BASE_ADDR;

    freq = CONFIG_SC_TIMER_CLK;
    asm volatile("mcr p15, 0, %0, c14, c0, 0" : : "r" (freq));

    sctr->cntfid0 = freq;

    /* Enable system counter */
    val = sctr->cntcr;
    val &= ~(SC_CNTCR_FREQ0 | SC_CNTCR_FREQ1);
    val |= SC_CNTCR_FREQ0 | SC_CNTCR_ENABLE | SC_CNTCR_HDBG;
    sctr->cntcr = val;

    s_imx6ull_enable_clk_in_waitmode();
}

/************************************* system counter register ops *************************************************/




static void s_arch_timer_init(void)
{
    g_sys_freq = read_cntfrq();

    disable_cntp();
    write_cntp_tval(TICK_PERIOD);
    enable_cntp();
}




static void s_rt_hw_timer_isr(int vector, void *param)
{
    rt_tick_increase();

    /* restart timer */
    disable_cntp();
    write_cntp_cval(read_cntp_cval() + TICK_PERIOD);
    enable_cntp();
}




int rt_hw_timer_init(void)
{
    // s_system_counter_init(); // [qemu] x

    s_arch_timer_init();

    rt_hw_interrupt_install(IRQ_SYS_TICK, s_rt_hw_timer_isr, RT_NULL, "tick");
    rt_hw_interrupt_umask(IRQ_SYS_TICK);

    return 0;
}
INIT_BOARD_EXPORT(rt_hw_timer_init);
 
 
