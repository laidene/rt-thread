#ifndef __LINUX_VGIC_H__
#define __LINUX_VGIC_H__

#include <rtthread.h>

void linux_vgic_init_cpu(void);
rt_uint32_t linux_vgic_lr_count(void);
void linux_vgic_log_physical_irq(void);
void linux_vgic_log_cpuif_state(const char *tag);
int linux_vgic_handle_physical_irq(void);
void linux_vgic_dump(void);

#endif
