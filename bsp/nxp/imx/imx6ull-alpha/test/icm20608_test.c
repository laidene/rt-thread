#include "spi/icm20608.h"


/* msh test */
static void _icm20608_test(int argc, char *argv[])
{
    rt_uint8_t id = 0;
    rt_int16_t accel[3], gyro[3];
    const char *bus = ICM20608_SPI_BUS_NAME;

    if (argc >= 2)
        bus = argv[1];

    if (icm20608_init(bus) != RT_EOK) {
        rt_kprintf("icm20608 init failed, check spi bus '%s' and cs pin\n", bus);
        return;
    }
    rt_kprintf("icm20608 init ok\n");

    if (icm20608_read_who_am_i(&id) == RT_EOK)
        rt_kprintf("who_am_i: 0x%02x (expect 0x%02x)\n", id, (rt_uint8_t)ICM20608_WHO_AM_I_VAL);

    if (icm20608_read_accel_gyro(accel, gyro) == RT_EOK) {
        rt_kprintf("accel: x=%d y=%d z=%d\n", accel[0], accel[1], accel[2]);
        rt_kprintf("gyro:  x=%d y=%d z=%d\n", gyro[0], gyro[1], gyro[2]);
    } else {
        rt_kprintf("read accel/gyro failed\n");
    }
}
MSH_CMD_EXPORT_ALIAS(_icm20608_test, icm20608_test, icm20608 test: icm20608_test [spi_bus_name]);