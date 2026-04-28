#include "drv_eth.h"
#include "fsl_enet.h"

#define DBG_TAG "imx6ull.eth"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>


#if (defined(RT_USING_ENET1)) || (defined(RT_USING_ENET2))


/* 管理总线引脚配置 */
static struct io_mux_pad_cfg _mdio_gpio[2] = {
    { IOMUXC_GPIO1_IO06_ENET1_MDIO, 0U, 0xB029 },
    { IOMUXC_GPIO1_IO07_ENET1_MDC, 0U, 0xB0E9 },
};

enum {
    #ifdef RT_USING_ENET1
    DEV_ENET1,
    #endif

    #ifdef RT_USING_ENET2
    DEV_ENET2,
    #endif

    DEV_ENET_MAX,
};


static struct imx6ul_enet_device _eth_dev[DEV_ENET_MAX] = {
    #ifdef RT_USING_ENET1
    {
        .parent         = (struct eth_device){0},
        .if_name        = "e1",
        .instance_id    = 1,

        .mac_addr           = {0xa8,0x5e,0x45,0x91,0x92,0x93},
        .phy_addr           = 2,
        .expected_phy_id1   = 7,

        .enet_phy_base_addr = ENET1,
        .irq_name           = "emac1_intr",
        .irq_num            = ENET1_IRQn,

        .enet_buffer_config = {
            ENET_RXBD_NUM,
            ENET_TXBD_NUM,
            ENET_RXBUFF_ALIGN_SIZE,
            ENET_TXBUFF_ALIGN_SIZE,
            RT_NULL,
            RT_NULL,
            RT_NULL,
            RT_NULL,
            RT_NULL,
            RT_NULL,
            RT_NULL,
            RT_NULL,
            ENET_RXBUFF_TOTAL_SIZE,
            ENET_TXBUFF_TOTAL_SIZE,
        },
        .enet_config = (enet_config_t){0},
        .enet_handle = (enet_handle_t){0},

        .pinmux = {
            {IOMUXC_SNVS_SNVS_TAMPER7_GPIO5_IO07,   0U,0x110B0},

            {IOMUXC_ENET1_RX_DATA0_ENET1_RDATA00,   0U,0xB0E9 },
            {IOMUXC_ENET1_RX_DATA1_ENET1_RDATA01,   0U,0xB0E9 },
            {IOMUXC_ENET1_RX_EN_ENET1_RX_EN,        0U,0xB0E9 },
            {IOMUXC_ENET1_RX_ER_ENET1_RX_ER,        0U,0xB0E9 },

            {IOMUXC_ENET1_TX_CLK_ENET1_REF_CLK1,    1U,0x00F0 },
            {IOMUXC_ENET1_TX_DATA0_ENET1_TDATA00,   0U,0xB0E9 },
            {IOMUXC_ENET1_TX_DATA1_ENET1_TDATA01,   0U,0xB0E9 },
            {IOMUXC_ENET1_TX_EN_ENET1_TX_EN,        0U,0xB0E9 },
        },
        .phy_reset_gpio_base = GPIO5,
        .phy_reset_gpio_pin = 7,
    },
    #endif

    #ifdef RT_USING_ENET2
    {
        .parent         =  (struct eth_device){0},
        .if_name        = "e2",
        .instance_id    = 2,

        .mac_addr           = {0xa8,0x5e,0x45,0x01,0x02,0x03},
        .phy_addr           = 1,
        .expected_phy_id1   = 7,

        .enet_phy_base_addr = ENET2,
        .irq_name           = "emac2_intr",
        .irq_num            = ENET2_IRQn,

        .enet_buffer_config = {
            ENET_RXBD_NUM,
            ENET_TXBD_NUM,
            ENET_RXBUFF_ALIGN_SIZE,
            ENET_TXBUFF_ALIGN_SIZE,
            RT_NULL,
            RT_NULL,
            RT_NULL,
            RT_NULL,
            RT_NULL,
            RT_NULL,
            RT_NULL,
            RT_NULL,
            ENET_RXBUFF_TOTAL_SIZE,
            ENET_TXBUFF_TOTAL_SIZE,
        },
        .enet_config = (enet_config_t){0},
        .enet_handle = (enet_handle_t){0},

        .pinmux = {
            {IOMUXC_SNVS_SNVS_TAMPER6_GPIO5_IO06,   0U, 0x110B0},

            {IOMUXC_ENET2_RX_DATA0_ENET2_RDATA00,   0U, 0xB0E9},
            {IOMUXC_ENET2_RX_DATA1_ENET2_RDATA01,   0U, 0xB0E9},
            {IOMUXC_ENET2_RX_EN_ENET2_RX_EN,        0U, 0xB0E9},
            {IOMUXC_ENET2_RX_ER_ENET2_RX_ER,        0U, 0xB0E9},

            {IOMUXC_ENET2_TX_CLK_ENET2_REF_CLK2,    1U, 0x00F0},
            {IOMUXC_ENET2_TX_DATA0_ENET2_TDATA00,   0U, 0xB0E9},
            {IOMUXC_ENET2_TX_DATA1_ENET2_TDATA01,   0U, 0xB0E9},
            {IOMUXC_ENET2_TX_EN_ENET2_TX_EN,        0U, 0xB0E9},
        },
        .phy_reset_gpio_base = GPIO5,
        .phy_reset_gpio_pin = 6,
    },
    #endif
};


/**********************************************************************************
 * @brief 更新PHY链接状态
 * 
 * 当PHY链接状态发生变化时调用该函数更新状态并通知上层网络框架。
 * 
 * @param[in] dev 线程入口参数，传入 imx6ul_enet_device 设备结构体指针
 * @param[in] up 链接状态，RT_TRUE表示链接 up，RT_FALSE表示链接 down
 * 
 **********************************************************************************/
