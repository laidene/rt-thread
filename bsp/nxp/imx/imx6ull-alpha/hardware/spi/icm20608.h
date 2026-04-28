#ifndef __HARD_WARE_SPI_ICM20608_H__
#define __HARD_WARE_SPI_ICM20608_H__

#include <rtthread.h>
#include "gpio/drv_gpio.h"

#define ICM20608_CS_PIN       GET_PIN(1, 20)
#define ICM20608_SPI_BUS_NAME "spi1"
#define ICM20608_DEVICE_NAME  "icm20608"

#define ICM20608_WHO_AM_I     0x75
#define ICM20608_WHO_AM_I_VAL 0xAF

#define ICM20608_PWR_MGMT_1   0x6B
#define ICM20608_ACCEL_XOUT_H 0x3B
#define ICM20608_GYRO_XOUT_H  0x43

rt_err_t icm20608_init(const char *spi_bus_name);
rt_err_t icm20608_read_who_am_i(rt_uint8_t *id);
rt_err_t icm20608_read_accel_gyro(rt_int16_t accel[3], rt_int16_t gyro[3]);

#endif