#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>

/*-------------------------------------------typedef---------------------------------------------*/
/* 仅报告消抖后稳定电平的变化；持续按住不会重复产生按下事件。 */
typedef enum
{
    AL_KEY_NONE = 0,
    AL_KEY_PRESS,
    AL_KEY_RELEASE
} al_key_event_t;

typedef struct
{
    bool candidate; /* 最近一次采样的候选电平。 */
    bool stable; /* 已通过消抖门限的逻辑电平。 */
    uint32_t since; /* 候选电平开始保持的单调毫秒时刻。 */
} al_key_t;

/*-------------------------------------------function---------------------------------------------*/
/* 周期采样逻辑按下状态；now 回绕时仍以无符号差值判断持续时间。 */
al_key_event_t al_key_sample(al_key_t *ctx, bool pressed, uint32_t now, uint32_t debounce_ms);
