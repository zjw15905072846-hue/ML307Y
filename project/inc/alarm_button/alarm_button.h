#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "alarm_button/alarm_core.h"
#include "key.h"
#include "indicator.h"
/*-------------------------------------------define---------------------------------------------*/
#define ALARM_BUTTON_PRODUCT_ID 0x41420101U /* 报警产品的持久化身份，不随目录调整改变。 */

/*-------------------------------------------typedef---------------------------------------------*/
/* 报警产品私有时序，不作为其他产品的默认策略。 */
typedef struct
{
    uint32_t debounce_ms; /* 按下和释放共用的消抖门限。 */
    alarm_indicator_pattern_t pattern; /* 固定提示与等待回执时序。 */
} alarm_button_config_t;

/* 前台分别控制灯和蜂鸣器，并通过回调提交事件。 */
typedef struct
{
    void *user; /* 回调上下文，在产品运行期间保持有效。 */
    bool (*set_led)(void *user, bool on);
    bool (*set_buzzer)(void *user, bool on);
    /* 将本地故障上送后台或诊断端，不把错误伪装成成功。 */
    void (*fault)(void *user, int error);
    /* 投递一次独立保存请求；返回值只表示投递结果，不代表已经落盘。 */
    int (*submit_event)(void *user, uint32_t request, const alarm_event_t *event);
} alarm_button_callbacks_t;

/* 前台状态由产品任务独占；保存和回执结果以请求/事件 ID 关联。 */
typedef struct
{
    alarm_button_config_t config;
    alarm_button_callbacks_t callbacks;
    alarm_key_state_t key;
    alarm_indicator_state_t indicator;
    uint32_t latest_request_id; /* 最近一次有效按下生成的本地请求。 */
    uint32_t last_completed_request_id; /* 最近完成且可与提示关联的请求。 */
    unsigned pending_save_count; /* 已投递但尚未收到保存结果的请求数。 */
    bool background_idle; /* 后台明确报告无待处理工作。 */
    int last_error; /* 最近本地故障，阻止错误进入休眠。 */
} alarm_button_state_t;

/*-------------------------------------------function---------------------------------------------*/
/* 本产品独立配置，不影响其他产品。 */
alarm_button_config_t alarm_button_default_config(void);
int alarm_button_init(alarm_button_state_t *button_state,
                      const alarm_button_config_t *config,
                      const alarm_button_callbacks_t *callbacks);
/* 周期处理按键消抖和声光提示；保存请求交由后台处理。 */
void alarm_button_update(alarm_button_state_t *button_state, bool pressed, uint32_t now);
/* 提示结束、按键释放、后台空闲且无本地故障时才返回 true。 */
bool alarm_button_can_sleep(const alarm_button_state_t *button_state);

/* 后台按 FIFO 返回保存结果；旧请求不能绑定或结束新提示。 */
void alarm_button_on_save_result(alarm_button_state_t *button_state,
                                 uint32_t request, uint32_t event_id, int result);
/* 已完成持久删除的事件才允许通知前台成功。 */
void alarm_button_on_alarm_confirmed(alarm_button_state_t *button_state, uint32_t event_id);
