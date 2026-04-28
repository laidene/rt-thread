#include <rthw.h>
#include <rtdevice.h>
#include <mmu.h>
#include <ioremap.h>


#include "drv_common.h"


void io_mux_pad_init(const struct io_mux_pad_cfg *cfg)
{
    if ((cfg == RT_NULL) || (cfg->mux_reg == 0U)) {
        return;
    }

    IOMUXC_SetPinMux   (cfg->mux_reg, cfg->mux_mode, cfg->input_reg, cfg->input_daisy, cfg->pad_reg, cfg->mux_sion);

    IOMUXC_SetPinConfig(cfg->mux_reg, cfg->mux_mode, cfg->input_reg, cfg->input_daisy, cfg->pad_reg, cfg->pad_val);
}
