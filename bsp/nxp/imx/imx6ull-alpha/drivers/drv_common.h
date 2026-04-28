#ifndef __DRV_COMMON_H__
#define __DRV_COMMON_H__

#include "board.h"


#define GET_ARRAY_NUM(ins)          ((uint32_t)(sizeof(ins)/sizeof(ins[0])))

struct io_mux_pad_cfg {
    rt_uint32_t mux_reg;
    rt_uint32_t mux_mode;
    rt_uint32_t input_reg;
    rt_uint32_t input_daisy;
    rt_uint32_t pad_reg;

    rt_uint32_t mux_sion;
    rt_uint32_t pad_val;
};

void io_mux_pad_init(const struct io_mux_pad_cfg *cfg);

#endif
