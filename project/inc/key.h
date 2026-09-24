#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>

/*-------------------------------------------typedef---------------------------------------------*/
/* 仅报告消抖后稳定电平的变化；持续按住不会重复产生按下事件。 */
typedef enum
{
    ALARM_KEY_NONE = 0,
    ALARM_KEY_PRESS,
    ALARM_KEY_RELEASE
} alarm_key_event_t;

typedef struct
{
    bool candidate; /* 最近一次采样的候选电平。 */
    bool stable; /* 已通过消抖门限的逻辑电平。 */
    uint32_t since; /* 候选电平开始保持的单调毫秒时刻。 */
} alarm_key_state_t;

/*-------------------------------------------function---------------------------------------------*/
/* 周期采样逻辑按下状态；now 回绕时仍以无符号差值判断持续时间。 */
alarm_key_event_t alarm_key_sample(alarm_key_state_t *context, bool pressed, uint32_t now, uint32_t debounce_ms);
