#include "drv_spi.h"
#include <rtdevice.h>

#ifdef BSP_USING_SPI

#define DBG_TAG "imx6ull.spi"
#define DBG_LVL DBG_LOG       /* DBG_LOG DBG_INFO DBG_WARNING DBG_ERROR */
#include <rtdbg.h>


static struct imx6ull_spi_config _spi_config[] = {
#ifdef BSP_USING_SPI1
    SPI1_BUS_CONFIG,
#endif
#ifdef BSP_USING_SPI2
    SPI2_BUS_CONFIG,
#endif
#ifdef BSP_USING_SPI3
    SPI3_BUS_CONFIG,
#endif
#ifdef BSP_USING_SPI4
    SPI4_BUS_CONFIG,
#endif
};

static struct imx6ull_spi_bus _spi_bus[GET_ARRAY_NUM(_spi_config)];


/********************************** rt spi ops ***********************************************/

static rt_err_t _imx6ull_spi_configure(struct rt_spi_device *device, struct rt_spi_configuration *cfg)
{
    struct imx6ull_spi_bus* bus       = RT_NULL;
    ecspi_master_config_t   config    = {0};
    rt_uint32_t             scr_clock = 0;

    bus = (struct imx6ull_spi_bus*)device->bus->parent.user_data;

    ECSPI_MasterGetDefaultConfig(&config);

    config.samplePeriod = 10;
    config.txFifoThreshold = 0;
    config.channelConfig.dataLineInactiveState = kECSPI_DataLineInactiveStateHigh;

    if (cfg->data_width == 8) {
        config.burstLength = 8;
    }else {
        return -RT_EINVAL;
    }

    if (cfg->mode & RT_SPI_SLAVE) {
        config.channelConfig.channelMode = kECSPI_Slave;
    } else {
        config.channelConfig.channelMode = kECSPI_Master;
    }

    if(cfg->mode & RT_SPI_CPHA)
    {
        config.channelConfig.phase = kECSPI_ClockPhaseSecondEdge;
    }
    else
    {
        config.channelConfig.phase = kECSPI_ClockPhaseFirstEdge;
    }

    if (cfg->mode & RT_SPI_CPOL) {
        config.channelConfig.polarity = kECSPI_PolarityActiveLow;
    } else {
        config.channelConfig.polarity = kECSPI_PolarityActiveHigh;
    }

    config.baudRate_Bps = cfg->max_hz;

    scr_clock = (CLOCK_GetPllFreq(kCLOCK_PllUsb1) / 8U);
    ECSPI_MasterInit(bus->config->ECSPI, &config, scr_clock);

    return RT_EOK;
}

static rt_ssize_t _imx6ull_spi_xfer(struct rt_spi_device *device, struct rt_spi_message *message){
    struct imx6ull_spi_bus* imx_spi_bus = RT_NULL;
    rt_uint32_t pin = 0;

    const rt_uint8_t*   send_ptr = RT_NULL;
    rt_uint8_t*         recv_ptr = RT_NULL;
    rt_uint16_t         size     = 0;
    rt_uint8_t          temp_data;


    imx_spi_bus = (struct imx6ull_spi_bus*)device->bus->parent.user_data;
    pin         = (rt_uint32_t)device->parent.user_data;


    recv_ptr = (rt_uint8_t *)message->recv_buf;
    send_ptr = (rt_uint8_t *)message->send_buf;
    size     = message->length;

    if (message->cs_take && pin) {
        rt_pin_write(pin, PIN_LOW);
    }

    ECSPI_SetChannelSelect(imx_spi_bus->config->ECSPI, kECSPI_Channel0);
    while(size--) {
        temp_data = (send_ptr != RT_NULL) ? (*send_ptr++) : 0xff;

        while (!(imx_spi_bus->config->ECSPI->STATREG & ECSPI_STATREG_TE_MASK));
        ECSPI_WriteData(imx_spi_bus->config->ECSPI, temp_data);

        while (!(imx_spi_bus->config->ECSPI->STATREG & ECSPI_STATREG_RR_MASK));
        temp_data = ECSPI_ReadData(imx_spi_bus->config->ECSPI);

        if (recv_ptr != RT_NULL) {
            *recv_ptr++ = temp_data;
        }
    }


    if (message->cs_release && pin) {
        rt_pin_write(pin, PIN_HIGH);
    }

    return (rt_ssize_t)message->length;
}

static const struct rt_spi_ops imx6ull_spi_ops = {
    .configure = _imx6ull_spi_configure,
    .xfer      = _imx6ull_spi_xfer,
};

/********************************** rt spi ops ***********************************************/



rt_err_t imx6ull_spi_device_attach(const char* bus_name, const char* device_name, rt_uint32_t pin)
{
    rt_err_t ret = RT_EOK;

    rt_pin_mode(pin, PIN_MODE_OUTPUT);
    rt_pin_write(pin, PIN_HIGH);

    struct rt_spi_device *spi_device = (struct rt_spi_device *)rt_malloc(sizeof(struct rt_spi_device));
    RT_ASSERT(spi_device != RT_NULL);
    ret = rt_spi_bus_attach_device(spi_device, device_name, bus_name, (void*)pin);

    return ret;
}



static void imx6ull_spi_gpio_init(struct imx6ull_spi_bus* bus) 
{
    io_mux_pad_init(&bus->config->clk_gpio);
    io_mux_pad_init(&bus->config->miso_gpio);
    io_mux_pad_init(&bus->config->mosi_gpio);
}

int imx6ull_spi_init(void)
{
    rt_size_t bus_size = 0;
    bus_size = GET_ARRAY_NUM(_spi_config);

    for(int i = 0; i < bus_size; i++) {
        _spi_bus[i].config = &_spi_config[i];

        /* iomux */
        imx6ull_spi_gpio_init(&_spi_bus[i]);

        /* clk */
        CLOCK_EnableClock(_spi_bus[i].config->clk_ip_name);

        /* rt spi framework */
        _spi_bus[i].parent.parent.user_data = &_spi_bus[i];
        rt_spi_bus_register(&_spi_bus[i].parent, _spi_bus[i].config->name, &imx6ull_spi_ops);
    }

    return RT_EOK;
}


//INIT_DEVICE_EXPORT(imx6ull_spi_init); /* [qemu] x */

#endif