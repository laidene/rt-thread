#ifndef __DRV_GPIO_H__
#define __DRV_GPIO_H__

#include "drv_common.h"

#define GET_PIN(PORTx, PIN)      (32 * (PORTx - 1) + (PIN & 31))

#endif