#include "drv_sdio.h"
#include "fsl_clock.h"
#include "fsl_usdhc.h"
#include "fsl_host.h"

#include <drivers/dev_mmcsd_core.h>
#include <ioremap.h>


#define DBG_TAG "imx6ull.sdio"
#define DBG_LVL DBG_LOG /* DBG_LOG DBG_INFO DBG_WARNING DBG_ERROR */
#include <rtdbg.h>


#define CACHE_LINESIZE         (32)
#define USDHC_ADMA_TABLE_WORDS (8U)
#define IMXRT_MAX_FREQ         (52UL * 1000UL * 1000UL)


static uint32_t _usdhc_adma2_table[USDHC_ADMA_TABLE_WORDS];

#ifdef RT_USING_SDIO1
struct imx6ull_sdio_cfg sdio1_cfg = {
    .base        = (void*)USDHC1_BASE,
    .clk_ip_name = kCLOCK_Usdhc1,
    .usdhc_div   = kCLOCK_Usdhc1Div,
    .io_cfg = {
        .io_cmd   = {IOMUXC_SD1_CMD_USDHC1_CMD,     0, 0x10B1},
        .io_clk   = {IOMUXC_SD1_CLK_USDHC1_CLK,     0, 0x10B1},
        .io_cd    = {IOMUXC_UART1_RTS_B_USDHC1_CD_B,0, 0x10B0},
        .io_wp    = {0},
        .io_rst   = {0},
        .io_data0 = {IOMUXC_SD1_DATA0_USDHC1_DATA0, 0, 0x10B1},
        .io_data1 = {IOMUXC_SD1_DATA1_USDHC1_DATA1, 0, 0x10B1},
        .io_data2 = {IOMUXC_SD1_DATA2_USDHC1_DATA2, 0, 0x10B1},
        .io_data3 = {IOMUXC_SD1_DATA3_USDHC1_DATA3, 0, 0x10B1},
        .io_data4 = {0},
        .io_data5 = {0},
        .io_data6 = {0},
        .io_data7 = {0},
    },
};
#endif

#ifdef RT_USING_SDIO2
struct imx6ull_sdio_cfg sdio2_cfg = {
    .base        = (void*)USDHC2_BASE,
    .clk_ip_name = kCLOCK_Usdhc2,
    .usdhc_div   = kCLOCK_Usdhc2Div,
    .io_cfg   = {
        .io_cmd   = {IOMUXC_NAND_WE_B_USDHC2_CMD,     0, 0x10B1},
        .io_clk   = {IOMUXC_NAND_RE_B_USDHC2_CLK,     0, 0x10B1},
        .io_cd    = {0},
        .io_wp    = {0},
        .io_rst   = {IOMUXC_NAND_ALE_USDHC2_RESET_B,  0, 0x10B0},
        .io_data0 = {IOMUXC_NAND_DATA00_USDHC2_DATA0, 0, 0x10B1},
        .io_data1 = {IOMUXC_NAND_DATA01_USDHC2_DATA1, 0, 0x10B1},
        .io_data2 = {IOMUXC_NAND_DATA02_USDHC2_DATA2, 0, 0x10B1},
        .io_data3 = {IOMUXC_NAND_DATA03_USDHC2_DATA3, 0, 0x10B1},
        .io_data4 = {IOMUXC_NAND_DATA04_USDHC2_DATA4, 0, 0x10B1},
        .io_data5 = {IOMUXC_NAND_DATA05_USDHC2_DATA5, 0, 0x10B1},
        .io_data6 = {IOMUXC_NAND_DATA06_USDHC2_DATA6, 0, 0x10B1},
        .io_data7 = {IOMUXC_NAND_DATA07_USDHC2_DATA7, 0, 0x10B1},
    },
};
#endif




/************************************* imx sdio ops***********************************/

static void _sdmmc_host_error_recovery(USDHC_Type* base)
{
    uint32_t status = 0U;
    status = USDHC_GetPresentStatusFlags(base); /* get host present status */
    /* check command inhibit status flag */
    if ((status & kUSDHC_CommandInhibitFlag) != 0U) {
        /* reset command line */
        USDHC_Reset(base, kUSDHC_ResetCommand, 1000U);
    }
    /* check data inhibit status flag */
    if ((status & kUSDHC_DataInhibitFlag) != 0U) {
        /* reset data line */
        USDHC_Reset(base, kUSDHC_ResetData, 1000U);
    }
}


