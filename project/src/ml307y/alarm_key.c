/*------------------------------------------includes--------------------------------------------*/
#include "ml307y/alarm_key.h"
#include "ml307y/diag_uart.h"
#include "key.h"
#include "cm_gpio.h"
#include "cm_iomux.h"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    int sdk_pin;
    void (*wake_notify)(void *);
    void *wake_argument;
    bool initialized;
} alarm_key_device_t;

/*-------------------------------------------variables-------------------------------------------*/
/* SDK 中断没有 user 参数；当前构建只选择一块报警板。 */
static alarm_key_device_t alarm_key_device;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : alarm_key_sample
* Description    : 非阻塞消抖，只在稳定电平改变时返回边沿
* Input          : context - 消抖状态；pressed - 原始逻辑按下；now/debounce_ms - 毫秒时刻及门限
* Output         : context - 候选电平、稳定电平与起始时间
* Return         : ALARM_KEY_NONE/PRESS/RELEASE
* Attention      : 前台周期调用；中断不负责消抖
*******************************************************************************/
alarm_key_event_t alarm_key_sample(alarm_key_state_t *context, bool pressed, uint32_t now, uint32_t debounce_ms)
{
    if (!context)
    {
        return ALARM_KEY_NONE;
    }
    if (pressed != context->candidate)
    {
        context->candidate = pressed;
        context->since = now;
    }
    if (context->candidate != context->stable && (uint32_t)(now - context->since) >= debounce_ms)
    {
        context->stable = context->candidate;
        return context->stable ? ALARM_KEY_PRESS : ALARM_KEY_RELEASE;
    }
    return ALARM_KEY_NONE;
}

/*******************************************************************************
* Function Name  : ml307y_alarm_key_interrupt
* Description    : 把按键边沿通知前台任务
* Input          : 无
* Output         : 零等待唤醒通知
* Return         : 无
* Attention      : 中断内不读写文件、联网或消抖
*******************************************************************************/
static void ml307y_alarm_key_interrupt(void)
{
    if (alarm_key_device.wake_notify)
    {
        alarm_key_device.wake_notify(alarm_key_device.wake_argument);
    }
}

/*******************************************************************************
* Function Name  : ml307y_alarm_key_read
* Description    : 读取按下接地的并联逻辑按键
* Input          : user - 按键器件；pressed - 输出地址
* Output         : pressed - 原始逻辑按下状态
* Return         : true - 有效读取；false - 读取失败
* Attention      : 失败不能解释为松开
*******************************************************************************/
static bool ml307y_alarm_key_read(void *user, bool *pressed)
{
    alarm_key_device_t *device = user;
    cm_gpio_level_e level;
    if (!device || !device->initialized || !pressed ||
        cm_gpio_get_level((cm_gpio_num_e)device->sdk_pin, &level) != 0)
    {
        return false;
    }
    *pressed = level == CM_GPIO_LEVEL_LOW;
    return true;
}

/*******************************************************************************
* Function Name  : ml307y_alarm_key_set_wakeup
* Description    : 保存按键中断的前台唤醒通知
* Input          : user - 按键器件；notify/argument - 通知函数及上下文
* Output         : 按键器件的唤醒回调
* Return         : 无
* Attention      : 通知函数只投递消息
*******************************************************************************/
static void ml307y_alarm_key_set_wakeup(void *user, void (*notify)(void *), void *argument)
{
    alarm_key_device_t *device = user;
    if (device && device->initialized)
    {
        device->wake_notify = notify;
        device->wake_argument = argument;
    }
}

/*******************************************************************************
* Function Name  : ml307y_alarm_key_init
* Description    : 直接配置 26 脚输入，按需注册已核验的唤醒中断
* Input          : key - 产品按键接口；wake_verified - 中断唤醒已核验标志
* Output         : 成功后绑定按键读取与唤醒接口
* Return         : true - 初始化完成；false - SDK 操作失败
* Attention      : 按本板接线直接试用 GPIO26；唤醒未核验时仅由前台轮询
*******************************************************************************/
bool ml307y_alarm_key_init(alarm_key_interface_t *key, bool wake_verified)
{
    cm_gpio_cfg_t input = {CM_GPIO_MODE_NUM, CM_GPIO_DIRECTION_INPUT, CM_GPIO_PULL_UP};
    cm_gpio_level_e level;
    int32_t error;
    if (!key)
    {
        ml307y_uart_diag_printf("[project][alarm-key-interface] error=-1");
        return false;
    }
    if (alarm_key_device.initialized)
    {
        ml307y_uart_diag_printf("[project][alarm-key-already-initialized] error=-1");
        return false;
    }
    error = cm_iomux_set_pin_func(CM_IOMUX_PIN_26, CM_IOMUX_FUNC_FUNCTION2);
    if (error != 0)
    {
        ml307y_uart_diag_printf("[project][alarm-key-iomux] error=%d", error);
        return false;
    }
    error = cm_gpio_init(CM_GPIO_NUM_26, &input);
    if (error != 0)
    {
        ml307y_uart_diag_printf("[project][alarm-key-gpio-init] error=%d", error);
        return false;
    }
    error = cm_gpio_get_level(CM_GPIO_NUM_26, &level);
    if (error != 0)
    {
        ml307y_uart_diag_printf("[project][alarm-key-gpio-read] error=%d", error);
        return false;
    }
    if (wake_verified)
    {
        error = cm_gpio_interrupt_register(CM_GPIO_NUM_26, ml307y_alarm_key_interrupt);
        if (error != 0)
        {
            ml307y_uart_diag_printf("[project][alarm-key-irq-register] error=%d", error);
            return false;
        }
        error = cm_gpio_interrupt_enable(CM_GPIO_NUM_26, CM_GPIO_IT_EDGE_BOTH);
        if (error != 0)
        {
            ml307y_uart_diag_printf("[project][alarm-key-irq-enable] error=%d", error);
            return false;
        }
    }
    alarm_key_device.sdk_pin = CM_GPIO_NUM_26;
    alarm_key_device.initialized = true;
    key->user = &alarm_key_device;
    key->read = ml307y_alarm_key_read;
    key->set_wakeup = wake_verified ? ml307y_alarm_key_set_wakeup : NULL;
    key->wake_verified = wake_verified;
    key->ready = true;
    return true;
}
