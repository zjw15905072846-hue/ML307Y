/**********************************************************************/
/**
 * @file cm_demo_sif2.c
 * @copyright Copyright (c) 2025-2025 
 * @author xinyisemi
 * @date 2026-01-19
 * @version V1.0
 * @brief cm sif接口测试模块
 **********************************************************************/

#define CM_SIF_DEMO_EN (1)

#if CM_SIF_DEMO_EN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cm_sys.h"
#include "cm_demo_uart.h"
#include "embedded_cli.h"
#include "cm_sif.h"

#define STR_LEN(s) (sizeof(s) - 1)
#define STR_ITEM(s) (s), STR_LEN(s)

#define SIF_TEST_WRITE (0)  // 1:测试发送； 0：测试接收
#define SIF_TEST_DATA_LEN (64)

static uint8_t s_sif_write_buf[SIF_TEST_DATA_LEN] = 
{
    0x55, 0x2a, 0xff, 0x33, 0x22, 0x11 , 0x9, 0x1, 0xA5, 0x88 , 0x66, 0x99, 0x00, 0x01, 0x02, 0x03, 
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f
};
static uint8_t s_sif_read_buf[SIF_TEST_DATA_LEN] = {0};
extern cm_sif_cfg_t s_cm_sif_cfg;
void sif_read_rb(cm_sif_id_e sif_id, uint8_t *data, uint32_t len)
{
    if (len == 0)
    {
        cm_log_printf(0, "[sif2_test] sif_read_rb: read_len is 0\r\n");
    }
    else
    {
        memcpy(s_sif_read_buf, data, len);
        for (uint32_t i = 0; i < len; i++)
        {
            cm_log_printf(0, "[sif2_test] s_sif_read_buf[%u]=0x%x", i, s_sif_read_buf[i]);
        }
    }
}

cm_sif_cfg_t s_cm_sif_cfg = {
    .tx_gpio = CM_GPIO_NUM_15,
    .rx_gpio = CM_GPIO_NUM_14,
    .bit_order_type = CM_SIF_BIT_ORDER_MBS,
    .rx_start_level = CM_SIF_LEVEL_LOW,
    .rx_stop_level = CM_SIF_LEVEL_LOW,
    .tx_start_level = CM_SIF_LEVEL_LOW,
    .tx_stop_level = CM_SIF_LEVEL_LOW,
    .rx_invert_level = CM_SIF_LEVEL_NORMAL,
    .tx_invert_level = CM_SIF_LEVEL_NORMAL,
    .rx_data_invert = CM_SIF_DATA_NORMAL,
    .tx_data_invert = CM_SIF_DATA_NORMAL,
    .sync_level_long_time_us = 50000,
    .sync_level_short_time_us = 500,
    .logic_short_time_us = 500,
    .logic_long_time_us = 1000,
    .stop_level_time_us = 10000,
    .rx_ring_buffer_size = 64,
    .rx_recv_timeout = 0,                    /*!< 接收超时时间，单位ms */
    .recv_cb = sif_read_rb,
};

void cm_sif_write()
{
    cm_sif_slave_init(CM_SIF_ID_0, &s_cm_sif_cfg);
    cm_log_printf(0, "[sif2_test] sif_demo_write!!\n");

    while (1)
    {
        osDelay(50);
        cm_sif_slave_write(CM_SIF_ID_0, s_sif_write_buf, sizeof(s_sif_write_buf));
    }
}

void cm_sif_read()
{
    cm_sif_slave_init(CM_SIF_ID_0, &s_cm_sif_cfg);

    cm_log_printf(0, "[sif2_test] sif_demo_read!!\n");
    
    while (1)
    {
        osDelay(1000);
    }
}

void cm_test_sif2(EmbeddedCli *cli, char *args, void *context)
{
    const char *cmd = embeddedCliGetToken(args, 1);
    if (cmd == NULL)
    {
        cm_demo_printf("invalid param\n");
        return;
    }

    if (strncmp(cmd, STR_ITEM("read")) == 0)
    {
        osThreadAttr_t thread_attr = {0};
        thread_attr.name = "cm_sif_read";
        thread_attr.priority = osPriorityNormal2;
        thread_attr.stack_size = 0x1000;
        osThreadNew(cm_sif_read, NULL, &thread_attr);
    }
    else if(strncmp(cmd, STR_ITEM("write")) == 0)
    {
        osThreadAttr_t thread_attr2 = {0};
        thread_attr2.name = "cm_sif_write";
        thread_attr2.priority = osPriorityNormal1;
        thread_attr2.stack_size = 0x1000;
        osThreadNew(cm_sif_write, NULL, &thread_attr2);
    }

    else
    {
        cm_demo_printf("invalid cmd=%s error\n",cmd);
    }
}

#endif /* CM_SIF_DEMO_EN */
