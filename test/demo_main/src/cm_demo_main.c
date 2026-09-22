/**
 *  @file    cm_demo_main.c
 *  @brief   demo main
 *  @copyright copyright © 2025 China Mobile IOT. All rights reserved.
 *  @author by cmiot0752
 *  @date 2025/05/20
 *  
 */


#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "cm_sys.h"
#include "cm_os.h"
#include "cm_fs.h"
#include "cm_mem.h"
#include "cm_sys.h"
#include "cm_sim.h"
#include "cm_modem.h"
#include "cm_uart.h"
#include "embedded_cli.h"

#include "cm_demo_common.h"
#include "cm_demo_main.h"
#include "cm_demo_fs.h"
#include "cm_demo_ftp.h"
#include "cm_demo_ntp.h"
#include "cm_demo_http.h"
#include "cm_demo_modem.h"
#include "cm_demo_sd.h"
#include "cm_demo_sys.h"
#include "cm_demo_uart.h"
#include "cm_demo_virt_at.h"
#include "cm_demo_sms.h"
#include "cm_demo_asocket.h"
#include "cm_demo_mqtt.h"
#include "cm_demo_ssl.h"
#include "cm_demo_gpio.h"
#include "cm_demo_powerkey.h"
#include "cm_demo_pwm.h"
#include "cm_demo_pm.h"
#include "cm_demo_adc.h"
#include "cm_demo_i2c.h"
#include "cm_demo_spi.h"
#include "cm_demo_fota.h"
#include "cm_demo_spi_lcd.h"
#include "cm_demo_socket.h"
#include "cm_demo_ping.h"
#include "cm_demo_rtc.h"
#include "cm_demo_deepseek.h"
#include "cm_demo_audio.h"
#include "cm_demo_lwip.h"
#include "cm_demo_sif2.h"
#include "cm_test_uart.h"
#include "cm_demo_lwm2m.h"

osEventFlagsId_t g_cmd_task_flag = NULL;
static osThreadId_t s_task_handle = NULL;
static char s_uart_rcv_buf[1024] = {0};

/** fill cmd table*/
static CliCommandBinding s_test_cmds[] = {
    //{"sys", "sys <cmd> [<param>]", true, NULL, cm_test_sys},
    //{"sd", "sd <cmd>", true, NULL, cm_test_sd},
    //{"ftp", "ftp", true, NULL, cm_test_ftp},
    {"http", "http <cmd>", true, NULL, cm_test_http},
    {"fs", "fs <cmd>", true, NULL, cm_test_fs},                                  //xinyi已测试，正常
    {"ntp", "ntp <cmd>", true, NULL, cm_test_ntp},                               //xinyi已测试，正常
    {"modem", "modem <cmd>", true, NULL, cm_test_modem},
    {"virt_at_sync", "virt_at_sync <cmd>", true, NULL, cm_virt_at_test_sync},    //<cmd> AT 命令
    {"virt_at_async", "virt_at_async <cmd>", true, NULL, cm_virt_at_test_async}, //<cmd> AT 命令
    {"urc", "urc <cmd>", true, NULL, cm_test_urc},                               //<cmd> reg/REG or dereg/DEREG
    {"sms", "sms <cmd>", true, NULL, cm_test_sendmsg},                           //<cmd> pdu/PDU or txt/TXT 
    //{"asocket", "asocket <cmd> <param>", true, NULL, cm_test_asocket},
    //{"ssl", "ssl <cmd> [<param>]", true, NULL, cm_test_ssl},
    {"mqtt", "mqtt <cmd>", true, NULL, cm_test_mqtt},
    {"ota", "ota <cmd>", true, NULL, cm_test_ota},                              //xinyi已测试，正常
    //{"spi_lcd", "spi_lcd", true, NULL, cm_test_spi_lcd},
    {"socket", "socket <cmd>", true, NULL, cm_test_socket},                     //xinyi已测试，正常
    {"ping", "ping <cmd> [<param>]", true, NULL, cm_test_ping},                 //xinyi已测试，正常
    {"rtc", "rtc <cmd>", true, NULL, cm_test_rtc},                              //xinyi已测试，正常   
    {"spi", "spi <cmd>", true, NULL,cm_test_spi},
    {"i2c", "i2c <cmd>", true, NULL,cm_test_i2c},
    {"sif2", "sif2 <cmd>", true, NULL,cm_test_sif2},                            //xinyi已测试，正常 
    {"adc", "adc <cmd>", true, NULL,cm_test_adc},
    {"gpio", "gpio <cmd> [<param>]", true, NULL,cm_test_gpio},
    {"pm", "pm <cmd> [<param>]", true, NULL,cm_test_pm},
    {"pwm", "pwm <cmd> [<param>]", true, NULL,cm_test_pwm},
    {"powerkey", "powerkey <cmd> ", true, NULL,cm_test_powerkey},
    //{"deepseek", "deepseek <cmd>", true, NULL, cm_test_deepseek},
    // {"audio_play", "audio_play <cmd> [<param>]", true, NULL, cm_test_audio_play}, //audio相关占据较多空间，使用时需要打开xy_export.list中的CM_AUDIO_API宏，再编译
    // {"audio_record", "audio_record <cmd> [<param>]", true, NULL, cm_test_audio_record},   
    {"lwip", "lwip <cmd> [<param>]", true, NULL, cm_test_lwip},                 //xinyi已测试，正常 ，测试代码和cm_test_socket重复，建议二者保留有一个即可
    //uart指令与uart_cfg指令配合使用，uart_cfg指令用来配置被测串口的各项参数包括回调格式，uart指令则进行相关输入输出打印
    {"uart", "uart <cmd> [<param>]", true, NULL, cm_test_uart_operation},
    {"uart_cfg", "uart_cfg <cmd> [<param>]", true, NULL, cm_test_uart_cfg},
    {"lwm2m", "lwm2m <cmd> [<param>...]", true, NULL, cm_test_lwm2m},
};

