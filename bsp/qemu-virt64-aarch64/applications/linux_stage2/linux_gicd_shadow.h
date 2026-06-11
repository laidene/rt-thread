#ifndef __LINUX_GICD_SHADOW_H__
#define __LINUX_GICD_SHADOW_H__

#include <rtthread.h>

#include "data_abort_helper.h"

void linux_gicd_shadow_prepare(void);//todo

int linux_gicd_shadow_abort(struct linux_stage2_trap_frame *tf);

#endif
