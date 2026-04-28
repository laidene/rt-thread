#include "key.h"
#include "gpio/drv_gpio.h"

#define KEY0_PIN_NUM    GET_PIN(1, 18)  /* GPIO1_IO18 */


static void (*_key0_callback)(void *args) = RT_NULL;
static void *_key0_args                   = RT_NULL;


static void _key0_irq_handler(void *args)
{
    if (_key0_callback != RT_NULL) {
        _key0_callback(_key0_args);
    }
}





void key0_set_callback(void (*callback)(void *args), void *args)
{
    _key0_callback = callback;
    _key0_args     = args;
}

void key_init()
{
    rt_pin_mode(KEY0_PIN_NUM, PIN_MODE_INPUT_PULLUP);

    rt_pin_attach_irq(KEY0_PIN_NUM, PIN_IRQ_MODE_FALLING, _key0_irq_handler, RT_NULL);

    rt_pin_irq_enable(KEY0_PIN_NUM, PIN_IRQ_ENABLE);
}

rt_ssize_t key0_read(void)
{
    return rt_pin_read(KEY0_PIN_NUM);
}