/** 适配embedded-cli输出*/
static void __uart_put_char(EmbeddedCli *cli, char c)
{
    (void)cli;
    cm_uart_write(OPENCPU_MAIN_URAT, (const void *)&c, 1, 1000);
}


/** app线程入口*/
static void __app_task_loop(void *param)
{
    int i = 0;
    int len = 0;
    cm_log_printf(0, "launch cmiot opencpu\n");
    cm_pm_work_lock();//外设使用时需要加锁，否则可能会导致通讯异常
    cm_demo_uart();
    EmbeddedCliConfig *config = embeddedCliDefaultConfig();
    config->enableAutoComplete = false; // disable VT100
    config->cmdBufferSize = 128;
    config->rxBufferSize = 128;
    config->maxBindingCount = 128;
    EmbeddedCli *cli = embeddedCliNew(config);
    cli->writeChar = __uart_put_char;

    /* 注册cli命令*/
    for(i = 0; i < sizeof(s_test_cmds) / sizeof(s_test_cmds[0]); i++)
    {
        embeddedCliAddBinding(cli, s_test_cmds[i]);
    }

    embeddedCliPrint(cli, "launch cmiot opencpu");
    embeddedCliPrint(cli, "cmiot opencpu version 000 start");
    // embeddedCliPrint(cli, "cmiot opencpu version 001 start");
    // embeddedCliPrint(cli, "cmiot opencpu version 002 start");
    while(1)
    {
        osEventFlagsWait(g_cmd_task_flag, 0x00000001U, osFlagsWaitAny, osWaitForever);
        len = cm_uart_read(OPENCPU_MAIN_URAT, (void*)&s_uart_rcv_buf[0], 1024, 1000);

        for(i = 0; i < len; i++)
        {
            embeddedCliReceiveChar(cli, s_uart_rcv_buf[i]);  //
        }

        embeddedCliProcess(cli);
    }
}


/**
 *  @brief OpenCPU程序入口
 *  
 *  @param [in] param 固定传入NULL
 *  
 *  @return 成功返回0，失败返回-1
 *  
 *  @details 禁止阻塞
 */
int cm_opencpu_entry(void *param)
{
    (void)param;
    g_cmd_task_flag = osEventFlagsNew(0);

    if(g_cmd_task_flag == NULL)
    {
        cm_log_printf(0, "event flag init failed");
        return -1;
    }

    osThreadAttr_t task_attr = {0};
    task_attr.name = "app_task";
    task_attr.stack_size = 4096 * 2;
    task_attr.priority = osPriorityBelowNormal;

    s_task_handle = osThreadNew(__app_task_loop, 0, &task_attr);
    return 0;
}
