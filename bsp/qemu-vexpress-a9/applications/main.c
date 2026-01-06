/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020/12/31     Bernard      Add license info
 */

#include <stdint.h>
#include <stdio.h>
#include <rtthread.h>
#include <gic.h>




int main(void)
{
    rt_kprintf("Hello RT-Thread!\n");
    arm_gic_dump_type(0);

    rt_uint32_t scr, cpsr;
    asm volatile("mrc p15, 0, %0, c1, c1, 0" : "=r"(scr));
    asm volatile("mrs %0, cpsr" : "=r"(cpsr));
    rt_kprintf("SCR = 0x%08x\n", scr);
    rt_kprintf("CPSR = 0x%08x\n", cpsr);
    
    /* 根据 Table B4-29 Processor security state:
     * - SCR.NS = 0: 总是 Secure state
     * - SCR.NS = 1: 
     *   * Monitor mode (CPSR.M = 0x16): Secure state
     *   * 其他模式: Non-secure state
     */
    rt_uint32_t ns = scr & 0x1;
    rt_uint32_t mode = cpsr & 0x1f;
    rt_uint32_t is_monitor = (mode == 0x16);
    
    if (ns == 0) {
        rt_kprintf("Current world: Secure (SCR.NS=0)\n");
    } else {
        if (is_monitor) {
            rt_kprintf("Current world: Secure (SCR.NS=1, Monitor mode)\n");
        } else {
            rt_kprintf("Current world: Non-Secure (SCR.NS=1, mode=0x%02x)\n", mode);
        }
    }

    
    return 0;
}
