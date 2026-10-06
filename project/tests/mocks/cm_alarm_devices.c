/*------------------------------------------includes--------------------------------------------*/
#include "cm_gpio.h"
#include "cm_iomux.h"
#include "cm_pwm.h"
#include "cm_adc.h"
#include "ml307y/diag_uart.h"
#include "ml307y/base_gpio.h"
#include <stdarg.h>
#include <stdio.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
unsigned alarm_mock_led_pin_touches;
unsigned alarm_mock_gpio_writes;
unsigned alarm_mock_pwm_writes;
unsigned alarm_mock_pwm_closes;
bool alarm_mock_pwm_running;
cm_pwm_dev_e alarm_mock_pwm_device;
uint32_t alarm_mock_pwm_period;
uint32_t alarm_mock_pwm_high;
int alarm_mock_pwm_error;
unsigned alarm_mock_key_iomux_calls;
unsigned alarm_mock_key_gpio_inits;
unsigned alarm_mock_key_gpio_reads;
unsigned alarm_mock_key_interrupts;
int alarm_mock_key_init_error;
int alarm_mock_key_read_error;
int alarm_mock_key_wakeup_error;
void (*alarm_mock_key_notify)(void *);
void *alarm_mock_key_argument;
int alarm_mock_led_init_error;
int alarm_mock_led_write_error;
unsigned alarm_mock_led_writes;
bool alarm_mock_led_on;
cm_gpio_level_e alarm_mock_key_level = CM_GPIO_LEVEL_HIGH;
char alarm_mock_last_diagnostic[128];
uint32_t alarm_mock_battery_millivolts = 3700U;
int alarm_mock_battery_error;
static cm_iomux_func_e alarm_mock_buzzer_function;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : project_button_input_init
* Description    : 模拟底包按键输入初始化结果
* Input          : 无
* Output         : 初始化计数
* Return         : 注入的初始化返回码
* Attention      : 不模拟 CM GPIO26；真实 HAL ABI 由 test_base_gpio 验证
*******************************************************************************/
int project_button_input_init(void)
{
    ++alarm_mock_key_gpio_inits;
    return alarm_mock_key_init_error;
}

/*******************************************************************************
* Function Name  : project_button_input_read
* Description    : 模拟按键高低电平及读取失败
* Input          : pressed - 输出地址
* Output         : 成功时输出低电平按下状态
* Return         : 0 或注入的读取错误
* Attention      : 失败不能覆盖调用者原有电平
*******************************************************************************/
int project_button_input_read(bool *pressed)
{
    ++alarm_mock_key_gpio_reads;
    if (alarm_mock_key_read_error)
    {
        return alarm_mock_key_read_error;
    }
    *pressed = alarm_mock_key_level == CM_GPIO_LEVEL_LOW;
    return 0;
}

/*******************************************************************************
* Function Name  : project_button_wakeup_configure
* Description    : 模拟唤醒配置成功和失败
* Input          : notify/argument - 按键通知
* Output         : 可由测试触发的回调
* Return         : 注入错误码
* Attention      : 不代表物理脚唤醒已经验证
*******************************************************************************/
int project_button_wakeup_configure(void (*notify)(void *), void *argument)
{
    alarm_mock_key_notify = notify;
    alarm_mock_key_argument = argument;
    return alarm_mock_key_wakeup_error;
}

/*******************************************************************************
* Function Name  : project_led_output_init
* Description    : 模拟 LED 初始关闭和初始化失败
* Input          : 无
* Output         : 成功时 LED 关闭
* Return         : 注入的初始化返回码
* Attention      : 器件测试不操作实际硬件
*******************************************************************************/
int project_led_output_init(void)
{
    if (!alarm_mock_led_init_error)
    {
        alarm_mock_led_on = false;
    }
    return alarm_mock_led_init_error;
}

/*******************************************************************************
* Function Name  : project_led_output_write
* Description    : 模拟 LED 输出及写入失败
* Input          : on - 目标亮灭
* Output         : 成功时更新亮灭状态和写入计数
* Return         : 注入的写入返回码
* Attention      : 失败保留旧状态，供器件重试测试使用
*******************************************************************************/
int project_led_output_write(bool on)
{
    ++alarm_mock_led_writes;
    if (!alarm_mock_led_write_error)
    {
        alarm_mock_led_on = on;
    }
    return alarm_mock_led_write_error;
}

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
    if ((int)pin == 96)
    {
        ++alarm_mock_led_pin_touches;
    }
    if (pin == CM_IOMUX_PIN_26)
    {
        ++alarm_mock_key_iomux_calls;
        /* 当前底包的 26 脚掩码为 0x02，只接受功能 1。 */
        return fun == CM_IOMUX_FUNC_FUNCTION1 ? 0 : -1;
    }
    if (pin == CM_IOMUX_PIN_74)
    {
        alarm_mock_buzzer_function = fun;
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
    /* 底包映射表只有 18 项，测试不得让表外编号假成功。 */
    if ((unsigned)gpio_num > 17U || !cfg)
    {
        return -3;
    }
    if (gpio_num == CM_GPIO_NUM_16 && alarm_mock_buzzer_function != CM_IOMUX_FUNC_FUNCTION3)
    {
        return -3;
    }
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
    if ((unsigned)gpio_num > 17U || !level)
    {
        return -3;
    }
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
* Description    : 核验 PWM0 复用和 SDK 周期范围，记录输出并支持失败注入
* Input          : dev/period/period_h - 模拟 SDK 参数
* Output         : 调用次数、设备号、周期与高电平时间
* Return         : 0 - 成功；负数 - 参数错误或注入失败
* Attention      : 仅链接主机测试
*******************************************************************************/
int32_t cm_pwm_open_ns(cm_pwm_dev_e dev, uint32_t period, uint32_t period_h)
{
    ++alarm_mock_pwm_writes;
    alarm_mock_pwm_device = dev;
    alarm_mock_pwm_period = period;
    alarm_mock_pwm_high = period_h;
    if (dev != CM_PWM_DEV_0 || alarm_mock_buzzer_function != CM_IOMUX_FUNC_FUNCTION1 ||
        period < 63U || period > 5120000U || period_h > period ||
        (period_h != 0U && period_h < 32U))
    {
        return -1;
    }
    if (!alarm_mock_pwm_error)
    {
        alarm_mock_pwm_running = true;
    }
    return alarm_mock_pwm_error;
}

/*******************************************************************************
* Function Name  : cm_pwm_close
* Description    : 模拟停止 PWM 时钟并注入关闭失败
* Input          : dev - PWM 设备
* Output         : 关闭次数及运行状态
* Return         : 0 或注入错误
* Attention      : 零占空比不等于关闭设备
*******************************************************************************/
int32_t cm_pwm_close(cm_pwm_dev_e dev)
{
    if (dev != CM_PWM_DEV_0)
    {
        return -1;
    }
    ++alarm_mock_pwm_closes;
    if (!alarm_mock_pwm_error)
    {
        alarm_mock_pwm_running = false;
    }
    return alarm_mock_pwm_error;
}

/*******************************************************************************
* Function Name  : cm_adc_vbat_read
* Description    : 模拟内部电池电压或采样失败
* Input          : voltage - 输出地址
* Output         : voltage - 成功时填写可控毫伏值
* Return         : 配置的 SDK 返回码
* Attention      : 仅链接主机测试
*******************************************************************************/
int32_t cm_adc_vbat_read(uint32_t *voltage)
{
    if (alarm_mock_battery_error)
    {
        return alarm_mock_battery_error;
    }
    *voltage = alarm_mock_battery_millivolts;
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
