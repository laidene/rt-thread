#include "drv_gpio.h"
#include <drivers/dev_pin.h>

static rt_uint32_t iomuxc_base      = IOMUXC_BASE     ;     /* 0x20E0000 */
static rt_uint32_t iomuxc_snvs_base = IOMUXC_SNVS_BASE;     /* 0x2290000 */


/* 0x0030U =           0011 0000 : dse= 110 r6  */
/* 0x0830U =      1000 0011 0000 : ode=1 dse= 110 r6  开漏*/
/* 0x3030U = 0011 0000 0011 0000 : pue=1 pke=1 pus=0 dse=110 r6 100k下拉*/
/* 0xB030U = 1011 0000 0011 0000 : pue=1 pke=1 pus=3 dse=110 r6 22k上拉*/

/* 0x0000 = src=0=低速翻转 dse=启动能力 speed=0=50M低速 ode=0=推挽 pke=0=禁用 hsy=0=无延迟 */

/* i.mx6ull pad control register (SW_PAD_CTL), 32-bit, bit0=LSB */
struct gpio_pad_ctrl {
    uint32_t sre        : 1;  /* bit 0:  slew rate (0=slow, 1=fast) */

    uint32_t reserved_1 : 2;  /* bits 1-2: reserved */

    uint32_t dse        : 3;  /* bits 3-5: drive strength (0=disabled, 1-7=R0/2..R0/7) */
    uint32_t speed      : 2;  /* bits 6-7: speed (0=50M, 1/2=100M, 3=200M) */

    uint32_t reserved_2 : 3;  /* bits 8-10: reserved */

    uint32_t ode        : 1;  /* bit 11: open drain enable (0=disabled, 1=enabled) */

    uint32_t pke        : 1;  /* bit 12: pull/keeper enable (0=disabled, 1=enabled) */
    uint32_t pue        : 1;  /* bit 13: pull/keeper select (0=keeper, 1=pull) */
    uint32_t pus        : 2;  /* bits 14-15: pull config (0=100k down, 1=47k up, 2=100k up, 3=22k up) */

    uint32_t hys        : 1;  /* bit 16: hysteresis enable (0=disabled, 1=enabled) */
    uint32_t reserved_3 : 15; /* bits 17-31: reserved */
};


struct pin_mask
{
    GPIO_Type*      gpio      ;     /* GPIO base address */
    rt_int32_t      valid_mask;     /* Valid mask for the GPIO port */
    clock_ip_name_t gpio_clock;     /* GPIO clock */
};

/* ref Chapter 28  -> External Signals */
struct pin_mask mask_tab[5] =
{
    {GPIO1, 0xffffffff, kCLOCK_Gpio1},     /* GPIO1 */
    {GPIO2, 0x003fffff, kCLOCK_Gpio2},     /* GPIO2 */
    {GPIO3, 0x1fffffff, kCLOCK_Gpio3},     /* GPIO3, 29~31 not supported */
    {GPIO4, 0x1fffffff, kCLOCK_Gpio4},     /* GPIO4, 29~31 not supported */
    {GPIO5, 0x00000fff, kCLOCK_Gpio5},     /* GPIO5, 12~31 not supported */
};


/**
 * GPIOX in IOMUXC
 * GPIO5 in IOMUXC_SNVS
 * 
 * SW_MUX_CTL_CTRL and SW_PAD_CTRL offset
 */
const rt_int8_t gpio_reg_offset[5][32] =
{
    { 6,  7,  8,  9, 10, 11, 12, 13, 14, 15,  0,  1,  2,  3,  4,  5, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,},
    {32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 94, 95, 96, 97, 98, 99, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,},
    {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, -1, -1, -1,},
    {77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93,100,101,102,103,104,105,106,107,108,109,110,111, -1, -1, -1,},
    { 2,  3,  4,  5,  6,  7,  8,  9, 10, 11,  0,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,},
};


/**
 * @brief 
 * 
 * {pin_index, irq_mode, irq_handler, irq_handler_args}
 */
static struct rt_pin_irq_hdr pin_irq_hdr_tab[] =
{
    // GPIO1
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    // GPIO2
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    // GPIO3
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    // GPIO4
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    // GPIO5
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
};




/********************************************** rt gpio ops ***********************************************/

/**
 * @brief 
 * @param device 
 * @param pin  pin number(0-159)
 * @param mode one of PIN_MODE_XXX
 */
static void imx6ull_pin_mode(struct rt_device* device, rt_base_t pin, rt_uint8_t mode)
{
    (void)device;

    GPIO_Type*          gpio_base = RT_NULL;
    gpio_pin_config_t   config;
    rt_uint32_t         config_value = 0;
    rt_int8_t           port, pin_num, reg_offset;
    rt_uint32_t         mux_base_addr, config_base_addr;

    port    = pin >> 5; /* 1 port has 32 pins eg: pin=33(port=1,pin_num=1)*/
    pin_num = pin & 31;

    config.outputLogic   = PIN_LOW;
    config.interruptMode = kGPIO_NoIntmode;

    switch (mode) {
    case PIN_MODE_OUTPUT: {
        config.direction = kGPIO_DigitalOutput;
        config_value = 0x0030U;
    } break;

    case PIN_MODE_INPUT: {
        config.direction = kGPIO_DigitalInput;
        config_value = 0x0830U;
    } break;

    case PIN_MODE_INPUT_PULLDOWN: {
        config.direction = kGPIO_DigitalInput;
        config_value = 0x3030U;
    } break;

    case PIN_MODE_INPUT_PULLUP: {
        config.direction = kGPIO_DigitalInput;
        config_value = 0xB030U;
    } break;

    case PIN_MODE_OUTPUT_OD: {
        config.direction = kGPIO_DigitalOutput;
        config_value = 0x0830U;
    } break;
    }

    reg_offset = gpio_reg_offset[port][pin_num];
    gpio_base  = (GPIO_Type *)mask_tab[port].gpio;


    if (gpio_base != GPIO5) {
        IOMUXC_Type *periph = (IOMUXC_Type *)iomuxc_base;

        mux_base_addr    = (rt_uint32_t)&periph->SW_MUX_CTL_PAD[reg_offset];
        config_base_addr = (rt_uint32_t)&periph->SW_PAD_CTL_PAD[reg_offset];
    } else {
        IOMUXC_SNVS_Type *periph = (IOMUXC_SNVS_Type *)iomuxc_snvs_base;

        mux_base_addr    = (rt_uint32_t)&periph->SW_MUX_CTL_PAD[reg_offset];
        config_base_addr = (rt_uint32_t)&periph->SW_PAD_CTL_PAD[reg_offset];
    }

    IOMUXC_SetPinMux   (mux_base_addr, 0x5U, 0x00000000U, 0x0U, config_base_addr, 1);
    IOMUXC_SetPinConfig(mux_base_addr, 0x5U, 0x00000000U, 0x0U, config_base_addr, config_value);

    GPIO_PinInit(mask_tab[port].gpio, pin_num, &config);
}


static void imx6ull_pin_write(struct rt_device *device, rt_base_t pin, rt_uint8_t value)
{
    (void)device;

    rt_int8_t port = 0, pin_num = 0;

    port    = pin >> 5;
    pin_num = pin & 31;

    GPIO_WritePinOutput(mask_tab[port].gpio, pin_num, value);
}


static rt_ssize_t imx6ull_pin_read(struct rt_device *device, rt_base_t pin)
{
    (void)device;

    rt_ssize_t value = 0;
    rt_int8_t port = 0, pin_num = 0;

    value   = PIN_LOW;
    port    = pin >> 5;
    pin_num = pin & 31;

    value = GPIO_ReadPadStatus(mask_tab[port].gpio, pin_num);

    return value;
}


/**
 * @brief set the interrupt handler for the pin
 * @param device 
 * @param pin   pin number(0-159)
 * @param mode  
 * @param hdr   interrupt handler
 * @param args  interrupt handler arguments
 * @return 
 */
static rt_err_t imx6ull_pin_attach_irq(struct rt_device *device, rt_base_t pin, rt_uint8_t mode, void (*hdr)(void* args), void* args)
{
    (void)device;

    rt_base_t level = 0;
    level = rt_hw_interrupt_disable();

    if(pin_irq_hdr_tab[pin].pin  == pin  &&
       pin_irq_hdr_tab[pin].hdr  == hdr  &&
       pin_irq_hdr_tab[pin].mode == mode &&
       pin_irq_hdr_tab[pin].args == args)
    {
        rt_hw_interrupt_enable(level);
        return RT_EOK;
    }

    pin_irq_hdr_tab[pin].pin  = pin;
    pin_irq_hdr_tab[pin].hdr  = hdr;
    pin_irq_hdr_tab[pin].mode = mode;
    pin_irq_hdr_tab[pin].args = args;
    rt_hw_interrupt_enable(level);

    return RT_EOK;
}


/**
 * @brief unset the interrupt handler for the pin
 * @param device 
 * @param pin   pin number(0-159)
 * @return 
 */
static rt_err_t imx6ull_pin_detach_irq(struct rt_device *device, rt_base_t pin)
{
    rt_base_t level = 0;

    level = rt_hw_interrupt_disable();

    if(pin_irq_hdr_tab[pin].pin == -1)
    {
        rt_hw_interrupt_enable(level);
        return RT_EOK;
    }

    pin_irq_hdr_tab[pin].pin = -1;
    pin_irq_hdr_tab[pin].hdr = RT_NULL;
    pin_irq_hdr_tab[pin].mode = 0;
    pin_irq_hdr_tab[pin].args = RT_NULL;
    rt_hw_interrupt_enable(level);

    return RT_EOK;
}


/**
 * @brief  config_enable or disable the interrupt for the pin
 * @param device 
 * @param pin       
 * @param enabled   PIN_IRQ_ENABLE / PIN_IRQ_DISABLE
 * @return 
 */
static rt_err_t imx6ull_pin_irq_enable(struct rt_device *device, rt_base_t pin, rt_uint8_t enabled)
{
    gpio_interrupt_mode_t int_mode;
    rt_int8_t port = 0, pin_num = 0;

    port    = pin >> 5;
    pin_num = pin & 31;

    if(pin_irq_hdr_tab[pin].pin == -1)
    {
        rt_kprintf("rtt pin: %d callback function not initialized!\n", pin);
        return RT_ENOSYS;
    }

    if(enabled == PIN_IRQ_ENABLE)
    {
        switch (pin_irq_hdr_tab[pin].mode)
        {
        case PIN_IRQ_MODE_RISING:
            int_mode = kGPIO_IntRisingEdge;
            break;
        case PIN_IRQ_MODE_FALLING:
            int_mode = kGPIO_IntFallingEdge;
            break;
        case PIN_IRQ_MODE_RISING_FALLING:
            int_mode = kGPIO_IntRisingOrFallingEdge;
        case PIN_IRQ_MODE_HIGH_LEVEL:
            int_mode = kGPIO_IntHighLevel;
            break;
        case PIN_IRQ_MODE_LOW_LEVEL:
            int_mode = kGPIO_IntHighLevel;
            break;
        default:
            int_mode = kGPIO_IntRisingEdge;
            break;
        }

        GPIO_SetPinInterruptConfig(mask_tab[port].gpio, pin_num, int_mode);
        GPIO_EnableInterrupts     (mask_tab[port].gpio, 1U << pin_num);
    } 
    else if(enabled == PIN_IRQ_DISABLE)
    {
        GPIO_DisableInterrupts(mask_tab[port].gpio, pin_num);
    }
    else
    {
        return RT_EINVAL;
    }

    return RT_EOK;
}


static const struct rt_pin_ops gpio_ops = {
    .pin_mode       = imx6ull_pin_mode,
    .pin_write      = imx6ull_pin_write,
    .pin_read       = imx6ull_pin_read,
    .pin_attach_irq = imx6ull_pin_attach_irq,
    .pin_detach_irq = imx6ull_pin_detach_irq,
    .pin_irq_enable = imx6ull_pin_irq_enable,
    .pin_get        = RT_NULL,
};

/********************************************** rt gpio ops ***********************************************/





/********************************************** gpio isr ***********************************************/

/**
 * @brief 
 * @param index_offset GPIOX index offset(0 / 32 / 64 / 96 / 128)  
 * @param pin_start 0 / 15
 * @param base GPIOX base address
 */
static void imx6ull_isr(rt_int16_t index_offset, rt_int8_t pin_start, GPIO_Type *base)
{
    rt_int32_t isr_status = 0, index = 0;
    rt_int8_t i = 0, pin_end = 0;

    pin_end = pin_start + 15;
    isr_status = GPIO_GetPinsInterruptFlags(base) & base->IMR;

    for (i = pin_start; i <= pin_end; i++)
    {
        if (isr_status & (1 << i))
        {
            GPIO_ClearPinsInterruptFlags(base, (1 << i));
            index = index_offset + i;
            if (pin_irq_hdr_tab[index].hdr != RT_NULL)
            {
                pin_irq_hdr_tab[index].hdr(pin_irq_hdr_tab[index].args);
            }
        }
    }
};

static void GPIO1_Combined_0_15_IRQHandler(int irqno, void *param)
{
    rt_interrupt_enter();
    imx6ull_isr(0, 0, mask_tab[0].gpio);
    rt_interrupt_leave();
}

static void GPIO1_Combined_16_31_IRQHandler(int irqno, void *param)
{
    rt_interrupt_enter();
    imx6ull_isr(0, 15, mask_tab[0].gpio);
    rt_interrupt_leave();
}

static void GPIO2_Combined_0_15_IRQHandler(int irqno, void *param)
{
    rt_interrupt_enter();
    imx6ull_isr(32, 0, mask_tab[1].gpio);
    rt_interrupt_leave();
}

static void GPIO2_Combined_16_31_IRQHandler(int irqno, void *param)
{
    rt_interrupt_enter();
    imx6ull_isr(32, 15, mask_tab[1].gpio);
    rt_interrupt_leave();
}

static void GPIO3_Combined_0_15_IRQHandler(int irqno, void *param)
{
    rt_interrupt_enter();
    imx6ull_isr(64, 0, mask_tab[2].gpio);
    rt_interrupt_leave();
}

static void GPIO3_Combined_16_31_IRQHandler(int irqno, void *param)
{
    rt_interrupt_enter();
    imx6ull_isr(64, 15, mask_tab[2].gpio);
    rt_interrupt_leave();
}

static void GPIO4_Combined_0_15_IRQHandler(int irqno, void *param)
{
    rt_interrupt_enter();
    imx6ull_isr(96, 0, mask_tab[3].gpio);
    rt_interrupt_leave();
}

static void GPIO4_Combined_16_31_IRQHandler(int irqno, void *param)
{
    rt_interrupt_enter();
    imx6ull_isr(96, 15, mask_tab[3].gpio);
    rt_interrupt_leave();
}

static void GPIO5_Combined_0_15_IRQHandler(int irqno, void *param)
{
    rt_interrupt_enter();
    imx6ull_isr(128, 0, mask_tab[4].gpio);
    rt_interrupt_leave();
}

static void GPIO5_Combined_16_31_IRQHandler(int irqno, void *param)
{
    rt_interrupt_enter();
    imx6ull_isr(128, 15, mask_tab[4].gpio);
    rt_interrupt_leave();
}

/********************************************** gpio isr ***********************************************/





static void imx6ull_pin_interrupt_install(void)
{
    rt_hw_interrupt_install(GPIO1_Combined_0_15_IRQn,  GPIO1_Combined_0_15_IRQHandler,  RT_NULL, "GPIO1_00_15");
    rt_hw_interrupt_install(GPIO1_Combined_16_31_IRQn, GPIO1_Combined_16_31_IRQHandler, RT_NULL, "GPIO1_16_31");
    rt_hw_interrupt_install(GPIO2_Combined_0_15_IRQn,  GPIO2_Combined_0_15_IRQHandler,  RT_NULL, "GPIO2_00_15");
    rt_hw_interrupt_install(GPIO2_Combined_16_31_IRQn, GPIO2_Combined_16_31_IRQHandler, RT_NULL, "GPIO2_16_31");
    rt_hw_interrupt_install(GPIO3_Combined_0_15_IRQn,  GPIO3_Combined_0_15_IRQHandler,  RT_NULL, "GPIO3_00_15");
    rt_hw_interrupt_install(GPIO3_Combined_16_31_IRQn, GPIO3_Combined_16_31_IRQHandler, RT_NULL, "GPIO3_16_31");
    rt_hw_interrupt_install(GPIO4_Combined_0_15_IRQn,  GPIO4_Combined_0_15_IRQHandler,  RT_NULL, "GPIO4_00_15");
    rt_hw_interrupt_install(GPIO4_Combined_16_31_IRQn, GPIO4_Combined_16_31_IRQHandler, RT_NULL, "GPIO4_16_31");
    rt_hw_interrupt_install(GPIO5_Combined_0_15_IRQn,  GPIO5_Combined_0_15_IRQHandler,  RT_NULL, "GPIO5_00_15");
    rt_hw_interrupt_install(GPIO5_Combined_16_31_IRQn, GPIO5_Combined_16_31_IRQHandler, RT_NULL, "GPIO5_16_31");

    rt_hw_interrupt_umask(GPIO1_Combined_0_15_IRQn);
    rt_hw_interrupt_umask(GPIO1_Combined_16_31_IRQn);
    rt_hw_interrupt_umask(GPIO2_Combined_0_15_IRQn);
    rt_hw_interrupt_umask(GPIO2_Combined_16_31_IRQn);
    rt_hw_interrupt_umask(GPIO3_Combined_0_15_IRQn);
    rt_hw_interrupt_umask(GPIO3_Combined_16_31_IRQn);
    rt_hw_interrupt_umask(GPIO4_Combined_0_15_IRQn);
    rt_hw_interrupt_umask(GPIO4_Combined_16_31_IRQn);
    rt_hw_interrupt_umask(GPIO5_Combined_0_15_IRQn);
    rt_hw_interrupt_umask(GPIO5_Combined_16_31_IRQn);
}

int imx6ull_hw_pin_init(void)
{
    for(int port = 0; port < sizeof(mask_tab) / sizeof(mask_tab[0]); port++)
    {
        CLOCK_EnableClock(mask_tab[port].gpio_clock);
    }

    imx6ull_pin_interrupt_install();

    rt_device_pin_register("pin", &gpio_ops, RT_NULL);

    return RT_EOK;
}
INIT_BOARD_EXPORT(imx6ull_hw_pin_init);

