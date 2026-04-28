#ifndef __AP3216C_H__
#define __AP3216C_H__

#include <rtthread.h>
#include <rtdevice.h>

/**
 *  IR  = infrared
 *  ALS = ambient light sensor
 *  PSD = proximity sensor detector
 */

#define AP3216C_ADDR            0x1e

#define AP3216C_SYS_CFG         0x00

#define AP3216C_INT_STATUS      0x01
#define AP3216C_INT_CLEAR       0x02

#define AP3216C_IR_DATA_LOW     0x0A
#define AP3216C_IR_DATA_HIGH    0x0B

#define AP3216C_ALS_DATA_LOW    0x0C
#define AP3216C_ALS_DATA_HIGH   0x0D

#define AP3216C_PSD_DATA_LOW    0x0E
#define AP3216C_PSD_DATA_HIGH   0x0F


rt_err_t ap3216c_init(const char *name);
rt_err_t ap3216c_read_data(rt_uint16_t *ir, rt_uint16_t *als, rt_uint16_t *ps);

#endif
