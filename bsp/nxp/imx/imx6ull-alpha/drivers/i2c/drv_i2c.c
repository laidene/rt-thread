#include "drv_i2c.h"
#include "drv_common.h"
#include "fsl_i2c.h"


#define DBG_TAG "imx6ull.i2c"
#define DBG_LVL DBG_LOG       /* DBG_LOG DBG_INFO DBG_WARNING DBG_ERROR */
#include <rtdbg.h>


#if !defined(BSP_USING_I2C1) && !defined(BSP_USING_I2C2) && !defined(BSP_USING_I2C3) && !defined(BSP_USING_I2C4)
    #error "Please define at least one BSP_USING_I2Cx"
#endif



static struct imx6ull_i2c_config i2c_config[] = {
#ifdef BSP_USING_I2C1
    I2C1_BUS_CONFIG,
#endif

#ifdef BSP_USING_I2C2
    I2C2_BUS_CONFIG,
#endif

#ifdef BSP_USING_I2C3
    I2C3_BUS_CONFIG,
#endif

#ifdef BSP_USING_I2C4
    I2C4_BUS_CONFIG,
#endif
};


static struct imx6ull_i2c_bus_device i2c_bus_devices[sizeof(i2c_config) / sizeof(i2c_config[0])];
static char                          i2c_buff_temp[4][1024];




extern uint32_t I2C_GetInstance(I2C_Type *base);


/******************************************** rt i2c ops ***********************************************************/

rt_ssize_t imx6ull_i2c_master_xfer(struct rt_i2c_bus_device *bus, struct rt_i2c_msg msgs[], rt_uint32_t num)
{
    uint32_t instance = 0;
    struct imx6ull_i2c_bus_device* i2c_bus_device = RT_NULL;
    static i2c_master_transfer_t xfer = {0};
    
    RT_ASSERT(bus != RT_NULL);

    i2c_bus_device = (struct imx6ull_i2c_bus_device*)bus;

    instance = I2C_GetInstance(i2c_bus_device->config->hw_base);
    for(int i = 0; i < num; i++) {
        if(msgs[i].flags & RT_I2C_RD) {
            xfer.flags          = kI2C_TransferDefaultFlag;
            xfer.slaveAddress   = msgs[i].addr;
            xfer.direction      = kI2C_Read;
            xfer.subaddress     = 0;
            xfer.subaddressSize = 0;
            xfer.data           = (uint8_t *volatile)i2c_buff_temp[instance - 1];
            xfer.dataSize       = msgs[i].len;

            I2C_MasterTransferBlocking(i2c_bus_device->config->I2C, &xfer);

            rt_memcpy(msgs[i].buf, i2c_buff_temp[instance - 1], msgs[i].len);
        } else {
            xfer.flags          = kI2C_TransferDefaultFlag;
            xfer.slaveAddress   = msgs[i].addr;
            xfer.direction      = kI2C_Write;
            xfer.subaddress     = 0;
            xfer.subaddressSize = 0;
            xfer.data           = (uint8_t *volatile)i2c_buff_temp[instance - 1];
            xfer.dataSize       = msgs[i].len;
            rt_memcpy(i2c_buff_temp[instance - 1], msgs[i].buf, msgs[i].len);

            I2C_MasterTransferBlocking(i2c_bus_device->config->I2C, &xfer);
        }
    }

    return num;
}

rt_err_t imx6ull_i2c_bus_control(struct rt_i2c_bus_device *bus, int cmd, void *args)
{
    return RT_EOK;
}

#ifdef RT_USING_DEVICE_OPS
static const struct rt_i2c_bus_device_ops imx6ull_i2c_ops = {
    .master_xfer     = imx6ull_i2c_master_xfer,
    .slave_xfer      = RT_NULL,
    .i2c_bus_control = imx6ull_i2c_bus_control,
};
#endif

/******************************************** rt i2c ops ***********************************************************/





/**
 * @brief initialize the i2c bus
 *   1 iomux iopad
 *   2 clk enable
 *   3 i2c config
 *   4 register bus
 * @param  
 * @return 
 */
int rt_hw_i2c_init(void)
{
    rt_uint16_t         i2c_bus_device_num = 0;
    rt_uint32_t         src_clock          = 0;
    i2c_master_config_t master_config      = {0};


    i2c_bus_device_num = sizeof(i2c_config) / sizeof(i2c_config[0]);

    /* IPG_CLK_ROOT / (CSCMR1[PERCLK_PODF] + 1) /  */
    src_clock =  CLOCK_GetFreq(kCLOCK_IpgClk) / ( CLOCK_GetDiv(kCLOCK_PerclkDiv)  + 1U );
    LOG_D("I2C clock: %d", src_clock);

    for(int i = 0; i < i2c_bus_device_num; i++) {
        i2c_bus_devices[i].config = &i2c_config[i];
        i2c_bus_devices[i].config->hw_base = i2c_bus_devices[i].config->I2C;

        /* iomux */
        io_mux_pad_init(&i2c_bus_devices[i].config->scl_gpio);
        io_mux_pad_init(&i2c_bus_devices[i].config->sda_gpio);

        /* clk */
        CLOCK_EnableClock(i2c_bus_devices[i].config->clk_ip_name);

        /* i2c ctrl */
        I2C_MasterGetDefaultConfig(&master_config);
        master_config.baudRate_Bps = i2c_bus_devices[i].config->baud_rate;
        I2C_MasterInit(i2c_bus_devices[i].config->I2C, &master_config, src_clock);

        /* rt i2c framework */
        i2c_bus_devices[i].parent.ops = &imx6ull_i2c_ops;
        rt_i2c_bus_device_register(&i2c_bus_devices[i].parent, i2c_bus_devices[i].config->name);
    }

    return RT_EOK;
}
INIT_DEVICE_EXPORT(rt_hw_i2c_init);
