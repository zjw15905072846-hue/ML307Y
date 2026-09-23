/*------------------------------------------includes--------------------------------------------*/
#include "ml307y/diag_uart.h"
#include "cm_iomux.h"
#include "cm_os.h"
#include "cm_uart.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

/*-------------------------------------------define---------------------------------------------*/
#define ML_UART_DIAG_TEXT_SIZE 256U /* 单条诊断含 CRLF 的最大缓冲字节数。 */

/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
static osMutexId_t s_uart_diag_lock; /* 串行化多任务写入，防止文本交叉。 */
static bool s_uart_diag_ready;       /* 串口打开后才允许输出。 */

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : ml_uart_diag_init
* Description    : 将模组17/18脚复用为UART0并配置115200、8N1、无流控
* Input          : 无
* Output         : UART0及诊断输出互斥锁
* Return         : 0成功；负值表示互斥锁、引脚复用或串口打开失败
* Attention      : 只在产品启动任务中调用；引脚复用失败无法自动恢复原功能
*******************************************************************************/
int ml_uart_diag_init(void)
{
    cm_uart_cfg_t config = {0};
    int result;

    if (s_uart_diag_ready)
    {
        return 0;
    }
    s_uart_diag_lock = osMutexNew(NULL);
    if (!s_uart_diag_lock)
    {
        return -1;
    }
    /* 两个管脚都完成 UART 复用后再打开设备。 */
    if (cm_iomux_set_pin_func(CM_IOMUX_PIN_17, CM_IOMUX_FUNC_FUNCTION1) != 0 ||
        cm_iomux_set_pin_func(CM_IOMUX_PIN_18, CM_IOMUX_FUNC_FUNCTION1) != 0)
    {
        osMutexDelete(s_uart_diag_lock);
        s_uart_diag_lock = NULL;
        return -1;
    }
    config.byte_size = CM_UART_BYTE_SIZE_8;
    config.parity = CM_UART_PARITY_NONE;
    config.stop_bit = CM_UART_STOP_BIT_ONE;
    config.flow_ctrl = CM_UART_FLOW_CTRL_NONE;
    config.baudrate = CM_UART_BAUDRATE_115200;
    result = cm_uart_open(CM_UART_DEV_0, &config);
    if (result != 0)
    {
        osMutexDelete(s_uart_diag_lock);
        s_uart_diag_lock = NULL;
        return result;
    }
    s_uart_diag_ready = true;
    return 0;
}

/*******************************************************************************
* Function Name  : ml_uart_diag_printf
* Description    : 格式化并完整发送一行SSCOM可直接显示的串口文本
* Input          : format - printf格式字符串；其余参数 - 格式化参数
* Output         : UART0发送以CRLF结尾的文本
* Return         : 成功发送的字节数；负值表示未初始化、文本过长或写入失败
* Attention      : 仅供普通任务调用，不在UART回调或中断内调用；禁止输出凭据
*******************************************************************************/
int ml_uart_diag_printf(const char *format, ...)
{
    char text[ML_UART_DIAG_TEXT_SIZE];
    va_list arguments;
    int length;
    int offset = 0;

    if (!s_uart_diag_ready || !format)
    {
        return -1;
    }
    va_start(arguments, format);
    length = vsnprintf(text, sizeof(text) - 2U, format, arguments);
    va_end(arguments);
    if (length < 0 || (unsigned)length >= sizeof(text) - 2U)
    {
        return -1;
    }
    /* SSCOM 使用 CRLF 分行；预留空间在格式化阶段已经检查。 */
    text[length++] = '\r';
    text[length++] = '\n';
    if (osMutexAcquire(s_uart_diag_lock, osWaitForever) != osOK)
    {
        return -1;
    }
    /* SDK 可能只写入部分字节，循环直到整行发出或明确失败。 */
    while (offset < length)
    {
        int written = cm_uart_write(CM_UART_DEV_0, text + offset, length - offset, 100);
        if (written <= 0 || written > length - offset)
        {
            osMutexRelease(s_uart_diag_lock);
            return written < 0 ? written : -1;
        }
        offset += written;
    }
    osMutexRelease(s_uart_diag_lock);
    return offset;
}
