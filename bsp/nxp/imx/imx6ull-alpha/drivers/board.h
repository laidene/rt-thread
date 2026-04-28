#ifndef __BOARD_H__
#define __BOARD_H__

#include <rtconfig.h>
#include <mmu.h>

#include "imx6ull.h"


#if defined(__CC_ARM)
    extern int Image$$RW_IRAM1$$ZI$$Limit;
    #define HEAP_BEGIN      ((void*)&Image$$RW_IRAM1$$ZI$$Limit)
#elif defined(__GNUC__)
    extern int __bss_end;
    #define HEAP_BEGIN      ((void*)&__bss_end)
#endif

#ifdef RT_USING_SMART
    #define HEAP_END        (void*)(KERNEL_VADDR_START + 16 * 1024 * 1024)
    #define PAGE_START      HEAP_END
    #define PAGE_END        (void*)(KERNEL_VADDR_START + 128 * 1024 * 1024)
#else
    #define HEAP_END        (void*)(0x80000000 + 64 * 1024 * 1024)
#endif

void rt_hw_board_init(void);

#endif
