#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>

/*-------------------------------------------typedef---------------------------------------------*/
/* 初始常亮、闪鸣、等待回执三个阶段的时长由产品配置。 */
typedef struct
{
    uint32_t initial_ms; /* 初始亮灯且静音阶段。 */
    uint32_t blink_ms; /* 随后的同步闪灯与鸣叫阶段。 */
    uint32_t wait_ms; /* 固定提示结束后允许等待成功回执的最长时间。 */
    uint32_t half_period_ms; /* 闪鸣半周期；必须非零。 */
} al_pattern_t;

/* tick 只计算目标状态，实际 LED/PWM 写入由板级唯一所有者执行。 */
typedef struct
{
    bool led;
    bool buzzer;
    bool active;
} al_output_t;

typedef struct
{
    al_pattern_t pattern;
    uint32_t event_id; /* 当前提示所属持久事件；0 表示尚无可确认事件。 */
    uint32_t started; /* 当前提示启动的单调毫秒时刻。 */
    bool active; /* 提示序列尚未结束。 */
    bool acked; /* 收到当前事件的确认，固定提示阶段仍须走完。 */
} al_indicator_t;

/*-------------------------------------------function---------------------------------------------*/
/* 新提示替换旧提示，不改变持久事件队列。 */
void al_indicator_start(al_indicator_t *ctx, const al_pattern_t *pattern, uint32_t event_id,
                        uint32_t now);
/* 只有 event_id 匹配当前提示时才记录成功确认。 */
void al_indicator_ack(al_indicator_t *ctx, uint32_t event_id);
/* 显式停止提示；报警产品正常回执路径不以此打断前四秒。 */
void al_indicator_stop(al_indicator_t *ctx);
/* 返回本次期望声光状态，并在阶段完成时推进上下文。 */
al_output_t al_indicator_tick(al_indicator_t *ctx, uint32_t now);
