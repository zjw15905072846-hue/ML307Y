/*------------------------------------------includes--------------------------------------------*/
#include "indicator.h"
#include <string.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : alarm_indicator_start
* Description    : 替换提示但不修改上报队列
* Input          : context - 提示上下文；pattern - 产品时序；event_id - 事件ID；now - 起始毫秒
* Output         : context - 替换后的提示状态
* Return         : 无
* Attention      : event_id=0代表保存失败；仍提示但不接受成功确认
*******************************************************************************/
void alarm_indicator_start(alarm_indicator_state_t *context, const alarm_indicator_pattern_t *pattern, uint32_t event_id,
                        uint32_t now)
{
    if (!context || !pattern)
    {
        return;
    }
    memset(context, 0, sizeof(*context));
    context->pattern = *pattern;
    context->event_id = event_id;
    context->started = now;
    /* 限制时序字段，避免计算周期和截止时间时出现异常溢出。 */
    context->active = pattern->half_period_ms > 0 && pattern->initial_ms < 0x40000000U &&
                  pattern->blink_ms < 0x40000000U && pattern->wait_ms < 0x40000000U;
}

/*******************************************************************************
* Function Name  : alarm_indicator_on_confirmation
* Description    : 仅匹配当前提示的有效事件ID
* Input          : context - 当前提示；event_id - 已持久完成的报警ID
* Output         : 匹配时置acked，不立即中断前四秒
* Return         : 无
* Attention      : 旧事件ID不能影响新提示
*******************************************************************************/
void alarm_indicator_on_confirmation(alarm_indicator_state_t *context, uint32_t event_id)
{
    if (context && event_id != 0 && context->event_id == event_id)
    {
        context->acked = true;
    }
}

/*******************************************************************************
* Function Name  : alarm_indicator_tick
* Description    : 执行完整初始常亮及闪鸣，然后按回执或截止时间收尾
* Input          : context - 当前提示；now - 单调毫秒
* Output         : context.active - 提示是否结束
* Return         : 声光电平及提示活动状态
* Attention      : 仅计算期望电平；由板级统一驱动
*******************************************************************************/
alarm_indicator_output_t alarm_indicator_tick(alarm_indicator_state_t *context, uint32_t now)
{
    alarm_indicator_output_t output = {false, false, false};
    uint32_t elapsed;
    uint32_t front;
    if (!context || !context->active)
    {
        return output;
    }
    elapsed = (uint32_t)(now - context->started);
    front = context->pattern.initial_ms + context->pattern.blink_ms;
    /* 初始提示阶段必须走完；之后收到匹配回执或等候超时才结束。 */
    if (elapsed >= front && (context->acked || elapsed - front >= context->pattern.wait_ms))
    {
        context->active = false;
        return output;
    }
    output.active = true;
    if (elapsed < context->pattern.initial_ms || elapsed >= front)
    {
        output.led = true;
    }
    else
    {
        /* 半周期奇偶交替驱动 LED 和蜂鸣器，回执不改变这一阶段的节奏。 */
        output.led = (((elapsed - context->pattern.initial_ms) / context->pattern.half_period_ms) % 2U) == 0;
        output.buzzer = output.led;
    }
    return output;
}

/*******************************************************************************
* Function Name  : alarm_indicator_stop
* Description    : 为复用产品提供显式停止提示序列的接口
* Input          : context - 声光序列上下文
* Output         : 标记提示结束，下一次tick输出关闭
* Return         : 无
* Attention      : 本产品成功回执只用confirmation，不能以stop打断前4秒
*******************************************************************************/
void alarm_indicator_stop(alarm_indicator_state_t *context)
{
    if (context)
    {
        context->active = false;
    }
}