static void _imx6ull_sdio_io_init(struct imx6ull_sdio_device* dev)
{
    io_mux_pad_init(&dev->cfg->io_cfg.io_cmd);
    io_mux_pad_init(&dev->cfg->io_cfg.io_clk);
    io_mux_pad_init(&dev->cfg->io_cfg.io_cd);
    io_mux_pad_init(&dev->cfg->io_cfg.io_wp);
    io_mux_pad_init(&dev->cfg->io_cfg.io_rst);
    io_mux_pad_init(&dev->cfg->io_cfg.io_data0);
    io_mux_pad_init(&dev->cfg->io_cfg.io_data1);
    io_mux_pad_init(&dev->cfg->io_cfg.io_data2);
    io_mux_pad_init(&dev->cfg->io_cfg.io_data3);
    io_mux_pad_init(&dev->cfg->io_cfg.io_data4);
    io_mux_pad_init(&dev->cfg->io_cfg.io_data5);
    io_mux_pad_init(&dev->cfg->io_cfg.io_data6);
    io_mux_pad_init(&dev->cfg->io_cfg.io_data7);
}

static void _imx6ull_sdio_clk_init(struct imx6ull_sdio_device* dev) {
    CLOCK_EnableClock(dev->cfg->clk_ip_name);
    CLOCK_SetDiv(dev->cfg->usdhc_div, 5U);
}

static void _imx6ull_sdio_init(struct imx6ull_sdio_device* dev) {
    usdhc_host_t *usdhc_host = &dev->usdhc_host;

    usdhc_host->config.dataTimeout          = USDHC_DATA_TIMEOUT;
    usdhc_host->config.endianMode           = USDHC_ENDIAN_MODE;
    usdhc_host->config.readWatermarkLevel   = USDHC_READ_WATERMARK_LEVEL;
    usdhc_host->config.writeWatermarkLevel  = USDHC_WRITE_WATERMARK_LEVEL;
    usdhc_host->config.readBurstLen         = USDHC_READ_BURST_LEN;
    usdhc_host->config.writeBurstLen        = USDHC_WRITE_BURST_LEN;

    USDHC_Init(usdhc_host->base, &(usdhc_host->config));
}

/************************************* imx sdio ops***********************************/





/************************************* rt sdio ops***********************************/

