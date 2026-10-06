/*------------------------------------------includes--------------------------------------------*/
#include "ml307y/alarm_buzzer.h"
#include "cm_gpio.h"
#include "cm_iomux.h"
#include "cm_pwm.h"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    int sdk_pin;
    uint32_t frequency_hz;
    bool on;
    bool initialized;
} alarm_buzzer_device_t;

/*-------------------------------------------variables-------------------------------------------*/
static alarm_buzzer_device_t alarm_buzzer_device;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : ml307y_alarm_buzzer_set
* Description    : 按已初始化的有源或无源蜂鸣器类型切换输出
* Input          : user - 蜂鸣器器件；on - 目标状态
* Output         : GPIO 电平或 PWM 占空比及已应用状态
* Return         : true - 应用成功；false - SDK 写入失败
* Attention      : 静音关闭 PWM 时钟；发声恢复原宏配置，关闭失败不得标记静音
*******************************************************************************/
static bool ml307y_alarm_buzzer_set(void *user, bool on)
{
    alarm_buzzer_device_t *device = user;
    uint32_t period;
    bool written;
    if (!device || !device->initialized)
    {
        return false;
    }
    if (device->on == on)
    {
        return true;
    }
    if (device->frequency_hz)
    {
        period = 1000000000U / device->frequency_hz;
        written = on ? cm_pwm_open_ns(CM_PWM_DEV_0, period,
                                      period * ALARM_BUZZER_DUTY_PERCENT / 100U) == 0
                     : cm_pwm_close(CM_PWM_DEV_0) == 0;
    }
    else
    {
        written = cm_gpio_set_level((cm_gpio_num_e)device->sdk_pin,
                                    on ? CM_GPIO_LEVEL_HIGH : CM_GPIO_LEVEL_LOW) == 0;
    }
    if (written)
    {
        device->on = on;
    }
    return written;
}

/*******************************************************************************
* Function Name  : ml307y_alarm_buzzer_init
* Description    : 使用现有 GPIO16/PWM0 配置初始化蜂鸣器为静音状态
* Input          : buzzer - 产品蜂鸣器接口；sdk_pin - CM GPIO 编号；frequency_hz - PWM 频率
* Output         : 成功后绑定蜂鸣器输出接口
* Return         : true - 就绪；false - 配置或 SDK 操作失败
* Attention      : 本板 XCL-5020ATP 使用 PWM；频率零值仅保留给有源 GPIO 兼容调用
*******************************************************************************/
bool ml307y_alarm_buzzer_init(alarm_buzzer_interface_t *buzzer, int sdk_pin, uint32_t frequency_hz)
{
    cm_gpio_cfg_t output = {CM_GPIO_MODE_NUM, CM_GPIO_DIRECTION_OUTPUT, CM_GPIO_PULL_DOWN};
    if (!buzzer || alarm_buzzer_device.initialized || frequency_hz > 20000U ||
        sdk_pin != CM_GPIO_NUM_16)
    {
        return false;
    }
    if (frequency_hz)
    {
        if (cm_iomux_set_pin_func(CM_IOMUX_PIN_74, CM_IOMUX_FUNC_FUNCTION1) != 0 ||
            cm_pwm_open_ns(CM_PWM_DEV_0, 1000000000U / frequency_hz, 0U) != 0 ||
            cm_pwm_close(CM_PWM_DEV_0) != 0)
        {
            return false;
        }
    }
    /* 当前底包中 GPIO14～17 必须使用功能 3；物理 74 脚对应 GPIO16。 */
    else if (cm_iomux_set_pin_func(CM_IOMUX_PIN_74, CM_IOMUX_FUNC_FUNCTION3) != 0 ||
             cm_gpio_init((cm_gpio_num_e)sdk_pin, &output) != 0 ||
             cm_gpio_set_level((cm_gpio_num_e)sdk_pin, CM_GPIO_LEVEL_LOW) != 0)
    {
        return false;
    }
    alarm_buzzer_device.sdk_pin = sdk_pin;
    alarm_buzzer_device.frequency_hz = frequency_hz;
    alarm_buzzer_device.on = false;
    alarm_buzzer_device.initialized = true;
    buzzer->user = &alarm_buzzer_device;
    buzzer->set = ml307y_alarm_buzzer_set;
    buzzer->ready = true;
    return true;
}
