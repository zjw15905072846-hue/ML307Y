/*------------------------------------------includes--------------------------------------------*/
#include "alarm_button/alarm_button.h"
#include "alarm_button/product_config.h"
#include <string.h>
/*-------------------------------------------define---------------------------------------------*/
#define ALARM_BUTTON_EMERGENCY_EVENT 0x0CU /* 紧急求助事件的平台协议编码。 */

/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : alarm_button_default_config
* Description    : 返回一键报警产品的按键与声光时序
* Input          : 无
* Output         : 无
* Return         : 本产品独立默认配置
* Attention      : 公共层不保存全局产品参数
*******************************************************************************/
alarm_button_config_t alarm_button_default_config(void)
{
    alarm_button_config_t config = {ALARM_BUTTON_DEBOUNCE_MS,
                     {ALARM_BUTTON_INITIAL_MS, ALARM_BUTTON_BLINK_MS, ALARM_BUTTON_WAIT_MS, ALARM_BUTTON_HALF_PERIOD_MS}};
    return config;
}

/*******************************************************************************
* Function Name  : alarm_button_init
* Description    : 绑定前台声光与后台提交端口
* Input          : button_state - 产品上下文；config - 时序；callbacks - 声光和事件提交端口
* Output         : 初始化产品状态
* Return         : ALARM_OK或参数/时序配置错误
* Attention      : 初始化本身不发送报警，不绑定任何固定GPIO
*******************************************************************************/
int alarm_button_init(alarm_button_state_t *button_state,
                      const alarm_button_config_t *config,
                      const alarm_button_callbacks_t *callbacks)
{
    uint64_t total;
    if (!button_state || !config || !callbacks || !callbacks->set_led || !callbacks->set_buzzer ||
        !callbacks->submit_event)
    {
        return ALARM_ERROR_ARGUMENT;
    }
    total =
        (uint64_t)config->pattern.initial_ms + config->pattern.blink_ms + config->pattern.wait_ms;
    /* 毫秒差值后续用有符号比较，整段时序须小于半个 32 位计时周期。 */
    if (!config->debounce_ms || !config->pattern.half_period_ms || total >= 0x80000000ULL)
    {
        return ALARM_ERROR_CONFIG;
    }
    memset(button_state, 0, sizeof(*button_state));
    button_state->config = *config;
    button_state->callbacks = *callbacks;
    return ALARM_OK;
}

/*******************************************************************************
* Function Name  : alarm_button_record_fault
* Description    : 保留并报告本地故障，禁止伪装上传成功
* Input          : button_state - 产品上下文；error - 明确错误码
* Output         : 保存last_error并调用故障回调
* Return         : 无
* Attention      : 本地错误禁止进入正常休眠
*******************************************************************************/
static void alarm_button_record_fault(alarm_button_state_t *button_state, int error)
{
    button_state->last_error = error;
    if (button_state->callbacks.fault)
    {
        button_state->callbacks.fault(button_state->callbacks.user, error);
    }
}

/*******************************************************************************
* Function Name  : alarm_button_set_outputs
* Description    : 按 LED、蜂鸣器的顺序应用本轮声光状态
* Input          : button_state - 前台状态；led/buzzer - 目标状态
* Output         : 两个器件的实际输出
* Return         : true - 均成功；false - 某个器件失败
* Attention      : 前一个器件失败时不继续写下一个器件
*******************************************************************************/
static bool alarm_button_set_outputs(alarm_button_state_t *button_state, bool led, bool buzzer)
{
    return button_state->callbacks.set_led(button_state->callbacks.user, led) &&
           button_state->callbacks.set_buzzer(button_state->callbacks.user, buzzer);
}