static void _mmc_request(struct rt_mmcsd_host* host, struct rt_mmcsd_req* req)
{
    struct imx6ull_sdio_device* dev  = RT_NULL;
    struct rt_mmcsd_cmd*        cmd  = RT_NULL;
    struct rt_mmcsd_data*       data = RT_NULL;
    rt_uint32_t*                buf  = RT_NULL;
    usdhc_adma_config_t         dma_cfg     = { 0 };
    usdhc_command_t             fsl_command = { 0 };
    usdhc_data_t                fsl_data    = { 0 };
    usdhc_transfer_t            fsl_content = { 0 };
    status_t                    err_no;

    dev = (struct imx6ull_sdio_device*)host->private_data;
    RT_ASSERT(dev != RT_NULL);


    dma_cfg.dmaMode          = USDHC_DMA_MODE;
    dma_cfg.burstLen         = kUSDHC_EnBurstLenForINCR;
    dma_cfg.admaTable        = dev->usdhc_adma2_table;
    dma_cfg.admaTableWords   = USDHC_ADMA_TABLE_WORDS;


    cmd = req->cmd;
    RT_ASSERT(cmd != RT_NULL);

    LOG_D("cmd->cmd_code: %02d, cmd->arg: %08x, cmd->flags: %08x --> ", cmd->cmd_code, cmd->arg, cmd->flags);

    fsl_command.index       = cmd->cmd_code;
    fsl_command.argument    = cmd->arg;

    if (cmd->cmd_code == STOP_TRANSMISSION) {
        fsl_command.type = kCARD_CommandTypeAbort;
    } else {
        fsl_command.type = kCARD_CommandTypeNormal;
    }

    switch (cmd->flags & RESP_MASK) {
    case RESP_NONE:
        fsl_command.responseType = kCARD_ResponseTypeNone;
        break;
    case RESP_R1:
        fsl_command.responseType = kCARD_ResponseTypeR1;
        break;
    case RESP_R1B:
        fsl_command.responseType = kCARD_ResponseTypeR1b;
        break;
    case RESP_R2:
        fsl_command.responseType = kCARD_ResponseTypeR2;
        break;
    case RESP_R3:
        fsl_command.responseType = kCARD_ResponseTypeR3;
        break;
    case RESP_R4:
        fsl_command.responseType = kCARD_ResponseTypeR4;
        break;
    case RESP_R5:
        fsl_command.responseType = kCARD_ResponseTypeR5;
        break;
    case RESP_R6:
        fsl_command.responseType = kCARD_ResponseTypeR6;
        break;
    case RESP_R7:
        fsl_command.responseType = kCARD_ResponseTypeR7;
        break;

    default:
        RT_ASSERT(RT_NULL);
    }

    fsl_command.flags = 0;

    fsl_content.command = &fsl_command;



    data = cmd->data;
    if (data) {
        if (req->stop != RT_NULL) {
            fsl_data.enableAutoCommand12 = true;
        } else {
            fsl_data.enableAutoCommand12 = false;
        }

        fsl_data.enableAutoCommand23 = false;

        fsl_data.enableIgnoreError = false;
        fsl_data.blockSize = data->blksize;
        fsl_data.blockCount = data->blks;

        LOG_D(" blksize:%d, blks:%d ", fsl_data.blockSize, fsl_data.blockCount);

        if (((rt_uint32_t)data->buf & (CACHE_LINESIZE - 1)) ||
            ((rt_uint32_t)data->buf > 0x00000000 && (rt_uint32_t)data->buf < 0x00080000)) {
            buf = rt_malloc_align(fsl_data.blockSize * fsl_data.blockCount, CACHE_LINESIZE);
            RT_ASSERT(buf != RT_NULL);

            LOG_D(" malloc buf: %p, data->buf:%p, %d ", buf, data->buf, fsl_data.blockSize * fsl_data.blockCount);
        }

        if ((cmd->cmd_code == WRITE_BLOCK) || (cmd->cmd_code == WRITE_MULTIPLE_BLOCK)) {
            if (buf) {
                LOG_D(" write(data->buf to buf) ");
                rt_memcpy(buf, data->buf, fsl_data.blockSize * fsl_data.blockCount);
                fsl_data.txData = (const uint32_t*)buf;
            } else {
                fsl_data.txData = (const uint32_t*)data->buf;
            }

            fsl_data.rxData = RT_NULL;
        } else {
            if (buf) {
                fsl_data.rxData = (uint32_t*)buf;
            } else {
                fsl_data.rxData = (uint32_t*)data->buf;
            }
            fsl_data.txData = RT_NULL;
        }
        fsl_content.data = &fsl_data;
    } else {
        fsl_content.data = NULL;
    }

    err_no = USDHC_TransferBlocking(dev->usdhc_host.base, &dma_cfg, &fsl_content);
    if (err_no == kStatus_Fail) {
         _sdmmc_host_error_recovery(dev->usdhc_host.base);
        LOG_D(" ***USDHC_TransferBlocking error: %d*** --> \n", err_no);
        cmd->err = -RT_ERROR;
    }

    if (buf) {
        if (fsl_data.rxData) {
            LOG_D("read copy buf to data->buf ");
            rt_memcpy(data->buf, buf, fsl_data.blockSize * fsl_data.blockCount);
        }
        rt_free_align(buf);
    }

    if ( (cmd->flags & RESP_MASK) == RESP_R2 ) {
        cmd->resp[3] = fsl_command.response[0];
        cmd->resp[2] = fsl_command.response[1];
        cmd->resp[1] = fsl_command.response[2];
        cmd->resp[0] = fsl_command.response[3];

        LOG_D(" resp 0x%08X 0x%08X 0x%08X 0x%08X\n", cmd->resp[0], cmd->resp[1], cmd->resp[2], cmd->resp[3]);
    } else {
        cmd->resp[0] = fsl_command.response[0];
        LOG_D(" resp 0x%08X\n", cmd->resp[0]);
    }

    mmcsd_req_complete(host);

    return;
}


/**
 * @brief           设置sdio的时钟和位宽
 * 
 * @param host      RTT的sdio设备
 * @param io_cfg    RTT的配置
 */
