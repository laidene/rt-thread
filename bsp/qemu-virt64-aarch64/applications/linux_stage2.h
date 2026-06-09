#ifndef __LINUX_STAGE2_H__
#define __LINUX_STAGE2_H__

#include <rtthread.h>

struct linux_stage2_trap_frame
{
    rt_uint64_t x[31];
    rt_uint64_t esr;
    rt_uint64_t far;
    rt_uint64_t hpfar;
    rt_uint64_t elr;
    rt_uint64_t pad;
};

void linux_stage2_init(void);
int linux_stage2_abort(struct linux_stage2_trap_frame *tf);

#endif
