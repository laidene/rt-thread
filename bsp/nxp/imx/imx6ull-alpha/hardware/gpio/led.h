#ifndef __HARD_WARE_PINS_LED_H__
#define __HARD_WARE_PINS_LED_H__

#include <rtconfig.h>
#include <rtthread.h>
#include <rtdevice.h>

#ifdef __cplusplus
extern "C" {
#endif


void led_init(void);
void led_on(void);
void led_off(void);
void led_toggle(void);


#ifdef __cplusplus
}
#endif

#endif