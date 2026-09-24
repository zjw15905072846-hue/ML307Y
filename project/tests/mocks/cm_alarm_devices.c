/*------------------------------------------includes--------------------------------------------*/
#include "cm_gpio.h"
#include "cm_iomux.h"
#include "cm_pwm.h"
#include "cm_adc.h"
#include "ml307y/diag_uart.h"
#include <stdarg.h>
#include <stdio.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
unsigned alarm_mock_led_pin_touches;
unsigned alarm_mock_gpio_writes;
unsigned alarm_mock_pwm_writes;
unsigned alarm_mock_key_iomux_calls;
unsigned alarm_mock_key_gpio_inits;
unsigned alarm_mock_key_gpio_reads;
unsigned alarm_mock_key_interrupts;
cm_gpio_level_e alarm_mock_key_level = CM_GPIO_LEVEL_HIGH;
char alarm_mock_last_diagnostic[128];

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : cm_iomux_set_pin_func
* Description    : 模拟引脚复用并记录物理 96 脚访问
* Input          : pin/fun - 模拟 SDK 参数
* Output         : 访问计数
* Return         : 0
* Attention      : 仅链接主机测试
*******************************************************************************/
int32_t cm_iomux_set_pin_func(cm_iomux_pin_e pin, cm_iomux_func_e fun)
{
    (void)fun;
    if ((int)pin == 96)
    {
        ++alarm_mock_led_pin_touches;
    }
    if (pin == CM_IOMUX_PIN_26)
    {
        ++alarm_mock_key_iomux_calls;
    }
    return 0;
}

/*******************************************************************************
* Function Name  : cm_gpio_init
* Description    : 模拟 GPIO 初始化
* Input          : gpio_num/cfg - 模拟 SDK 参数
* Output         : 无
* Return         : 0
* Attention      : 仅链接主机测试
*******************************************************************************/
int32_t cm_gpio_init(cm_gpio_num_e gpio_num, cm_gpio_cfg_t *cfg)
{
    if (gpio_num == CM_GPIO_NUM_26)
    {
        ++alarm_mock_key_gpio_inits;
    }
    (void)cfg;
    return 0;
}

/*******************************************************************************
* Function Name  : cm_gpio_set_level
* Description    : 模拟 GPIO 输出
* Input          : gpio_num/level - 模拟 SDK 参数
* Output         : 写入次数
* Return         : 0
* Attention      : 仅链接主机测试
*******************************************************************************/
int32_t cm_gpio_set_level(cm_gpio_num_e gpio_num, cm_gpio_level_e level)
{
    (void)gpio_num;
    (void)level;
    ++alarm_mock_gpio_writes;
    return 0;
}

/*******************************************************************************
* Function Name  : cm_gpio_get_level
* Description    : 模拟未按下的 GPIO 电平
* Input          : gpio_num - 模拟 SDK 参数；level - 输出地址
* Output         : level - 高电平
* Return         : 0
* Attention      : 仅链接主机测试
*******************************************************************************/
int32_t cm_gpio_get_level(cm_gpio_num_e gpio_num, cm_gpio_level_e *level)
{
    if (gpio_num == CM_GPIO_NUM_26)
    {
        ++alarm_mock_key_gpio_reads;
        *level = alarm_mock_key_level;
        return 0;
    }
    *level = CM_GPIO_LEVEL_HIGH;
    return 0;
}

/*******************************************************************************
* Function Name  : cm_gpio_interrupt_register
* Description    : 模拟按键中断注册
* Input          : gpio_num/interrupt_cb - 模拟 SDK 参数
* Output         : 无
* Return         : 0
* Attention      : 仅链接主机测试
*******************************************************************************/
int32_t cm_gpio_interrupt_register(cm_gpio_num_e gpio_num, void *interrupt_cb)
{
    if (gpio_num == CM_GPIO_NUM_26)
    {
        ++alarm_mock_key_interrupts;
    }
    (void)interrupt_cb;
    return 0;
}

/*******************************************************************************
* Function Name  : cm_gpio_interrupt_enable
* Description    : 模拟按键中断使能
* Input          : gpio_num/intr_mode - 模拟 SDK 参数
* Output         : 无
* Return         : 0
* Attention      : 仅链接主机测试
*******************************************************************************/
int32_t cm_gpio_interrupt_enable(cm_gpio_num_e gpio_num, cm_gpio_interrupt_e intr_mode)
{
    (void)gpio_num;
    (void)intr_mode;
    return 0;
}

/*******************************************************************************
* Function Name  : cm_pwm_open_ns
* Description    : 模拟 PWM 输出
* Input          : dev/period/period_h - 模拟 SDK 参数
* Output         : 写入次数
* Return         : 0
* Attention      : 仅链接主机测试
*******************************************************************************/
int32_t cm_pwm_open_ns(cm_pwm_dev_e dev, uint32_t period, uint32_t period_h)
{
    (void)dev;
    (void)period;
    (void)period_h;
    ++alarm_mock_pwm_writes;
    return 0;
}

/*******************************************************************************
* Function Name  : cm_adc_vbat_read
* Description    : 模拟有效内部电池电压
* Input          : voltage - 输出地址
* Output         : voltage - 3700 mV
* Return         : 0
* Attention      : 仅链接主机测试
*******************************************************************************/
int32_t cm_adc_vbat_read(uint32_t *voltage)
{
    *voltage = 3700U;
    return 0;
}

/*******************************************************************************
* Function Name  : ml307y_uart_diag_printf
* Description    : 捕获主机测试中的最后一条串口诊断
* Input          : format - 格式串及可变参数
* Output         : 无
* Return         : 已格式化的字节数
* Attention      : 不模拟真实 UART0 发送
*******************************************************************************/
int ml307y_uart_diag_printf(const char *format, ...)
{
    va_list arguments;
    int length;
    va_start(arguments, format);
    length = vsnprintf(alarm_mock_last_diagnostic, sizeof(alarm_mock_last_diagnostic), format, arguments);
    va_end(arguments);
    return length;
}
