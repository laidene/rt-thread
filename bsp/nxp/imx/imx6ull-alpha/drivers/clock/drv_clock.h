#ifndef __BSP_CLOCK_H__
#define __BSP_CLOCK_H__

void system_clock_init(void);

void rt_hw_us_delay(uint32_t us);
void rt_hw_ms_delay(uint32_t ms);

#endif
