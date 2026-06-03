#ifndef __LINUX_STAGE2_H__
#define __LINUX_STAGE2_H__

void linux_stage2_init(void);
void linux_stage2_abort(rt_uint64_t esr, rt_uint64_t far, rt_uint64_t hpfar, rt_uint64_t elr);

#endif
