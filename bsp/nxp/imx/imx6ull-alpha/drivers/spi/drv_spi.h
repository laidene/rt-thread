#ifndef __DRIVERS_SPI_DRV_SPI_H__

#define __DRIVERS_SPI_DRV_SPI_H__

#include "drv_common.h"
#include <drivers/dev_spi.h>
#include "fsl_ecspi.h"


struct imx6ull_spi_config {
    char *name;

    ECSPI_Type *ECSPI;                  /* spi base addr */
    rt_uint32_t clk_ip_name;            /* spi clock, eg: kCLOCK_Ecspx */

    struct io_mux_pad_cfg clk_gpio;
    struct io_mux_pad_cfg miso_gpio;
    struct io_mux_pad_cfg mosi_gpio;
};

struct imx6ull_spi_bus {
    struct rt_spi_bus           parent;
    struct imx6ull_spi_config*  config;
};


#ifdef BSP_USING_SPI1

    #define SPI1_BUS_CONFIG                                                 \
        {                                                                   \
            .ECSPI       = ECSPI1,                                          \
            .name        = "spi1",                                          \
            .clk_ip_name = kCLOCK_Ecspi1,                                   \
            .clk_gpio    = {IOMUXC_CSI_DATA04_ECSPI1_SCLK, 0, 0X10B1},      \
            .miso_gpio   = {IOMUXC_CSI_DATA07_ECSPI1_MISO, 0, 0X10B1},      \
            .mosi_gpio   = {IOMUXC_CSI_DATA06_ECSPI1_MOSI, 0, 0X10B1},      \
        }

#endif /* BSP_USING_SPI1 */


#ifdef BSP_USING_SPI2

    #define SPI2_BUS_CONFIG                                                 \
        {                                                                   \
            .ECSPI       = ECSPI2,                                          \
            .name        = "spi2",                                          \
            .clk_ip_name = kCLOCK_Ecspi2,                                   \
            .clk_gpio    = {IOMUXC_UART4_TX_DATA_ECSPI2_SCLK, 0, 0X10B1},   \
            .miso_gpio   = {IOMUXC_UART5_RX_DATA_ECSPI2_MISO, 0, 0X10B1},   \
            .mosi_gpio   = {IOMUXC_UART5_TX_DATA_ECSPI2_MOSI, 0, 0X10B1},   \
        }

#endif /* BSP_USING_SPI2 */


#ifdef BSP_USING_SPI3

    #define SPI3_BUS_CONFIG                                                 \
        {                                                                   \
            .ECSPI       = ECSPI3,                                          \
            .name        = "spi3",                                          \
            .clk_ip_name = kCLOCK_Ecspi3,                                   \
            .clk_gpio    = {IOMUXC_UART2_RX_DATA_ECSPI3_SCLK, 0, 0X10B1},   \
            .miso_gpio   = {IOMUXC_UART2_RTS_B_ECSPI3_MISO,   0, 0X10B1},   \
            .mosi_gpio   = {IOMUXC_UART2_CTS_B_ECSPI3_MOSI,   0, 0X10B1},   \
        }

#endif /* BSP_USING_SPI3 */


#ifdef BSP_USING_SPI4

    #define SPI4_BUS_CONFIG                                                 \
        {                                                                   \
            .ECSPI       = ECSPI4,                                          \
            .name        = "spi4",                                          \
            .clk_ip_name = kCLOCK_Ecspi4,                                   \
            .clk_gpio    = {IOMUXC_ENET2_TX_DATA1_ECSPI4_SCLK, 0, 0X10B1},  \
            .miso_gpio   = {IOMUXC_ENET2_TX_CLK_ECSPI4_MISO,   0, 0X10B1},  \
            .mosi_gpio   = {IOMUXC_ENET2_TX_EN_ECSPI4_MOSI,    0, 0X10B1},  \
        }

#endif /* BSP_USING_SPI4 */



rt_err_t imx6ull_spi_device_attach(const char* bus_name, const char* device_name, rt_uint32_t pin);


#endif