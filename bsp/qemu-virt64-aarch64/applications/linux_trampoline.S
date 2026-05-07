/*
 * Trampoline for booting a Linux Image on a reserved CPU.
 */

#ifndef __ASSEMBLY__
#define __ASSEMBLY__
#endif

#define LINUX_IMAGE_ENTRY       0x48200000
#define LINUX_DTB_ADDRESS       0x4f000000

    .section ".text.linux_trampoline", "ax"
    .align 6
    .global linux_cpu3_trampoline
linux_cpu3_trampoline:
    ldr     x0, =LINUX_DTB_ADDRESS
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    ldr     x4, =LINUX_IMAGE_ENTRY
    br      x4

    .global linux_cpu3_trampoline_end
linux_cpu3_trampoline_end:
