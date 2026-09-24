/*------------------------------------------includes--------------------------------------------*/
#include "key.h"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : alarm_key_sample
* Description    : 非阻塞消抖，只在稳定电平改变时返回边沿
* Input          : context - 消抖上下文；pressed - 逻辑按下；now/debounce_ms - 毫秒时刻及门限
* Output         : context - 候选电平、稳定电平与起始时间
* Return         : ALARM_KEY_NONE/PRESS/RELEASE
* Attention      : 周期调用；按下与松开使用相同消抖门限
*******************************************************************************/
alarm_key_event_t alarm_key_sample(alarm_key_state_t *context, bool pressed, uint32_t now, uint32_t debounce_ms)
{
    if (!context)
    {
        return ALARM_KEY_NONE;
    }
    if (pressed != context->candidate)
    {
        /* 只重置候选稳定计时，不立即把一次采样当作有效边沿。 */
        context->candidate = pressed;
        context->since = now;
    }
    if (context->candidate != context->stable && (uint32_t)(now - context->since) >= debounce_ms)
    {
        /* 无符号差值允许单调毫秒计数正常回绕。 */
        context->stable = context->candidate;
        return context->stable ? ALARM_KEY_PRESS : ALARM_KEY_RELEASE;
    }
    return ALARM_KEY_NONE;
}
