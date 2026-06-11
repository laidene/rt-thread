#ifndef __LINUX_STAGE2_H__
#define __LINUX_STAGE2_H__

#include <rtthread.h>
#include "data_abort_helper.h"


void linux_stage2_prepare(void);
void linux_stage2_enable(void);
int  linux_stage2_abort(struct linux_stage2_trap_frame *tf);

#endif
