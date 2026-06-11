#ifndef __HYP_LOG_H__
#define __HYP_LOG_H__

#include <rtthread.h>

void hyp_log_printf(const char *fmt, ...);


void hyp_log_dump(void);
void hyp_log_clear(void);

#endif
