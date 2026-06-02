/*
 * 进入linux的入口
 */

#ifndef __ASSEMBLY__
#define __ASSEMBLY__
#endif

#define LINUX_IMAGE_ENTRY       0x48200000
#define LINUX_DTB_ADDRESS       0x4f000000

    .section ".text.linux_entry", "ax"
    .align 6
    .global linux_entry
linux_entry:
    ldr     x0, =LINUX_DTB_ADDRESS
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    ldr     x4, =LINUX_IMAGE_ENTRY
    br      x4

    .global linux_entry_end
linux_entry_end: