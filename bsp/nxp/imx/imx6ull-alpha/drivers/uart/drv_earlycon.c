#include "drv_common.h"

#define RT_CONSOLE_EARLY_BUF_SIZE   (RT_CONSOLEBUF_SIZE * 100)

static char         s_buf[RT_CONSOLE_EARLY_BUF_SIZE];
static rt_size_t    s_idx = 0;


void rt_hw_console_output(const char *str)
{
    if (s_idx < RT_CONSOLE_EARLY_BUF_SIZE - 1) {
        rt_size_t copy_len = rt_strlen(str);

        if (s_idx + copy_len >= RT_CONSOLE_EARLY_BUF_SIZE) {
            copy_len = RT_CONSOLE_EARLY_BUF_SIZE - s_idx - 1;
        }

        if (copy_len > 0) {
            rt_memcpy(&s_buf[s_idx], str, copy_len);
            s_idx += copy_len;
            s_buf[s_idx] = '\0';
        }
    }
}

void rt_earlycon_kick_completed(void)
{
    if (s_idx > 0) {
        rt_kputs(s_buf);
        s_idx = 0;
        s_buf[0] = '\0';
    }
}
