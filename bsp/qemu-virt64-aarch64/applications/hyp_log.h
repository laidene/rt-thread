#ifndef __HYP_LOG_H__
#define __HYP_LOG_H__

#include <rtthread.h>

void hyp_log_putc(char ch);
void hyp_log_puts(const char *str);
void hyp_log_put_hex(rt_uint64_t value);
void hyp_log_exception(const char *tag, rt_uint64_t esr, rt_uint64_t far,
                       rt_uint64_t hpfar, rt_uint64_t elr, rt_uint64_t spsr);
void hyp_log_hvc_args(rt_uint64_t x0, rt_uint64_t x1, rt_uint64_t x2, rt_uint64_t x3);

void hyp_log_dump(void);
void hyp_log_clear(void);

#endif
