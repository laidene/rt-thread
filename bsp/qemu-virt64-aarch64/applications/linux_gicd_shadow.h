#ifndef __LINUX_GICD_SHADOW_H__
#define __LINUX_GICD_SHADOW_H__

#include <rtthread.h>

#include "linux_stage2.h"

void linux_gicd_shadow_init(void);
int linux_gicd_shadow_abort(struct linux_stage2_trap_frame *tf);

#endif