static void _mmc_set_iocfg(struct rt_mmcsd_host* host, struct rt_mmcsd_io_cfg* io_cfg)
{
    struct imx6ull_sdio_device* dev;
    unsigned int                usdhc_clk = 0;
    unsigned int                actual_clk = 0;
    unsigned int                bus_width = 0;
    unsigned int                src_clk   = 0;

    dev = (struct imx6ull_sdio_device*)host->private_data;
    RT_ASSERT(dev != RT_NULL);


    usdhc_clk = io_cfg->clock;
    bus_width = io_cfg->bus_width;

    if(usdhc_clk > IMXRT_MAX_FREQ) {
        usdhc_clk = IMXRT_MAX_FREQ;
    }
    src_clk = CLOCK_GetSysPfdFreq(kCLOCK_Pfd2) / (CLOCK_GetDiv(dev->cfg->usdhc_div) + 1U);

    if(usdhc_clk) {
        actual_clk = USDHC_SetSdClock(dev->usdhc_host.base, src_clk, usdhc_clk);
        LOG_D("[sdio cfg] requested_clk=%u, actual_clk=%u", usdhc_clk, actual_clk);
        
        if(bus_width == MMCSD_BUS_WIDTH_8) {
            USDHC_SetDataBusWidth(dev->usdhc_host.base, kUSDHC_DataBusWidth8Bit);
        } else if (bus_width == MMCSD_BUS_WIDTH_4) {
            USDHC_SetDataBusWidth(dev->usdhc_host.base, kUSDHC_DataBusWidth4Bit);
        } else if (bus_width == MMCSD_BUS_WIDTH_1) {
            USDHC_SetDataBusWidth(dev->usdhc_host.base, kUSDHC_DataBusWidth1Bit);
        } else {
            RT_ASSERT(RT_NULL);
        }
    }
    return;
}


static const struct rt_mmcsd_host_ops _ops = {
    .request                = _mmc_request,
    .set_iocfg              = _mmc_set_iocfg,
    .get_card_status        = RT_NULL,
    .enable_sdio_irq        = RT_NULL,
    .card_busy              = RT_NULL,
    .signal_voltage_switch  = RT_NULL,
};

/************************************* rt sdio ops***********************************/





static int imx6ull_sdio_device_init(void)
{
#ifdef RT_USING_SDIO1
    struct imx6ull_sdio_device* sdio1_device = RT_NULL;
    struct rt_mmcsd_host*       host1        = RT_NULL;


    host1 = mmcsd_alloc_host();
    if (host1 == RT_NULL) {
        LOG_E("alloc host fail");
        return -RT_ERROR;
    }

    sdio1_device = rt_malloc(sizeof(struct imx6ull_sdio_device));
    if (sdio1_device == RT_NULL) {
        LOG_E("malloc sdio1_device fail");
        goto err;
    }
    rt_memset(sdio1_device, 0, sizeof(struct imx6ull_sdio_device));


    sdio1_device->cfg               = &sdio1_cfg;
    //sdio1_device->usdhc_host.base   = (USDHC_Type*)rt_ioremap((void*)sdio1_device->cfg->base, 0x1000);
    sdio1_device->usdhc_host.base   = (USDHC_Type*)sdio1_device->cfg->base;
    sdio1_device->usdhc_adma2_table = _usdhc_adma2_table;


    strncpy(host1->name, "sd", sizeof(host1->name) - 1);
    host1->ops              = &_ops;
    host1->freq_min         = 375000;
    host1->freq_max         = 25000000;
    host1->valid_ocr        = VDD_32_33 | VDD_33_34;
    host1->flags            = MMCSD_BUSWIDTH_4    | MMCSD_MUTBLKWRITE \
                            | MMCSD_SUP_HIGHSPEED | MMCSD_SUP_SDIO_IRQ; /* [todo] */
    host1->max_seg_size     = 65535;
    host1->max_dma_segs     = 2;
    host1->max_blk_size     = 512;
    host1->max_blk_count    = 4096;

    _imx6ull_sdio_io_init(sdio1_device);    // io mux pad
    _imx6ull_sdio_clk_init(sdio1_device);   // clk
    _imx6ull_sdio_init(sdio1_device);       // imx sdio ctrl


    sdio1_device->host  = host1;
    host1->private_data = sdio1_device;

    mmcsd_change(host1);

#endif

#ifdef RT_USING_SDIO2
#endif

    return RT_EOK;

err:
#ifdef RT_USING_SDIO1
    mmcsd_free_host(host1);
#endif

    return -RT_ENOMEM;
}

//INIT_DEVICE_EXPORT(imx6ull_sdio_device_init); /* [qemu] x */