void _imx6ul_eth_link_change(struct imx6ul_enet_device* dev, rt_bool_t up)
{
    if (up) {
        LOG_D("enet%d link up", dev->instance_id);
        eth_device_linkchange(&dev->parent, RT_TRUE);
        dev->phy_link_status = RT_TRUE;
    } else {
        LOG_D("enet%d link down", dev->instance_id);
        eth_device_linkchange(&dev->parent, RT_FALSE);
        dev->phy_link_status = RT_FALSE;
    }
}


/**********************************************************************************
 * @brief PHY链接检测线程入口函数
 * 
 * 该线程负责检测PHY的链接状态，并在状态变化时进行相应处理（如重新协商）。
 * 在设备初始化时创建，并持续运行以监控PHY状态。
 * 
 * @param[in] param 线程入口参数，传入 imx6ul_enet_device 设备结构体指针
 * 
 **********************************************************************************/
static void _phy_detect_thread_entry(void* param)
{
    bool                        link = false;
    phy_speed_t                 speed;
    phy_duplex_t                duplex;
    ENET_Type*                  base_addr = RT_NULL;
    struct imx6ul_enet_device   *dev = (struct imx6ul_enet_device*)param;

    
    base_addr = dev->enet_phy_base_addr;

    /* 硬件复位 */
    phy_reset(dev->phy_reset_gpio_base, dev->phy_reset_gpio_pin);
    rt_thread_delay(150);

    PHY_Init(base_addr, dev->phy_addr, SYS_CLOCK_HZ, dev->expected_phy_id1);

    PHY_GetLinkStatus(base_addr, dev->phy_addr, &link);

    if (link) {
        PHY_GetLinkSpeedDuplex(base_addr, dev->phy_addr, &speed, &duplex);
        dev->enet_config.miiSpeed  = (enet_mii_speed_t)speed;
        dev->enet_config.miiDuplex = (enet_mii_duplex_t)duplex;
    } else {
        LOG_W("PHY Link down, please check the cable connection and link partner setting.");
    }

    while (1) {
        PHY_GetLinkStatus(base_addr, dev->phy_addr, &link);
        if (link != dev->phy_link_status) {
            if (link == true) {
                PHY_StartNegotiation(base_addr, dev->phy_addr);
            }
            _imx6ul_eth_link_change(dev, link); /* todo */
        }
        rt_thread_delay(DETECT_DELAY_ONE_SECOND);
    }
}


void rx_enet_callback(void* base)
{
    int32_t instance = 0;
    instance = get_instance_by_base(base);
    if (instance == -1) {
        LOG_E("interrput match base addr error");
        return;
    }
    eth_device_ready(&(_eth_dev[instance].parent));
    ENET_DisableInterrupts(base, ENET_RX_INTERRUPT);
}

void tx_enet_callback(void* base)
{
    ENET_DisableInterrupts(base, ENET_TX_INTERRUPT);
}


int32_t get_instance_by_base(void* base)
{
    int32_t i = 0;
    int32_t instance = 0;
    for (i = 0; i < DEV_ENET_MAX; i++) {
        if ((void*)_eth_dev[i].phy_reset_gpio_base == base) {
            break;
        }
    }
    if (i == DEV_ENET_MAX) {
        return -1;
    }
    return instance;

    for (int i = 0; i < GET_ARRAY_NUM(_eth_dev); i++) {
        if ((void*)_eth_dev[i].phy_reset_gpio_base == base) {
            return _eth_dev[i].instance_id;
        }
    }
    return -1;
}



/**********************************************************************************
 * @brief 初始化EMAC设备并注册到RT-Thread设备框架中
 * 
 * @return 成功返回RT_EOK，失败返回-RT_ERROR
 **********************************************************************************/
static int _eth_init(void)
{
    rt_err_t state = RT_EOK;
    char link_detect[10];

    /* 初始化管理总线引脚 */
    io_mux_pad_init(&_mdio_gpio[0]);
    io_mux_pad_init(&_mdio_gpio[1]);


    for(int i = 0; i < GET_ARRAY_NUM(_eth_dev); i++) {
        struct imx6ul_enet_device* dev = &_eth_dev[i];

        dev->parent.parent.ops  = RT_NULL;/* todo */
        dev->parent.eth_rx      = RT_NULL; /* todo */
        dev->parent.eth_tx      = RT_NULL; /* todo */
        dev->phy_link_status    = RT_FALSE;


        /* rtt框架函数 */
        state = eth_device_init(&dev->parent, dev->if_name);
        if (RT_EOK == state) {
            LOG_I("emac device init success");
        } else {
            LOG_E("emac device init faild: %d", state);
            state = -RT_ERROR;
        }

        /* 创建PHY链接检测线程 */
        rt_sprintf(link_detect,"link_d%d",dev->instance_id);

        rt_thread_t phy_link_tid;
        phy_link_tid = rt_thread_create( link_detect,
                                         _phy_detect_thread_entry, /* todo */
                                         dev,
                                         4096,
                                         RT_THREAD_PRIORITY_MAX - 2,
                                         2
         );
        if (phy_link_tid != RT_NULL) {
            rt_thread_startup(phy_link_tid);
        }

        memset(link_detect,0,sizeof(link_detect));
    }

    return state;
}
MSH_CMD_EXPORT_ALIAS(_eth_init, eth_init, Initialize ENET device);

#endif /* (defined(RT_USING_ENET1)) || (defined(RT_USING_ENET2)) */