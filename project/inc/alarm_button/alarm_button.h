#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "alarm_button/alarm_core.h"
/*-------------------------------------------define---------------------------------------------*/
#define AB_PRODUCT_ID 0x41420101U /* 报警产品的持久化身份，不随目录调整改变。 */

/*-------------------------------------------typedef---------------------------------------------*/
/* 报警产品私有时序，不作为其他产品的默认策略。 */
typedef struct
{
    uint32_t debounce_ms; /* 按下和释放共用的消抖门限。 */
    uint32_t heartbeat_ms; /* 业务心跳间隔。 */
    al_pattern_t pattern; /* 固定提示与等待回执时序。 */
} ab_config_t;

/* 前台只通过回调提交事件及控制声光，不直接读写存储或 MQTT。 */
typedef struct
{
    void *user; /* 回调上下文，在产品运行期间保持有效。 */
    /* 板级声光写入结果；失败时上层必须保留故障状态。 */
    bool (*output)(void *user, bool led, bool buzzer);
    /* 将本地故障上送后台或诊断端，不把错误伪装成成功。 */
    void (*fault)(void *user, int error);
    /* 投递一次独立保存请求；返回值只表示投递结果，不代表已经落盘。 */
    int (*submit_event)(void *user, uint32_t request, const al_event_t *event);
} ab_io_t;

/* 前台状态由产品任务独占；保存和回执结果以请求/事件 ID 关联。 */
typedef struct
{
    ab_config_t config;
    ab_io_t io;
    al_store_t *store;
    al_reporter_t *reporter;
    al_key_t key;
    al_indicator_t indicator;
    al_output_t output;
    uint32_t latest_event_id; /* 当前提示绑定的持久事件；0 表示尚未保存。 */
    uint32_t latest_request; /* 最近一次有效按下生成的本地请求。 */
    uint32_t completed_request; /* 最近完成且可与提示关联的请求。 */
    unsigned pending_saves; /* 已投递但尚未收到保存结果的请求数。 */
    bool background_idle; /* 后台明确报告无待处理工作。 */
    int last_error; /* 最近本地故障，阻止错误进入休眠。 */
} ab_app_t;

/*-------------------------------------------function---------------------------------------------*/
/* 本产品独立配置，不影响其他产品。 */
ab_config_t ab_default_config(void);
int ab_init(ab_app_t *app, const ab_config_t *config, const ab_io_t *io, al_store_t *store,
            al_reporter_t *reporter);
/* 周期处理消抖、提示和心跳；telemetry 是当前采样快照。 */
void ab_poll(ab_app_t *app, bool pressed, uint32_t now, const al_event_t *telemetry);
/* 只有匹配的铠湾业务成功回执及持久删除完成才结束事件。 */
int ab_ack(ab_app_t *app, uint16_t sequence, uint8_t response, uint32_t now);
/* 提示结束、按键释放、后台空闲且无本地故障时才返回 true。 */
bool ab_can_sleep(const ab_app_t *app);

/* 后台按 FIFO 返回保存结果；旧请求不能绑定或结束新提示。 */
void ab_saved(ab_app_t *app, uint32_t request, uint32_t event_id, int result);
/* 已完成持久删除的事件才允许通知前台成功。 */
void ab_confirmed(ab_app_t *app, uint32_t event_id);
