/*------------------------------------------includes--------------------------------------------*/
#include "ml307y/alarm_led.h"
#include "ml307y/diag_uart.h"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    bool initialized;
} alarm_led_device_t;

/*-------------------------------------------variables-------------------------------------------*/
static alarm_led_device_t alarm_led_device;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : ml307y_alarm_led_set
* Description    : LED 禁用期间接受状态请求而不触碰引脚
* Input          : user - LED 器件；on - 目标状态
* Output         : 无物理输出
* Return         : true - 禁用状态下安全忽略；false - 尚未初始化
* Attention      : 禁用 LED 时不触碰物理 96 脚
*******************************************************************************/
static bool ml307y_alarm_led_set(void *user, bool on)
{
    alarm_led_device_t *device = user;
    if (!device || !device->initialized)
    {
        return false;
    }
    (void)on;
    return true;
}

/*******************************************************************************
* Function Name  : ml307y_alarm_led_init
* Description    : 为暂时禁用的 LED 绑定安全空输出
* Input          : led - 产品 LED 接口；sdk_pin - 必须为禁用值 -1
* Output         : 成功后绑定 LED 输出接口
* Return         : true - 安全空输出就绪；false - 请求启用或重复初始化
* Attention      : sdk_pin 为 -1 时只提供安全空输出，不访问物理 96 脚
*******************************************************************************/
bool ml307y_alarm_led_init(alarm_led_interface_t *led, int sdk_pin)
{
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
    if (sdk_pin != -1)
    {
        ml307y_uart_diag_printf("[project][alarm-led-disabled] sdk-pin=%d", sdk_pin);
        return false;
    }
    alarm_led_device.initialized = true;
    led->user = &alarm_led_device;
    led->set = ml307y_alarm_led_set;
    led->ready = true;
    return true;
}
