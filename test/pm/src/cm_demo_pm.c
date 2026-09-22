/*********************************************************
 *  @file    cm_demo_pm.c
 *  @brief   OpenCPU PM示例
 *  Copyright (c) 2021 China Mobile IOT.
 *  All rights reserved.
 *  created by zyf 2021/08/30
 ********************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include "stdlib.h"
#include "cm_sys.h"
#include "cm_pm.h"
#include "cm_demo_pm.h"
#include "cm_os.h"
#include "cm_demo_uart.h"
#include "cm_iomux.h"
#include "cm_gpio.h"
#include "cm_uart.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define CM_PM_MAX_NUM 10
#define STR_LEN(s) (sizeof(s) - 1)
#define STR_ITEM(s) (s), STR_LEN(s)

#define WAKEUP_OUT_GPIO         CM_GPIO_NUM_0  // 唤醒GPIO（输出），可以使用GPIO0作为唤醒输出
#define WAKEUP_OUT_PIN          CM_IOMUX_PIN_16  // 唤醒引脚（输出）,可以使用PIN16作为唤醒输出

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static cm_pm_sleepind_cb test_pm_callback[CM_PM_MAX_NUM] = {NULL};
static uint8_t wake_out_init_flag = 0;
static uint8_t cm_pm_lock_flag = 0;
/****************************************************************************
 * Private Functions
 ****************************************************************************/
static int parse_params(const char *args, const char **param, int param_num)
{
	int i = 0;
    for(i = 0; i < param_num; i++)
    {
        param[i] = embeddedCliGetToken(args, i + 2);
        if (param[i] == NULL)
        {
            cm_demo_printf("invalid param[%d]\n", i);
            return -1;
        }
    }
    return 0;
}

static void cm_pm_sleepind_callback(char status, int32_t source)
{
    int i;

    
    for(i = 0;i < CM_PM_MAX_NUM;i++)
    {
        if(test_pm_callback[i] != NULL)
        {
            test_pm_callback[i](status, source);
        }
    }

    if ((status == CM_PM_SLEEP_STATUS_ACTIVE) && (1 == wake_out_init_flag))
    {
        cm_gpio_set_level(WAKEUP_OUT_GPIO, 1);
    }
    else if ((status == CM_PM_SLEEP_STATUS_DEEP) && (1 == wake_out_init_flag))
    {
        cm_gpio_set_level(WAKEUP_OUT_GPIO, 0);
    }

}

static void cm_test_pm_init(void)
{
    int i=0;
    
    for(i = 0;i < CM_PM_MAX_NUM;i++)
    {
        test_pm_callback[i] = NULL;
    }
}

static void __init_status_pins(void)
{
    cm_gpio_cfg_t cfg = {0};
    int32_t ret = 0;

    cfg.direction = CM_GPIO_DIRECTION_OUTPUT;
    cfg.pull = CM_GPIO_PULL_DOWN;

    ret = cm_iomux_set_pin_func(WAKEUP_OUT_PIN, CM_IOMUX_FUNC_FUNCTION1);
    if (ret != 0)
    {
        cm_demo_printf("cm_iomux_set_pin_func ret=%d,error\r\n ", ret);
        return;
    }
    
    ret = cm_gpio_init(WAKEUP_OUT_GPIO, &cfg);
    if (ret != 0)
    {
        cm_demo_printf("cm_gpio_init ret=%d,error\r\n ", ret);
        return;
    }

    wake_out_init_flag = 1;

    // 默认设置唤醒输出引脚为高电平（唤醒状态）
    cm_gpio_set_level(WAKEUP_OUT_GPIO, 1);   

}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
void cm_test_pm(EmbeddedCli *cli, char *args, void *context)
{
    const char *cmd = embeddedCliGetToken(args, 1);
    if (cmd == NULL)
    {
        cm_demo_printf("invalid param\n");
        return;
    }

    if (strncmp(cmd, STR_ITEM("reboot")) == 0)
    {
        uint8_t type = 0;
        const char *param[1] = {NULL};
        int param_num = 1;

        if (parse_params(args, param, param_num) != 0)
        {
            return;
        }

        type = atoi((char *)param[0]);
        cm_demo_printf("[REBOOT]rebooting...\n");
        if(cm_uart_is_sending(OPENCPU_MAIN_URAT) == 1) 
        {
            osDelay(50); //等待串口数据发送完成
        }
        cm_pm_reboot(type);
    }
    else if (strncmp(cmd, STR_ITEM("poweroff")) == 0)
    {
        cm_demo_printf("[POWEROFF]poweroff...\n");
        if(cm_uart_is_sending(OPENCPU_MAIN_URAT) == 1) 
        {
            osDelay(50); //等待串口数据发送完成
        }
        cm_pm_poweroff();
    }
    else if (strncmp(cmd, STR_ITEM("power_on_reason")) == 0)
    {
        cm_demo_printf("[PM]power on reason:%d\n", cm_pm_get_power_on_reason());
    }
    else if (strncmp(cmd, STR_ITEM("sleep")) == 0)
    {
        uint8_t mode = 0;
        const char *param[1] = {NULL};
        int param_num = 1;

        if (parse_params(args, param, param_num) != 0)
        {
            return;
        }

        mode = atoi((char *)param[0]);
        cm_pm_set_cfg(CM_PM_CFG_SLEEPIND, cm_pm_sleepind_callback);
        cm_test_pm_init();
        
        if((mode == 1) && (cm_pm_lock_flag ==1))
        {
            cm_pm_work_unlock();
            cm_pm_lock_flag = 0;
        }
        else if((mode == 2) && (cm_pm_lock_flag ==0))
        {
            cm_pm_work_lock();
            cm_pm_lock_flag = 1;
        }
        else
        {
            cm_demo_printf("[PM]dormant mode(%d) is error...\n",mode);
        }
    }
    else if(strncmp(cmd, STR_ITEM("wake")) == 0) 
    {
        __init_status_pins();//wakein/wakeout初始化
        wake_out_init_flag = 1;
    }
    else
    {
        cm_demo_printf("invalid cmd=%s error\n",cmd);
    }
}