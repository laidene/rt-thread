#ifndef __DRV_I2C_H__
#define __DRV_I2C_H__

#include "drv_common.h"
#include <drivers/dev_i2c.h>


struct imx6ull_i2c_config {
    void*                 hw_base;
    I2C_Type*             I2C;
    char*                 name;
    rt_uint32_t           baud_rate;
    rt_uint32_t           clk_ip_name;
    rt_uint32_t           irq_num;

    struct io_mux_pad_cfg    scl_gpio;
    struct io_mux_pad_cfg    sda_gpio;

    i2c_master_handle_t master_handle;
};


struct imx6ull_i2c_bus_device {
    struct rt_i2c_bus_device    parent;
    struct imx6ull_i2c_config*  config;
};


#ifdef BSP_USING_I2C1

    #define I2C1_BUS_CONFIG                                             \
        {                                                               \
            .I2C         = I2C1,                                        \
            .name        = "i2c1",                                      \
            .clk_ip_name = kCLOCK_I2c1S,                                \
            .baud_rate   = I2C1_BAUD_RATE,                              \
            .irq_num     = I2C1_IRQn,                                   \
            .scl_gpio    = {IOMUXC_UART4_TX_DATA_I2C1_SCL, 1, 0x70B0},  \
            .sda_gpio    = {IOMUXC_UART4_RX_DATA_I2C1_SDA, 1, 0x70B0},  \
        }

#endif /* BSP_USING_I2C1 */

#ifdef BSP_USING_I2C2

    #define I2C2_BUS_CONFIG                                             \
        {                                                               \
            .I2C         = I2C2,                                        \
            .name        = "i2c2",                                      \
            .clk_ip_name = kCLOCK_I2c2S,                                \
            .baud_rate   = I2C2_BAUD_RATE,                              \
            .irq_num     = I2C2_IRQn,                                   \
            .scl_gpio    = {IOMUXC_UART5_TX_DATA_I2C2_SCL, 1, 0x70B0},  \
            .sda_gpio    = {IOMUXC_UART5_RX_DATA_I2C2_SDA, 1, 0x70B0},  \
        }

#endif /* BSP_USING_I2C2 */


#ifdef BSP_USING_I2C3

    #define I2C3_BUS_CONFIG                                             \
        {                                                               \
            .I2C         = I2C3,                                        \
            .name        = "i2c3",                                      \
            .clk_ip_name = kCLOCK_I2c3S,                                \
            .baud_rate   = I2C3_BAUD_RATE,                              \
            .irq_num     = I2C3_IRQn,                                   \
            .scl_gpio    = {IOMUXC_ENET2_RX_DATA0_I2C3_SCL, 1, 0x70B0}, \
            .sda_gpio    = {IOMUXC_ENET2_RX_DATA1_I2C3_SDA, 1, 0x70B0}, \
        }

#endif /* BSP_USING_I2C3 */

#ifdef BSP_USING_I2C4

    #define I2C4_BUS_CONFIG                                             \
        {                                                               \
            .I2C         = I2C4,                                        \
            .name        = "i2c4",                                      \
            .clk_ip_name = kCLOCK_I2c4S,                                \
            .baud_rate   = I2C4_BAUD_RATE,                              \
            .irq_num     = I2C4_IRQn,                                   \
            .scl_gpio    = {IOMUXC_UART2_TX_DATA_I2C4_SCL, 1, 0x70B0},  \
            .sda_gpio    = {IOMUXC_UART2_RX_DATA_I2C4_SDA, 1, 0x70B0},  \
        }

#endif /* BSP_USING_I2C4 */



#endif