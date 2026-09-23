/*------------------------------------------includes--------------------------------------------*/
#include "key.h"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : al_key_sample
* Description    : 非阻塞消抖，只在稳定电平改变时返回边沿
* Input          : ctx - 消抖上下文；pressed - 逻辑按下；now/debounce_ms - 毫秒时刻及门限
* Output         : ctx - 候选电平、稳定电平与起始时间
* Return         : AL_KEY_NONE/PRESS/RELEASE
* Attention      : 周期调用；按下与松开使用相同消抖门限
*******************************************************************************/
al_key_event_t al_key_sample(al_key_t *ctx, bool pressed, uint32_t now, uint32_t debounce_ms)
{
    if (!ctx)
    {
        return AL_KEY_NONE;
    }
    if (pressed != ctx->candidate)
    {
        ctx->candidate = pressed;
        ctx->since = now;
    }
    if (ctx->candidate != ctx->stable && (uint32_t)(now - ctx->since) >= debounce_ms)
    {
        ctx->stable = ctx->candidate;
        return ctx->stable ? AL_KEY_PRESS : AL_KEY_RELEASE;
    }
    return AL_KEY_NONE;
}
