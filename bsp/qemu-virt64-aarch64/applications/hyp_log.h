#ifndef __HYP_LOG_H__
#define __HYP_LOG_H__

#include <rtthread.h>

void hyp_log_stage2_abort(rt_uint64_t ipa, rt_uint64_t gicd_offset,
        rt_uint64_t esr, rt_uint64_t far, rt_uint64_t hpfar, rt_uint64_t elr);
void hyp_log_dump(void);
void hyp_log_clear(void);

#endif
