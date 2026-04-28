#include <rtthread.h>
#include <rtdevice.h>
#include "ap3216c.h"


static struct rt_i2c_bus_device* s_ap3216c_i2c_bus     = RT_NULL;


/************************************* i2c r/w ops ******************************************************/

static rt_err_t _ap3216c_write_reg(struct rt_i2c_bus_device *bus, rt_uint8_t reg, rt_uint8_t data)
{
    rt_uint8_t buf[2];
    struct rt_i2c_msg msgs;

    buf[0] = reg;   /* 寄存器地址 */
    buf[1] = data;  /* 要写入的数据 */

    msgs.addr  = AP3216C_ADDR;
    msgs.flags = RT_I2C_WR;
    msgs.buf   = buf;
    msgs.len   = 2;

    /* 调用I2C设备接口传输数据 */
    if (rt_i2c_transfer(bus, &msgs, 1) == 1) {
        return RT_EOK;
    } else {
        return -RT_ERROR;
    }
}

static rt_err_t _ap3216c_read_reg(struct rt_i2c_bus_device *bus, rt_uint8_t reg, rt_uint8_t *data)
{
    struct rt_i2c_msg msgs[2];
    rt_uint8_t reg_addr = reg;

    /* 先写寄存器地址 */
    msgs[0].addr  = AP3216C_ADDR;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf   = &reg_addr;
    msgs[0].len   = 1;

    /* 再读寄存器数据 */
    msgs[1].addr  = AP3216C_ADDR;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf   = data;
    msgs[1].len   = 1;

    /* 调用I2C设备接口传输数据 */
    if (rt_i2c_transfer(bus, msgs, 2) == 2) {
        return RT_EOK;
    } else {
        return -RT_ERROR;
    }
}

static rt_err_t _ap3216c_read_regs(struct rt_i2c_bus_device *bus, rt_uint8_t reg, rt_uint8_t *buf, rt_uint8_t len)
{
    struct rt_i2c_msg msgs[2];
    rt_uint8_t reg_addr = reg;

    /* 先写寄存器地址 */
    msgs[0].addr  = AP3216C_ADDR;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf   = &reg_addr;
    msgs[0].len   = 1;

    /* 再读寄存器数据 */
    msgs[1].addr  = AP3216C_ADDR;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf   = buf;
    msgs[1].len   = len;

    /* 调用I2C设备接口传输数据 */
    if (rt_i2c_transfer(bus, msgs, 2) == 2) {
        return RT_EOK;
    } else {
        return -RT_ERROR;
    }
}

/************************************* i2c r/w ops ******************************************************/



rt_err_t ap3216c_init(const char *name)
{
    rt_uint8_t data = 0;

    /* 查找I2C总线设备，获取I2C总线设备句柄 */
    s_ap3216c_i2c_bus = (struct rt_i2c_bus_device *)rt_device_find(name);

    if (s_ap3216c_i2c_bus == RT_NULL) {
        rt_kprintf("can't find %s device!\n", name);
        return -RT_ERROR;
    }

    /* 复位 AP3216C */
    if (_ap3216c_write_reg(s_ap3216c_i2c_bus, AP3216C_SYS_CFG, 0x04) != RT_EOK) {
        rt_kprintf("ap3216c reset failed!\n");
        return -RT_ERROR;
    }
    rt_thread_mdelay(50); /* AP3216C复位至少10ms */

    /* 开启 ALS、PS+IR 模式 */
    if (_ap3216c_write_reg(s_ap3216c_i2c_bus, AP3216C_SYS_CFG, 0x03) != RT_EOK) {
        rt_kprintf("ap3216c config failed!\n");
        return -RT_ERROR;
    }
    rt_thread_mdelay(10);

    /* 读取配置寄存器验证 */
    if (_ap3216c_read_reg(s_ap3216c_i2c_bus, AP3216C_SYS_CFG, &data) != RT_EOK) {
        rt_kprintf("ap3216c read config failed!\n");
        return -RT_ERROR;
    }

    if (data == 0x03) {
        rt_kprintf("ap3216c init success!\n");
        return RT_EOK;
    } else {
        rt_kprintf("ap3216c init failed, config reg = 0x%02x\n", data);
        s_ap3216c_i2c_bus = RT_NULL;
        return -RT_ERROR;
    }
}

rt_err_t ap3216c_read_data(rt_uint16_t *ir, rt_uint16_t *als, rt_uint16_t *ps)
{
    rt_uint8_t buf[6];

    if (s_ap3216c_i2c_bus == RT_NULL) {
        return -RT_ERROR;
    }

    /* 循环读取所有传感器数据寄存器 (从 IR_DATA_LOW 开始连续读取6个字节) */
    if (_ap3216c_read_regs(s_ap3216c_i2c_bus, AP3216C_IR_DATA_LOW, buf, 6) != RT_EOK) {
        return -RT_ERROR;
    }

    /* 解析 IR 数据 */
    if (buf[0] & 0x80) { /* IR_OF位为1,则数据无效 */
        *ir = 0;
    } else {
        *ir = ((rt_uint16_t)buf[1] << 2) | (buf[0] & 0x03);
    }

    /* 解析 ALS 数据 */
    *als = ((rt_uint16_t)buf[3] << 8) | buf[2];

    /* 解析 PS 数据 */
    if (buf[4] & 0x40) { /* PS_OF位为1,则数据无效 */
        *ps = 0;
    } else {
        *ps = ((rt_uint16_t)(buf[5] & 0x3F) << 4) | (buf[4] & 0x0F);
    }

    return RT_EOK;
}
