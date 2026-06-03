#include <rtthread.h>

#include "hyp_log.h"

#define HYP_LOG_BUF_SIZE    4096U
#define HYP_LOG_GICD_SIZE   0x00010000UL

static char hyp_log_buf[HYP_LOG_BUF_SIZE];
static volatile rt_uint32_t hyp_log_head;
static volatile rt_uint32_t hyp_log_count;
static volatile rt_uint32_t hyp_log_dropped;

static void hyp_log_putc(char ch)
{
    hyp_log_buf[hyp_log_head] = ch;
    hyp_log_head = (hyp_log_head + 1U) % HYP_LOG_BUF_SIZE;

    if (hyp_log_count < HYP_LOG_BUF_SIZE) {
        hyp_log_count++;
    } else {
        hyp_log_dropped++;
    }
}

static void hyp_log_puts(const char *str)
{
    while (*str != '\0') {
        hyp_log_putc(*str++);
    }
}

static void hyp_log_put_hex(rt_uint64_t value)
{
    static const char hex[] = "0123456789abcdef";

    hyp_log_puts("0x");

    for (int shift = 60; shift >= 0; shift -= 4) {
        hyp_log_putc(hex[(value >> shift) & 0xfU]);
    }
}

void hyp_log_stage2_abort(rt_uint64_t ipa, rt_uint64_t gicd_offset,
        rt_uint64_t esr, rt_uint64_t far, rt_uint64_t hpfar, rt_uint64_t elr)
{
    if (gicd_offset < HYP_LOG_GICD_SIZE) {
        hyp_log_puts("stage2 gicd ipa=");
        hyp_log_put_hex(ipa);
        hyp_log_puts(" off=");
        hyp_log_put_hex(gicd_offset);
    } else {
        hyp_log_puts("stage2 abort ipa=");
        hyp_log_put_hex(ipa);
    }

    hyp_log_puts(" esr=");
    hyp_log_put_hex(esr);
    hyp_log_puts(" far=");
    hyp_log_put_hex(far);
    hyp_log_puts(" hpfar=");
    hyp_log_put_hex(hpfar);
    hyp_log_puts(" elr=");
    hyp_log_put_hex(elr);
    hyp_log_putc('\n');
}

void hyp_log_dump(void)
{
    rt_uint32_t count = hyp_log_count;
    rt_uint32_t head = hyp_log_head;
    rt_uint32_t pos;

    rt_kprintf("hyp log: count=%u dropped=%u\n", count, hyp_log_dropped);

    if (count == 0U) {
        return;
    }

    pos = (head + HYP_LOG_BUF_SIZE - count) % HYP_LOG_BUF_SIZE;

    for (rt_uint32_t i = 0; i < count; ++i) {
        rt_kprintf("%c", hyp_log_buf[pos]);
        pos = (pos + 1U) % HYP_LOG_BUF_SIZE;
    }

    if (hyp_log_buf[(head + HYP_LOG_BUF_SIZE - 1U) % HYP_LOG_BUF_SIZE] != '\n') {
        rt_kprintf("\n");
    }
}

void hyp_log_clear(void)
{
    hyp_log_head = 0;
    hyp_log_count = 0;
    hyp_log_dropped = 0;
}
