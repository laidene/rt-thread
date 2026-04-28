/*
 * icm20608 6-axis imu (accel + gyro) driver and msh test
 */

#include "icm20608.h"
#include "spi/drv_spi.h"
#include <rtdevice.h>

#ifdef BSP_USING_SPI


static struct rt_spi_device *icm20608_dev;

#define ICM20608_SPI_READ_BIT  0x80

static rt_err_t _icm20608_read_reg(struct rt_spi_device *dev, rt_uint8_t reg, rt_uint8_t *buf, rt_uint8_t len)
{
    rt_uint8_t cmd = reg | ICM20608_SPI_READ_BIT;
    if (rt_spi_send_then_recv(dev, &cmd, 1, buf, len) != RT_EOK)
        return -RT_ERROR;
    return RT_EOK;
}

static rt_err_t _icm20608_write_reg(struct rt_spi_device *dev, rt_uint8_t reg, rt_uint8_t val)
{
    rt_uint8_t buf[2] = { reg, val };
    if (rt_spi_send(dev, buf, 2) != 2)
        return -RT_ERROR;
    return RT_EOK;
}




rt_err_t icm20608_init(const char *spi_bus_name)
{
    struct rt_spi_configuration cfg;
    rt_uint8_t id = 0;

    if (icm20608_dev != RT_NULL)
        return RT_EOK;

    imx6ull_spi_device_attach(spi_bus_name, ICM20608_DEVICE_NAME, (rt_uint32_t)ICM20608_CS_PIN);

    icm20608_dev = (struct rt_spi_device *)rt_device_find(ICM20608_DEVICE_NAME);
    if (icm20608_dev == RT_NULL)
        return -RT_ERROR;

    cfg.mode   = RT_SPI_MODE_0 | RT_SPI_MASTER | RT_SPI_MSB;
    cfg.data_width = 8;
    cfg.max_hz = 8000000;
    rt_spi_configure(icm20608_dev, &cfg);

    rt_thread_mdelay(10);

    if (_icm20608_write_reg(icm20608_dev, ICM20608_PWR_MGMT_1, 0x00) != RT_EOK)
        return -RT_ERROR;
    rt_thread_mdelay(50);

    if (icm20608_read_who_am_i(&id) != RT_EOK || id != ICM20608_WHO_AM_I_VAL)
        return -RT_ERROR;

    return RT_EOK;
}

rt_err_t icm20608_read_who_am_i(rt_uint8_t *id)
{
    if (icm20608_dev == RT_NULL || id == RT_NULL)
        return -RT_ERROR;
    return _icm20608_read_reg(icm20608_dev, ICM20608_WHO_AM_I, id, 1);
}

rt_err_t icm20608_read_accel_gyro(rt_int16_t accel[3], rt_int16_t gyro[3])
{
    rt_uint8_t buf[12];

    if (icm20608_dev == RT_NULL)
        return -RT_ERROR;

    if (_icm20608_read_reg(icm20608_dev, ICM20608_ACCEL_XOUT_H, buf, 6) != RT_EOK)
        return -RT_ERROR;
    if (accel) {
        accel[0] = (rt_int16_t)((rt_uint16_t)buf[0] << 8 | buf[1]);
        accel[1] = (rt_int16_t)((rt_uint16_t)buf[2] << 8 | buf[3]);
        accel[2] = (rt_int16_t)((rt_uint16_t)buf[4] << 8 | buf[5]);
    }

    if (_icm20608_read_reg(icm20608_dev, ICM20608_GYRO_XOUT_H, buf, 6) != RT_EOK)
        return -RT_ERROR;
    if (gyro) {
        gyro[0] = (rt_int16_t)((rt_uint16_t)buf[0] << 8 | buf[1]);
        gyro[1] = (rt_int16_t)((rt_uint16_t)buf[2] << 8 | buf[3]);
        gyro[2] = (rt_int16_t)((rt_uint16_t)buf[4] << 8 | buf[5]);
    }

    return RT_EOK;
}



#endif