/*******************************************************************************
* Function Name  : alarm_button_update
* Description    : 消抖后提交独立事件，同时推进声光提示
* Input          : button_state - 前台上下文；pressed - 原始逻辑电平；now - 单调毫秒
* Output         : 有效按下时投递保存请求，持续更新期望声光
* Return         : 无；异常写入last_error并回调
* Attention      : 前台不访问Flash或网络；请求入队不等于已经落盘
*******************************************************************************/
void alarm_button_update(alarm_button_state_t *button_state, bool pressed, uint32_t now)
{
    alarm_indicator_output_t output;
    alarm_event_t event = {0};
    int result;
    if (!button_state)
    {
        return;
    }
    if (alarm_key_sample(&button_state->key, pressed, now, button_state->config.debounce_ms) == ALARM_KEY_PRESS)
    {
        /* 在Flash操作前立即点亮，持久化失败仍按未确认提示处理。 */
        if (!alarm_button_set_outputs(button_state, true, false))
        {
            alarm_button_record_fault(button_state, ALARM_ERROR_NOT_READY);
        }
        alarm_indicator_start(&button_state->indicator, &button_state->config.pattern, 0, now);
        button_state->background_idle = false;
        event.uptime_ms = now;
        event.event_type = ALARM_BUTTON_EMERGENCY_EVENT;
        if (button_state->latest_request_id == UINT32_MAX)
        {
            alarm_button_record_fault(button_state, ALARM_ERROR_EXHAUSTED);
        }
        else
        {
            ++button_state->latest_request_id;
            result = button_state->callbacks.submit_event(button_state->callbacks.user, button_state->latest_request_id, &event);
            if (result == ALARM_OK)
            {
                ++button_state->pending_save_count;
            }
            else
            {
                alarm_button_record_fault(button_state, result);
            }
        }
    }
    output = alarm_indicator_tick(&button_state->indicator, now);
    if (!alarm_button_set_outputs(button_state, output.led, output.buzzer))
    {
        alarm_button_record_fault(button_state, ALARM_ERROR_NOT_READY);
    }
}

/*******************************************************************************
* Function Name  : alarm_button_can_sleep
* Description    : 前台静止且后台确认清空后才允许休眠
* Input          : button_state - 产品上下文
* Output         : 无
* Return         : true - 本地业务已静止；false - 仍有工作或故障
* Attention      : 平台层还须确认网络已退出、后台查询已完成且唤醒已验证
*******************************************************************************/
bool alarm_button_can_sleep(const alarm_button_state_t *button_state)
{
    if (!button_state || button_state->last_error != ALARM_OK ||
        button_state->key.stable || button_state->key.candidate ||
        button_state->indicator.active || button_state->pending_save_count)
    {
        return false;
    }
    /* 后台在网络退出且持久队列为空时才会报告静止。 */
    return button_state->background_idle;
}

/*******************************************************************************
* Function Name  : alarm_button_on_save_result
* Description    : 处理按请求顺序返回的后台持久化结果
* Input          : button_state - 前台上下文；request - 请求编号；event_id - 持久事件；result - 保存状态
* Output         : 更新待保存数量及最近提示对应的事件ID
* Return         : 无
* Attention      : 旧请求结果不能重新绑定新提示；重复完成不会重复减计数
*******************************************************************************/
void alarm_button_on_save_result(alarm_button_state_t *button_state, uint32_t request, uint32_t event_id, int result)
{
    if (!button_state || !request ||
        request <= button_state->last_completed_request_id ||
        request > button_state->latest_request_id)
    {
        return;
    }
    button_state->last_completed_request_id = request;
    if (button_state->pending_save_count)
    {
        --button_state->pending_save_count;
    }
    if (result != ALARM_OK || event_id == 0)
    {
        alarm_button_record_fault(button_state, result == ALARM_OK ? ALARM_ERROR_STORAGE : result);
        return;
    }
    if (request == button_state->latest_request_id)
    {
        /* 旧请求完成时不能重绑最新一次按键提示。 */
        button_state->indicator.event_id = event_id;
        button_state->last_error = ALARM_OK;
    }
}

/*******************************************************************************
* Function Name  : alarm_button_on_alarm_confirmed
* Description    : 处理后台已持久完成的业务确认
* Input          : button_state - 前台上下文；event_id - 完成的事件ID
* Output         : 匹配时标注提示确认
* Return         : 无
* Attention      : 不以MQTT发送成功调用；不打断前四秒声光
*******************************************************************************/
void alarm_button_on_alarm_confirmed(alarm_button_state_t *button_state, uint32_t event_id)
{
    if (button_state)
    {
        alarm_indicator_on_confirmation(&button_state->indicator, event_id);
    }
}
