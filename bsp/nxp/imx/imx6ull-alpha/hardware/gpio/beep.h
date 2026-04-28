#ifndef __HARD_WARE_PINS_BEEP_H__
#define __HARD_WARE_PINS_BEEP_H__

#include <rtconfig.h>
#include <rtthread.h>
#include <rtdevice.h>

#ifdef __cplusplus
extern "C" {
#endif


void beep_init(void);
void beep_on(void);
void beep_off(void);
void beep_toggle(void);


#ifdef __cplusplus
}
#endif

#endif