/*------------------------------------------includes--------------------------------------------*/
#include "ml307y/alarm_led.h"
#include "ml307y/diag_uart.h"
#include "ml307y/base_gpio.h"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    bool initialized;
    bool enabled;
    bool on;
} alarm_led_device_t;

/*-------------------------------------------variables-------------------------------------------*/
static alarm_led_device_t alarm_led_device;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : ml307y_alarm_led_set
* Description    : 通过底包设置物理 96 脚的 LED 亮灭状态
* Input          : user - LED 器件；on - 目标状态
* Output         : LED 高电平亮、低电平灭
* Return         : true - 状态已应用或显式禁用；false - 未初始化或写入失败
* Attention      : 仅成功后缓存状态，失败时允许下一次调用重试
*******************************************************************************/
static bool ml307y_alarm_led_set(void *user, bool on)
{
    alarm_led_device_t *device = user;
    if (!device || !device->initialized)
    {
        return false;
    }
    if (!device->enabled || device->on == on)
    {
        return true;
    }
    if (project_led_output_write(on) != 0)
    {
        return false;
    }
    device->on = on;
    return true;
}

/*******************************************************************************
* Function Name  : ml307y_alarm_led_init
* Description    : 为物理 96 脚绑定已初始化且关闭的 LED 输出
* Input          : led - 产品 LED 接口；sdk_pin - 当前 HAL GPIO_PIN_B 或禁用值 -1
* Output         : 成功后绑定 LED 输出接口
* Return         : true - 输出就绪；false - 参数错误、重复初始化或 HAL 失败
* Attention      : HAL GPIO_PIN_B 不是 CM GPIO 编号；96 脚不用于 ADC 电池采样
*******************************************************************************/
bool ml307y_alarm_led_init(alarm_led_interface_t *led, int sdk_pin)
{
    int error;
    if (!led)
    {
        ml307y_uart_diag_printf("[project][alarm-led-interface] error=-1");
        return false;
    }
    if (alarm_led_device.initialized)
    {
        ml307y_uart_diag_printf("[project][alarm-led-already-initialized] error=-1");
        return false;
    }
    if (sdk_pin != -1 && sdk_pin != ML307Y_LED_HAL_PIN)
    {
        ml307y_uart_diag_printf("[project][alarm-led-pin] sdk-pin=%d", sdk_pin);
        return false;
    }
    if (sdk_pin != -1)
    {
        ml307y_uart_diag_printf("[project] alarm LED init pin=96 hal=GPIO_PIN_B active-high");
        error = project_led_output_init();
        if (error != 0)
        {
            ml307y_uart_diag_printf("[project][alarm-led-hal-init] error=%d", error);
            return false;
        }
    }
    alarm_led_device.enabled = sdk_pin != -1;
    alarm_led_device.on = false;
    alarm_led_device.initialized = true;
    led->user = &alarm_led_device;
    led->set = ml307y_alarm_led_set;
    led->ready = true;
    return true;
}
