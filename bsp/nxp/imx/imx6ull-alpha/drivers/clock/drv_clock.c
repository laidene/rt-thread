#include "drv_common.h"

#define DBG_TAG "imx6ull.clk"
#define DBG_LVL DBG_WARNING
#include <rtdbg.h>


#define _K_GPT_LOAD_VALUE RT_UINT32_MAX

static uint32_t *s_gpt1_vbase         = (uint32_t*)IMX6ULL_GPT1_BASE;


static void s_clk_enable( CCM_Type *base )
{
    base->CCGR0 = 0XFFFFFFFF;
    base->CCGR1 = 0XFFFFFFFF;
    base->CCGR2 = 0XFFFFFFFF;
    base->CCGR3 = 0XFFFFFFFF;
    base->CCGR4 = 0XFFFFFFFF;
    base->CCGR5 = 0XFFFFFFFF;
    base->CCGR6 = 0XFFFFFFFF;
}


/**
 * @brief init clock tree
 * 
 */
static void s_board_boot_clock_run(void)
{
    rt_uint32_t reg_value;

    /* Boot ROM did initialize the XTAL, here we only sets external XTAL OSC freq */
    CLOCK_SetXtalFreq(24000000U);
    CLOCK_SetRtcXtalFreq(32768U);

    /*
     * ARM_CLK from 'pll1_sw_clk', whitch from 'pll1_main_clk' or 'step_clk'
     * if edit 'pll1_main_clk', switch to 'step_clk' first
     */
    reg_value = CCM->CCSR;
    if (0 == (reg_value & CCM_CCSR_PLL1_SW_CLK_SEL_MASK)) //if sel 'pll1_main_clk'
    {
        reg_value &= ~CCM_CCSR_STEP_SEL_MASK;
        reg_value |=  CCM_CCSR_STEP_SEL(0); //sel 'osc_clk(24M)'
        reg_value |=  CCM_CCSR_PLL1_SW_CLK_SEL(1); //sel 'step_clk'
        CCM->CCSR  =  reg_value;
    }

    /*
     * set PLL1(ARM PLL) at 1056MHz
     * set ARM_CLK at 528MHz
     * PLL output frequency = Fref * DIV_SEL / 2 = 24M * 88 / 2 = 1056M
     */
    CCM_ANALOG->PLL_ARM = CCM_ANALOG_PLL_ARM_ENABLE(1) | CCM_ANALOG_PLL_ARM_DIV_SELECT(88);

    reg_value  =  CCM->CCSR;
    reg_value &= ~CCM_CCSR_PLL1_SW_CLK_SEL_MASK;
    reg_value |=  CCM_CCSR_PLL1_SW_CLK_SEL(0); // resel 'pll1_main_clk'
    CCM->CCSR  =  reg_value;

    CCM->CACRR = CCM_CACRR_ARM_PODF(1); //'CACRR[ARM_PODF]=0b001' divide by 2



    /*
     * set PLL2(System PLL) at fixed 528MHz
     * PLL2_PFD0: 528M * 18 / FRAC
     * PLL2_PFD1: 528M * 18 / FRAC
     * PLL2_PFD2: 528M * 18 / FRAC 396MHz
     * PLL2_PFD3: 528M * 18 / FRAC
     */
    reg_value  =  CCM_ANALOG->PFD_528;
    reg_value &= ~0x3F3F3F3F;
    reg_value |=  CCM_ANALOG_PFD_528_SET_PFD0_FRAC(27);     //27: 352MHz
    reg_value |=  CCM_ANALOG_PFD_528_SET_PFD1_FRAC(16);     //16: 594MHz
    reg_value |=  CCM_ANALOG_PFD_528_SET_PFD2_FRAC(24);     //24: 396MHz
    reg_value |=  CCM_ANALOG_PFD_528_SET_PFD3_FRAC(32);     //32: 297MHz
    CCM_ANALOG->PFD_528 = reg_value;



    /*
     * set PLL3(USB  PLL) at fixed 480MHz
     * PLL3_PFD0: 480M * 18 / FRAC
     * PLL3_PFD1: 480M * 18 / FRAC
     * PLL3_PFD2: 480M * 18 / FRAC
     * PLL3_PFD3: 480M * 18 / FRAC
     */
    reg_value  =  CCM_ANALOG->PFD_480;
    reg_value &=  ~0x3F3F3F3F;
    reg_value |=  CCM_ANALOG_PFD_480_SET_PFD0_FRAC(12);     //12: 720MHz
    reg_value |=  CCM_ANALOG_PFD_480_SET_PFD1_FRAC(16);     //16: 540MHz
    reg_value |=  CCM_ANALOG_PFD_480_SET_PFD2_FRAC(17);     //17: 508.24MHz
    reg_value |=  CCM_ANALOG_PFD_480_SET_PFD3_FRAC(19);     //19: 457.74MHz
    CCM_ANALOG->PFD_480 = reg_value;


    /* set AHB_CLK at 132MHz for PLL2_PFD2(set AHB_PODF to 2) / 3 */
    CLOCK_SetDiv(kCLOCK_AhbDiv, 2);


    /*
     * set PERCLK_CLK at 66MHz from IPG_CLK
     */
    reg_value  =  CCM->CSCMR1;
    reg_value &= ~CCM_CSCMR1_PERCLK_CLK_SEL_MASK;
    reg_value |=  CCM_CSCMR1_PERCLK_CLK_SEL(0); //sel IPG_CLK
    reg_value &= ~CCM_CSCMR1_PERCLK_PODF_MASK;
    reg_value |=  CCM_CSCMR1_PERCLK_PODF(0); //'CSCMR1[PERCLK_PODF]=0b000000' divide by 1
    CCM->CSCMR1 = reg_value;


    CLOCK_DeinitAudioPll();
    CLOCK_DeinitVideoPll();
    CLOCK_DeinitEnetPll();

    /* Configure UART divider to default */
    CLOCK_SetMux(kCLOCK_UartMux, 0); /* Set UART source to PLL3 80M */
    CLOCK_SetDiv(kCLOCK_UartDiv, 0); /* Set UART divider to 1 */



    /* Configure ECSPI divider to default */
    CLOCK_SetMux(kCLOCK_EcspiMux, 0);           /* Set ECSPI source to PLL3 60M */
    CLOCK_SetDiv(kCLOCK_EcspiDiv, 0);           /* Set ECSPI divider to 1 */



    /* Set LCDIF_PRED. */
    CLOCK_SetDiv(kCLOCK_Lcdif1PreDiv, 2);
    /* Set LCDIF_CLK_PODF. */
    CLOCK_SetDiv(kCLOCK_Lcdif1Div, 4);
    /* Set Lcdif pre clock source. */
    CLOCK_SetMux(kCLOCK_Lcdif1PreMux, 2);
    CLOCK_SetMux(kCLOCK_Lcdif1Mux, 0);

}

