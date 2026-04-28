#ifndef __DRIVERS_NET_DRV_ETH_H__

#define __DRIVERS_NET_DRV_ETH_H__

#include "drv_common.h"
#include <netif/ethernetif.h>
#include "fsl_phy.h"

#define MAX_MAC_ADDR_LEN 6

struct imx6ul_enet_device
{
    /* 设备注册相关 */
    struct eth_device       parent;
    const char              *if_name;                       /* 设备名 */
    uint32_t                instance_id;                    /* 控制器实例号 */

    /* mac/phy 相关 */
    rt_uint8_t              mac_addr[MAX_MAC_ADDR_LEN];     /* mac 地址 */
    uint8_t                 phy_addr;                       /* phy 地址 */
    uint32_t                expected_phy_id1;               /* 期望读到的 phy id */

    /* 寄存器/中断相关 */
    ENET_Type               *enet_phy_base_addr;            /* 网络控制器基地址 */
    const char              *irq_name;                      /* 中断名称 */
    enum IRQn               irq_num;                        /* 中断号 */

    /* 链路状态 */
    rt_bool_t               phy_link_status;

    /* DMA/SDK 运行态 */
    enet_buffer_config_t    enet_buffer_config;             /* SDK buffer config */
    enet_config_t           enet_config;                    /* SDK config */
    enet_handle_t           enet_handle;                    /* SDK handle */

    /* 板级引脚/复位 */
    struct io_mux_pad_cfg   pinmux[9];                      /* 1个复位 RMII(4个发 4个收) 不包含mdio引脚 */
    GPIO_Type               *phy_reset_gpio_base;           /* 复位引脚所在的 GPIO 寄存器地址 */
    uint32_t                phy_reset_gpio_pin;             /* 复位引脚所在的 GPIO 引脚号 */
};


int32_t get_instance_by_base(void *base);

#endif
