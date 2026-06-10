#include <rtthread.h>

#include "hyp_log.h"

#define HYP_LOG_BUF_SIZE 16384U


static char hyp_log_buf[HYP_LOG_BUF_SIZE];
static volatile rt_uint32_t hyp_log_head;
static volatile rt_uint32_t hyp_log_count;
static volatile rt_uint32_t hyp_log_dropped;

void hyp_log_putc(char ch)
{
    hyp_log_buf[hyp_log_head] = ch;
    hyp_log_head = (hyp_log_head + 1U) % HYP_LOG_BUF_SIZE;

    if (hyp_log_count < HYP_LOG_BUF_SIZE) {
        hyp_log_count++;
    } else {
        hyp_log_dropped++;
    }
}

void hyp_log_puts(const char *str)
{
    while (*str != '\0') {
        hyp_log_putc(*str++);
    }
}

void hyp_log_put_hex(rt_uint64_t value)
{
    static const char hex[] = "0123456789abcdef";
    rt_bool_t started = RT_FALSE;

    hyp_log_puts("0x");

    if (value == 0) {
        hyp_log_putc('0');
        return;
    }

    for (int shift = 60; shift >= 0; shift -= 4) {
        rt_uint8_t digit = (value >> shift) & 0xfU;

        if (!started && digit == 0U) {
            continue;
        }

        started = RT_TRUE;
        hyp_log_putc(hex[digit]);
    }
}

void hyp_log_exception(const char *tag, rt_uint64_t esr, rt_uint64_t far,
                       rt_uint64_t hpfar, rt_uint64_t elr, rt_uint64_t spsr)
{
    rt_uint64_t ec = (esr >> 26) & 0x3fUL;
    rt_uint64_t iss = esr & 0x01ffffffUL;
    rt_uint64_t dfsc = esr & 0x3fUL;
    rt_uint64_t wnr = (esr >> 6) & 0x1UL;

    hyp_log_puts(tag);
    hyp_log_puts(" esr=");
    hyp_log_put_hex(esr);
    hyp_log_puts(" ec=");
    hyp_log_put_hex(ec);
    hyp_log_puts(" iss=");
    hyp_log_put_hex(iss);
    hyp_log_puts(" dfsc=");
    hyp_log_put_hex(dfsc);
    hyp_log_puts(" wnr=");
    hyp_log_put_hex(wnr);
    hyp_log_puts(" far=");
    hyp_log_put_hex(far);
    hyp_log_puts(" hpfar=");
    hyp_log_put_hex(hpfar);
    hyp_log_puts(" elr=");
    hyp_log_put_hex(elr);
    hyp_log_puts(" spsr=");
    hyp_log_put_hex(spsr);
    hyp_log_putc('\n');
}

void hyp_log_hvc_args(rt_uint64_t x0, rt_uint64_t x1, rt_uint64_t x2, rt_uint64_t x3)
{
    hyp_log_puts("[hyp] hvc args x0=");
    hyp_log_put_hex(x0);
    hyp_log_puts(" x1=");
    hyp_log_put_hex(x1);
    hyp_log_puts(" x2=");
    hyp_log_put_hex(x2);
    hyp_log_puts(" x3=");
    hyp_log_put_hex(x3);
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
