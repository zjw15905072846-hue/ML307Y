/*------------------------------------------includes--------------------------------------------*/
#include "custom_main.h"
#include "cm_os.h"
#include "ml307y/diag_uart.h"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/

/*******************************************************************************
* Function Name  : custom_uart_task
* Description    : 常驻任务初始化 UART0，成功后只打印一次 Hello World
* Input          : argument - 保留参数
* Output         : 串口文本
* Return         : 不返回
* Attention      : 初始化失败时按间隔重试，循环中必须让出 CPU
*******************************************************************************/
static void custom_uart_task(void *argument)
{
    int printed = 0;

    (void)argument;
    while (1)
    {
        if (!printed && ml307y_uart_diag_init() == 0)
        {
            printed = 1;
            (void)ml307y_uart_diag_printf("Hello World");
        }
        ml307y_uart_diag_printf("11111");
        osDelay(5U * osKernelGetTickFreq());
    }
}

/*******************************************************************************
* Function Name  : cm_opencpu_entry
* Description    : 创建串口任务后立即返回
* Input          : param - SDK 启动参数，本示例未使用
* Output         : 串口任务
* Return         : 0 - 任务创建成功；-1 - 任务创建失败
* Attention      : 入口中不进行串口操作或阻塞等待
*******************************************************************************/
int cm_opencpu_entry(void *param)
{
    osThreadAttr_t attributes = {0};
    (void)param;
    attributes.name = "custom-uart";
    attributes.stack_size = 2048U;
    attributes.priority = osPriorityNormal;
    osThreadNew(custom_uart_task, NULL, &attributes);
    return 0;
}
