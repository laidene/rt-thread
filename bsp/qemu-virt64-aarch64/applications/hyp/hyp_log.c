#include <rtthread.h>
#include <stdarg.h>

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

static void hyp_log_put_uint(rt_uint32_t value)
{
    rt_uint32_t div = 1;

    while ((value / div) >= 10U) {
        div *= 10U;
    }

    while (div != 0U) {
        hyp_log_putc((char)('0' + (value / div)));
        value %= div;
        div /= 10U;
    }
}

void hyp_log_printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);

    while (*fmt != '\0') {
        if (*fmt != '%') {
            hyp_log_putc(*fmt++);
            continue;
        }

        ++fmt;
        switch (*fmt) {
        case '\0':
            hyp_log_putc('%');
            --fmt;
            break;
        case '%':
            hyp_log_putc('%');
            break;
        case 's': {
            const char *str = va_arg(ap, const char *);
            hyp_log_puts(str != RT_NULL ? str : "(null)");
            break;
        }
        case 'x':
            hyp_log_put_hex((rt_uint32_t)va_arg(ap, unsigned int));
            break;
        case 'l':
            if (fmt[1] == 'x') {
                hyp_log_put_hex((rt_uint64_t)va_arg(ap, unsigned long));
                ++fmt;
            } else {
                hyp_log_putc('%');
                hyp_log_putc('l');
            }
            break;
        case 'u':
            hyp_log_put_uint((rt_uint32_t)va_arg(ap, unsigned int));
            break;
        default:
            hyp_log_putc('%');
            hyp_log_putc(*fmt);
            break;
        }

        ++fmt;
    }

    va_end(ap);
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
