#ifndef __DRIVERS_SDIO_DRV_SDIO_H__

#define __DRIVERS_SDIO_DRV_SDIO_H__

#include "drv_common.h"


struct imx6ull_sdio_io_mux_pad_cfg {
    struct io_mux_pad_cfg io_cmd;
    struct io_mux_pad_cfg io_clk;

    struct io_mux_pad_cfg io_cd;
    struct io_mux_pad_cfg io_wp;
    struct io_mux_pad_cfg io_rst;

    struct io_mux_pad_cfg io_data0;
    struct io_mux_pad_cfg io_data1;
    struct io_mux_pad_cfg io_data2;
    struct io_mux_pad_cfg io_data3;
    struct io_mux_pad_cfg io_data4;
    struct io_mux_pad_cfg io_data5;
    struct io_mux_pad_cfg io_data6;
    struct io_mux_pad_cfg io_data7;
};

struct imx6ull_sdio_cfg {
    void*       base;
    rt_uint32_t clk_ip_name;
    clock_div_t usdhc_div;

    struct imx6ull_sdio_io_mux_pad_cfg io_cfg;
};

struct imx6ull_sdio_device {
    struct rt_mmcsd_host*       host;
    struct imx6ull_sdio_cfg*    cfg;
    usdhc_host_t                usdhc_host;
    uint32_t*                   usdhc_adma2_table;
};

#endif
