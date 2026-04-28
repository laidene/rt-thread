#include <rtconfig.h>

#ifdef BSP_USING_ONCHIP_RTC

#include "drv_common.h"
#include <drivers/dev_rtc.h>
#include <sys/time.h>

#define DBG_TAG "imx6ull.rtc"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>



static struct rt_device s_rtc_device;





/******************************************* rtc hardware ctrl ops ****************************************** */


static time_t s_get_rtc_timestamp(void)
{
    snvs_hp_rtc_datetime_t rtcDate;
    SNVS_Type *snvs = (SNVS_Type*)IMX6ULL_SNVS_BASE;
    struct tm tm_new;

    SNVS_HP_RTC_GetDatetime(snvs, &rtcDate);

    tm_new.tm_sec  = rtcDate.second;
    tm_new.tm_min  = rtcDate.minute;
    tm_new.tm_hour = rtcDate.hour;
    tm_new.tm_mday = rtcDate.day;
    tm_new.tm_mon  = rtcDate.month - 1;
    tm_new.tm_year = rtcDate.year - 1900;

    return mktime(&tm_new);
}

static rt_err_t s_set_rtc_time_stamp(time_t time_stamp)
{
    snvs_hp_rtc_datetime_t rtcDate;
    SNVS_Type *snvs = (SNVS_Type*)IMX6ULL_SNVS_BASE;
    struct tm *p_tm;

    p_tm = localtime(&time_stamp);

    rtcDate.second = p_tm->tm_sec;
    rtcDate.minute = p_tm->tm_min;
    rtcDate.hour   = p_tm->tm_hour;
    rtcDate.day    = p_tm->tm_mday;
    rtcDate.month  = p_tm->tm_mon + 1;
    rtcDate.year   = p_tm->tm_year + 1900;

    if (kStatus_Success != SNVS_HP_RTC_SetDatetime(snvs, &rtcDate))
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}



static void s_rtc_init(void)
{
    snvs_hp_rtc_config_t snvsRtcConfig;
    SNVS_Type *snvs = (SNVS_Type*)IMX6ULL_SNVS_BASE;

    SNVS_HP_RTC_GetDefaultConfig(&snvsRtcConfig);
    SNVS_HP_RTC_Init(snvs, &snvsRtcConfig);

    SNVS_HP_RTC_StartTimer(snvs);
}

/******************************************* rtc hardware ctrl ops ****************************************** */




/*********************************** rt rtc ops *********************************************/

static rt_err_t s_rtc_ops_control( rt_device_t dev, int cmd, void *args )
{
    rt_err_t result;

    RT_ASSERT(RT_NULL != dev);

    result = RT_EOK;
    switch (cmd)
    {
        case RT_DEVICE_CTRL_RTC_GET_TIME:
            *(time_t *)args = s_get_rtc_timestamp();
            LOG_D("RTC: get rtc_time %x", *(time_t *)args);
            break;

        case RT_DEVICE_CTRL_RTC_SET_TIME:
            if (s_set_rtc_time_stamp(*(time_t *)args))
            {
                result = -RT_ERROR;
            }
            LOG_D("RTC: set rtc_time %x", *(time_t *)args);
            break;
        case RT_DEVICE_CTRL_RTC_GET_TIMEVAL:
        {
            struct timeval *tv = (struct timeval *)args;
            LOG_D("RTC: control GET_TIMEVAL");
            if (tv == RT_NULL)
            {
                result = -RT_ERROR;
                break;
            }
            tv->tv_sec = s_get_rtc_timestamp();
            tv->tv_usec = 0;
            LOG_D("RTC: GET_TIMEVAL -> sec=%ld, usec=%ld", (long)tv->tv_sec, (long)tv->tv_usec);
            break;
        }
        case RT_DEVICE_CTRL_RTC_SET_TIMEVAL:
        {
            struct timeval *tv = (struct timeval *)args;
            LOG_D("RTC: control SET_TIMEVAL");
            if (tv == RT_NULL)
            {
                result = -RT_ERROR;
                break;
            }
            LOG_D("RTC: SET_TIMEVAL -> sec=%ld, usec=%ld", (long)tv->tv_sec, (long)tv->tv_usec);
            if (s_set_rtc_time_stamp((time_t)tv->tv_sec))
            {
                result = -RT_ERROR;
            }
            break;
        }
        case RT_DEVICE_CTRL_RTC_GET_TIMESPEC:
        {
            struct timespec *ts = (struct timespec *)args;
            LOG_D("RTC: control GET_TIMESPEC");
            if (ts == RT_NULL)
            {
                result = -RT_ERROR;
                break;
            }
            ts->tv_sec = s_get_rtc_timestamp();
            ts->tv_nsec = 0;
            LOG_D("RTC: GET_TIMESPEC -> sec=%ld, nsec=%ld", (long)ts->tv_sec, (long)ts->tv_nsec);
            break;
        }
        case RT_DEVICE_CTRL_RTC_SET_TIMESPEC:
        {
            struct timespec *ts = (struct timespec *)args;
            LOG_D("RTC: control SET_TIMESPEC");
            if (ts == RT_NULL)
            {
                result = -RT_ERROR;
                break;
            }
            LOG_D("RTC: SET_TIMESPEC -> sec=%ld, nsec=%ld", (long)ts->tv_sec, (long)ts->tv_nsec);
            if (s_set_rtc_time_stamp((time_t)ts->tv_sec))
            {
                result = -RT_ERROR;
            }
            break;
        }
        case RT_DEVICE_CTRL_RTC_GET_TIMERES:
        {
            struct timespec *ts = (struct timespec *)args;
            LOG_D("RTC: control GET_TIMERES");
            if (ts == RT_NULL)
            {
                result = -RT_ERROR;
                break;
            }
            ts->tv_sec = 0;
            ts->tv_nsec = 0;
            LOG_D("RTC: GET_TIMERES -> tv_nsec=%ld", (long)ts->tv_nsec);
            break;
        }
    }

    return result;
}

#ifdef RT_USING_DEVICE_OPS

static const struct rt_device_ops sc_k_rtc_ops = {
    RT_NULL,            /* init */
    RT_NULL,            /* open */
    RT_NULL,            /* close */
    RT_NULL,            /* read */
    RT_NULL,            /* write */
    s_rtc_ops_control,  /* control */
};

#endif

/*********************************** rt rtc ops *********************************************/





static rt_err_t s_rt_rtc_register( rt_device_t device, const char *name, rt_uint32_t flag )
{
    RT_ASSERT(RT_NULL != device);

    s_rtc_init();

    device->type        = RT_Device_Class_RTC;

#ifdef RT_USING_DEVICE_OPS
    device->ops         = &sc_k_rtc_ops;
#else
    device->init        = RT_NULL;
    device->open        = RT_NULL;
    device->close       = RT_NULL;
    device->read        = RT_NULL;
    device->write       = RT_NULL;
    device->control     = _rtc_ops_control;
#endif

    device->user_data   = RT_NULL;

    device->rx_indicate = RT_NULL;
    device->tx_complete = RT_NULL;

    return rt_device_register(device, name, flag);
}




int rt_hw_rtc_init(void)
{
    rt_err_t result;

    result = s_rt_rtc_register(&s_rtc_device, "rtc", RT_DEVICE_FLAG_RDWR);
    if (RT_EOK != result) {
        LOG_E("rtc register err code: %d", result);
        return result;
    }

    LOG_D("rtc init success.");

    return RT_EOK;
}
// INIT_DEVICE_EXPORT(rt_hw_rtc_init); // [qemu] x

#endif /* BSP_USING_ONCHIP_RTC */
