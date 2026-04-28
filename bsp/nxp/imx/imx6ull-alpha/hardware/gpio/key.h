#ifndef __HARD_WARE_PINS_KEY_H__
#define __HARD_WARE_PINS_KEY_H__

#include <rtconfig.h>
#include <rtthread.h>
#include <rtdevice.h>

#ifdef __cplusplus
extern "C" {
#endif


void key_init();
void key0_set_callback(void (*callback)(void *args), void *args);
rt_ssize_t key0_read(void);

#ifdef __cplusplus
}
#endif

#endif