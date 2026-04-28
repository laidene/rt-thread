#include "beep.h"
#include "gpio/drv_gpio.h"


#define BEEP_PIN_NUM    GET_PIN(5, 1)   /* GPIO5_IO01 */


void beep_init(void)
{
    rt_pin_mode (BEEP_PIN_NUM, PIN_MODE_OUTPUT);
    rt_pin_write(BEEP_PIN_NUM, PIN_HIGH);
}

void beep_on(void)
{
    rt_pin_write(BEEP_PIN_NUM, PIN_LOW);
}

void beep_off(void)
{
    rt_pin_write(BEEP_PIN_NUM, PIN_HIGH);
}

void beep_toggle(void)
{
    rt_ssize_t state = rt_pin_read(BEEP_PIN_NUM);
    rt_pin_write(BEEP_PIN_NUM, (state == PIN_HIGH) ? PIN_LOW : PIN_HIGH);
}
