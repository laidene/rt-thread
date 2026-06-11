#include "hyp_log.h"


void hyp_log_hvc_args(rt_uint64_t x0, rt_uint64_t x1, rt_uint64_t x2, rt_uint64_t x3)
{
    hyp_log_printf("[hyp] hvc args x0=%lx x1=%lx x2=%lx x3=%lx\n", x0, x1, x2, x3);
}


void hyp_log_exception(
    const char *tag, rt_uint64_t esr, rt_uint64_t far, rt_uint64_t hpfar, rt_uint64_t elr, rt_uint64_t spsr)
{
    rt_uint64_t ec = (esr >> 26) & 0x3fUL;
    rt_uint64_t iss = esr & 0x01ffffffUL;
    rt_uint64_t dfsc = esr & 0x3fUL;
    rt_uint64_t wnr = (esr >> 6) & 0x1UL;

    hyp_log_printf("%s esr=%lx ec=%lx iss=%lx dfsc=%lx wnr=%lx far=%lx hpfar=%lx elr=%lx spsr=%lx\n",
                   tag,
                   esr,
                   ec,
                   iss,
                   dfsc,
                   wnr,
                   far,
                   hpfar,
                   elr,
                   spsr);
}