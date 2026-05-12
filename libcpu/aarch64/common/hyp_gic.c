/*
 * qemu virt64 el2 启动路径里的最小 gic trap 策略。
 */

#include <rtthread.h>

#define esr_el2_dabt_wnr               (1ul << 6)

#define gicd_base                      0x08000000ul
#define gicd_end                       0x08010000ul

rt_uint64_t hyp_gicd_write_trap_count rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gicd_write_gpa rt_section(".bss.noclean.hyp");
rt_uint64_t hyp_last_gicd_write_esr rt_section(".bss.noclean.hyp");

rt_uint64_t hyp_handle_gicd_dabt(rt_uint64_t esr, rt_uint64_t gpa)
{
  if (!(esr & esr_el2_dabt_wnr)) {
    return 0;
  }

  if (gpa < gicd_base || gpa >= gicd_end) {
    return 0;
  }

  hyp_gicd_write_trap_count++;
  hyp_last_gicd_write_gpa = gpa;
  hyp_last_gicd_write_esr = esr;

  return 1;
}
