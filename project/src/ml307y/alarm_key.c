/*------------------------------------------includes--------------------------------------------*/
#include "ml307y/alarm_key.h"
#include "ml307y/diag_uart.h"
#include "key.h"
#include "ml307y/base_gpio.h"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    bool initialized;
    alarm_key_interface_t *interface;
} alarm_key_device_t;

/*-------------------------------------------variables-------------------------------------------*/
/* 当前构建只选择一块报警板；边沿唤醒后由前台读取并消抖。 */
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
    if (!device || !device->initialized || !pressed)
    {
        return false;
    }
    return project_button_input_read(pressed) == 0;
}

/*******************************************************************************
* Function Name  : ml307y_alarm_key_set_wakeup
* Description    : 前台队列创建后装配按键中断唤醒通知
* Input          : user - 按键器件；notify/argument - 零等待通知及上下文
* Output         : wake_configured - 本次配置状态
* Return         : 无
* Attention      : 失败保留轮询与工作锁；配置成功不修改实板验证标志
*******************************************************************************/
static void ml307y_alarm_key_set_wakeup(void *user, void (*notify)(void *), void *argument)
{
    alarm_key_device_t *device = user;
    int result;
    if (!device || !device->initialized)
    {
        return;
    }
    result = project_button_wakeup_configure(notify, argument);
    device->interface->wake_configured = result == 0;
    ml307y_uart_diag_printf("[project][alarm-key-wakeup] configured=%u result=%d hardware_verified=0",
                           (unsigned)device->interface->wake_configured, result);
}

/*******************************************************************************
* Function Name  : ml307y_alarm_key_init
* Description    : 通过当前底包 AGPIO0 输入接口配置物理 26 脚
* Input          : key - 产品按键接口；wake_verified - 本轮必须为 false
* Output         : 成功后绑定轮询读取接口
* Return         : true - 初始化完成；false - 配置、HAL 或首次读取失败
* Attention      : 不把物理脚号传入 CM GPIO；前台保留工作锁及 30ms 消抖
*******************************************************************************/
bool ml307y_alarm_key_init(alarm_key_interface_t *key, bool wake_verified)
{
    bool pressed;
    int error;
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
    if (wake_verified)
    {
        ml307y_uart_diag_printf("[project][alarm-key-wakeup] polling required");
        return false;
    }
    ml307y_uart_diag_printf("[project] alarm key init pin=26 hal=AGPIO0 polling");
    error = project_button_input_init();
    if (error != 0)
    {
        ml307y_uart_diag_printf("[project][alarm-key-hal-init] error=%d", error);
        return false;
    }
    error = project_button_input_read(&pressed);
    if (error != 0)
    {
        ml307y_uart_diag_printf("[project][alarm-key-hal-read] error=%d", error);
        return false;
    }
    alarm_key_device.initialized = true;
    key->user = &alarm_key_device;
    key->read = ml307y_alarm_key_read;
    alarm_key_device.interface = key;
    key->set_wakeup = ml307y_alarm_key_set_wakeup;
    key->wake_configured = false;
    key->wake_verified = false;
    key->ready = true;
    ml307y_uart_diag_printf("[project] alarm key initial pressed=%u", (unsigned)pressed);
    return true;
}
