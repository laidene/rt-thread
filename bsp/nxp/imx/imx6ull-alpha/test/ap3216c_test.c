#include "i2c/ap3216c.h"



#define AP3216C_I2C_BUS_NAME          "i2c1"

static rt_bool_t is_inited = RT_FALSE;

static void _ap3216c_test(int argc, char *argv[])
{
    rt_uint16_t ir = 0, als = 0, ps = 0;
    char name[RT_NAME_MAX];

    rt_strncpy(name, AP3216C_I2C_BUS_NAME, RT_NAME_MAX);

    if (is_inited == RT_FALSE) {
        if (ap3216c_init(name) != RT_EOK) {
            return;
        }
        is_inited = RT_TRUE;
    }

    if (is_inited != RT_FALSE) {
        /* 读取传感器数据 */
        if (ap3216c_read_data(&ir, &als, &ps) == RT_EOK) {
            rt_kprintf("ap3216c sensor data:\n");
            rt_kprintf("  IR  : %d\n", ir);
            rt_kprintf("  ALS : %d\n", als);
            rt_kprintf("  PS  : %d\n", ps);
        } else {
            rt_kprintf("read ap3216c sensor data failed!\n");
        }
    } else {
        rt_kprintf("ap3216c sensor not initialized!\n");
    }
}

MSH_CMD_EXPORT_ALIAS(_ap3216c_test, ap3216c_test, i2c ap3216c sample);
