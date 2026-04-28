#include "led.h"
#include "gpio/drv_gpio.h"

#define LED_PIN_NUM     GET_PIN(1, 3)   /* GPIO1_IO03 */


void led_init(void)
{
    rt_pin_mode (LED_PIN_NUM, PIN_MODE_OUTPUT);
    rt_pin_write(LED_PIN_NUM, PIN_HIGH);
}

void led_on(void)
{
    rt_pin_write(LED_PIN_NUM, PIN_LOW);
}

void led_off(void)
{
    rt_pin_write(LED_PIN_NUM, PIN_HIGH);
}

void led_toggle(void)
{
    rt_ssize_t state = rt_pin_read(LED_PIN_NUM);
    rt_pin_write(LED_PIN_NUM, (state == PIN_HIGH) ? PIN_LOW : PIN_HIGH);
}
