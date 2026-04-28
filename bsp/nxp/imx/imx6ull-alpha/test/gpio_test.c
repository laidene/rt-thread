#include "gpio/led.h"
#include "gpio/beep.h"
#include "gpio/key.h"


#include "gpio/drv_gpio.h"

void my_key_cb(void* arg){
    rt_kprintf("my_key_cb arg=%x",(uint32_t)arg);
}


static void _pins_test(void)
{
    rt_kprintf("[pins_test] start:\n");

    led_init();
    beep_init();


    key0_set_callback(my_key_cb,(void*)0x1234);
    key_init();


    rt_kprintf("[pins_test] end:\n");

}

MSH_CMD_EXPORT_ALIAS(_pins_test, pins_test, GPIO devices sample (LED/BEEP/KEY));