/**
 * @brief init GPT1 Clock to 66MHz/66 = 1MHz
 * @param  
 */
static void s_board_delay_init(void)
{
    GPT_Type *_GPT = (GPT_Type*)s_gpt1_vbase;

    _GPT->CR = 0;

    _GPT->CR = GPT_CR_SWR(1);
    while (_GPT->CR & GPT_CR_SWR_MASK);

    /*
     * 000 No clock
     * 001 derive clock from ipg_clk
     * 010 derive clock from ipg_clk_highfreq
     * 011 derive clock from External Clock
     * 100 derive clock from ipg_clk_32k
     * 101 derive clock from ipg_clk_24M
     */
    _GPT->CR = GPT_CR_CLKSRC(0x1);

    _GPT->PR = GPT_PR_PRESCALER(65); //Set GPT1 Clock to 66MHz/66 = 1MHz

    _GPT->OCR[0] = GPT_OCR_COMP(_K_GPT_LOAD_VALUE);

    _GPT->CR |= GPT_CR_EN(1);
}



/**
 * @brief Init the system clock
 * 
 *      1 set the PLL and PFD frequeny
 *      2 set the GPT1 peripheral clock to 1MHz
 * @param  
 */
void system_clock_init(void)
{
    s_board_boot_clock_run();
    s_board_delay_init();

    s_clk_enable(CCM);
}



void rt_hw_us_delay(uint32_t us)
{
    GPT_Type *_GPT = (GPT_Type *)s_gpt1_vbase;

    rt_uint64_t old_cnt, new_cnt;
    rt_uint64_t total = 0;

    old_cnt = _GPT->CNT;
    while (1) {
        new_cnt = _GPT->CNT;
        if (old_cnt != new_cnt) {
            if (new_cnt > old_cnt) {
                total += (new_cnt - old_cnt);
            } else {
                total += (new_cnt + _K_GPT_LOAD_VALUE - old_cnt);
            }
            old_cnt = new_cnt;

            if (total >= us)
                break;
        }
    }
}

void rt_hw_ms_delay(uint32_t ms)
{
    while (ms--)
    {
        rt_hw_us_delay(1000);
    }
}
