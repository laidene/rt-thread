#include "drv_common.h"


static void _print_all_pll_freq(void)
{
    uint32_t freq;

    freq = CLOCK_GetPllFreq(kCLOCK_PllArm);
    rt_kprintf("PLL1-ARM PLL: %u MHz\n", freq / 1000000);

    freq = CLOCK_GetPllFreq(kCLOCK_PllSys);
    rt_kprintf("PLL2-System PLL: %u MHz\n", freq / 1000000);

    freq = CLOCK_GetPllFreq(kCLOCK_PllUsb1);
    rt_kprintf("PLL3-USB1: %u MHz\n", freq / 1000000);

    freq = CLOCK_GetPllFreq(kCLOCK_PllAudio);
    rt_kprintf("PLL4-Audio PLL: %u MHz\n", freq / 1000000);

    freq = CLOCK_GetPllFreq(kCLOCK_PllVideo);
    rt_kprintf("PLL5-Video PLL: %u MHz\n", freq / 1000000);

    freq = CLOCK_GetPllFreq(kCLOCK_PllEnet0);
    rt_kprintf("PLL6-ENET PLL ETH0: %u MHz\n", freq / 1000000);
    freq = CLOCK_GetPllFreq(kCLOCK_PllEnet1);
    rt_kprintf("PLL6-ENET PLL ETH1: %u MHz\n", freq / 1000000);
    freq = CLOCK_GetPllFreq(kCLOCK_PllEnet2);
    rt_kprintf("PLL6-ENET PLL ETH2: %u MHz\n", freq / 1000000);

    freq = CLOCK_GetPllFreq(kCLOCK_PllUsb2);
    rt_kprintf("PLL7-USB2: %u MHz\n", freq / 1000000);



    freq = CLOCK_GetSysPfdFreq(kCLOCK_Pfd0);
    rt_kprintf("528PLL PFD0: %u MHz\n", freq / 1000000);


    freq = CLOCK_GetSysPfdFreq(kCLOCK_Pfd1);
    rt_kprintf("528PLL PFD1: %u MHz\n", freq / 1000000);

    freq = CLOCK_GetSysPfdFreq(kCLOCK_Pfd2);
    rt_kprintf("528PLL PFD2: %u MHz\n", freq / 1000000);


    freq = CLOCK_GetSysPfdFreq(kCLOCK_Pfd3);
    rt_kprintf("528PLL PFD3: %u MHz\n", freq / 1000000);



    freq = CLOCK_GetUsb1PfdFreq(kCLOCK_Pfd0);
    rt_kprintf("USB1_PLL PFD0: %u MHz\n", freq / 1000000);


    freq = CLOCK_GetUsb1PfdFreq(kCLOCK_Pfd1);
    rt_kprintf("USB1_PLL PFD1: %u MHz\n", freq / 1000000);

    freq = CLOCK_GetUsb1PfdFreq(kCLOCK_Pfd2);
    rt_kprintf("USB1_PLL PFD2: %u MHz\n", freq / 1000000);

    freq = CLOCK_GetUsb1PfdFreq(kCLOCK_Pfd3);
    rt_kprintf("USB1_PLL PFD3: %u MHz\n", freq / 1000000);



    /* CPU clock Switcher clock generation */
    rt_kprintf("CPU Clock:    %u MHz \n", CLOCK_GetFreq(kCLOCK_CpuClk) / 1000000);
    rt_kprintf("AXI Clock:    %u MHz \n", CLOCK_GetFreq(kCLOCK_AxiClk) / 1000000);  /* System PLL PFD2 / 2 */
    rt_kprintf("AHB Clock:    %u MHz \n", CLOCK_GetFreq(kCLOCK_AhbClk) / 1000000);  /* System PLL PFD2 / 4 */
    rt_kprintf("IPG Clock:    %u MHz \n", CLOCK_GetFreq(kCLOCK_IpgClk) / 1000000);  /* AHB Clock / 8 */

}

MSH_CMD_EXPORT_ALIAS(_print_all_pll_freq, print_all_pll_freq, print all pll frequency);
