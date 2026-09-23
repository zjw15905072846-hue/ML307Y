/*------------------------------------------includes--------------------------------------------*/
#include "indicator.h"
#include <string.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : al_indicator_start
* Description    : 替换提示但不修改上报队列
* Input          : ctx - 提示上下文；pattern - 产品时序；event_id - 事件ID；now - 起始毫秒
* Output         : ctx - 替换后的提示状态
* Return         : 无
* Attention      : event_id=0代表保存失败；仍提示但不接受成功确认
*******************************************************************************/
void al_indicator_start(al_indicator_t *ctx, const al_pattern_t *pattern, uint32_t event_id,
                        uint32_t now)
{
    if (!ctx || !pattern)
    {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->pattern = *pattern;
    ctx->event_id = event_id;
    ctx->started = now;
    ctx->active = pattern->half_period_ms > 0 && pattern->initial_ms < 0x40000000U &&
                  pattern->blink_ms < 0x40000000U && pattern->wait_ms < 0x40000000U;
}

/*******************************************************************************
* Function Name  : al_indicator_ack
* Description    : 仅匹配当前提示的有效事件ID
* Input          : ctx - 当前提示；event_id - 已持久完成的报警ID
* Output         : 匹配时置acked，不立即中断前四秒
* Return         : 无
* Attention      : 旧事件ID不能影响新提示
*******************************************************************************/
void al_indicator_ack(al_indicator_t *ctx, uint32_t event_id)
{
    if (ctx && event_id != 0 && ctx->event_id == event_id)
    {
        ctx->acked = true;
    }
}

/*******************************************************************************
* Function Name  : al_indicator_tick
* Description    : 执行完整初始常亮及闪鸣，然后按回执或截止时间收尾
* Input          : ctx - 当前提示；now - 单调毫秒
* Output         : ctx.active - 提示是否结束
* Return         : 声光电平及提示活动状态
* Attention      : 仅计算期望电平；由板级统一驱动
*******************************************************************************/
al_output_t al_indicator_tick(al_indicator_t *ctx, uint32_t now)
{
    al_output_t out = {false, false, false};
    uint32_t elapsed;
    uint32_t front;
    if (!ctx || !ctx->active)
    {
        return out;
    }
    elapsed = (uint32_t)(now - ctx->started);
    front = ctx->pattern.initial_ms + ctx->pattern.blink_ms;
    if (elapsed >= front && (ctx->acked || elapsed - front >= ctx->pattern.wait_ms))
    {
        ctx->active = false;
        return out;
    }
    out.active = true;
    if (elapsed < ctx->pattern.initial_ms || elapsed >= front)
    {
        out.led = true;
    }
    else
    {
        out.led = (((elapsed - ctx->pattern.initial_ms) / ctx->pattern.half_period_ms) % 2U) == 0;
        out.buzzer = out.led;
    }
    return out;
}

/*******************************************************************************
* Function Name  : al_indicator_stop
* Description    : 为复用产品提供显式停止提示序列的接口
* Input          : ctx - 声光序列上下文
* Output         : 标记提示结束，下一次tick输出关闭
* Return         : 无
* Attention      : 本产品成功回执只用ack，不能以stop打断前4秒
*******************************************************************************/
void al_indicator_stop(al_indicator_t *ctx)
{
    if (ctx)
    {
        ctx->active = false;
    }
}
