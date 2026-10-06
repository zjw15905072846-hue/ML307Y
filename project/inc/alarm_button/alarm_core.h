#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "storage_interface.h"
/*-------------------------------------------define---------------------------------------------*/
#define ALARM_CAPACITY 96U /* 单份报警镜像可保存的事件数。 */
#define ALARM_STORAGE_EMPTY 1 /* 已验证介质为空白，才允许创建初始镜像。 */
/* valid 位分别表示真实电压、电量、信号及发生时 UTC 可用。 */
#define ALARM_TELEMETRY_VOLTAGE 1U
#define ALARM_TELEMETRY_PERCENT 2U
#define ALARM_TELEMETRY_CSQ 4U
#define ALARM_TIME_UTC 8U

/*-------------------------------------------typedef---------------------------------------------*/
/* 报警队列与重试层使用的明确结果，不合并介质损坏与读写故障。 */
typedef enum
{
    ALARM_OK = 0,
    ALARM_ERROR_ARGUMENT = -1,
    ALARM_ERROR_STORAGE = -2,
    ALARM_ERROR_FULL = -3,
    ALARM_ERROR_FOREIGN = -4,
    ALARM_ERROR_CORRUPT = -5,
    ALARM_ERROR_STALE = -6,
    ALARM_ERROR_REJECTED = -7,
    ALARM_ERROR_EXHAUSTED = -8,
    ALARM_ERROR_NOT_READY = -9,
    ALARM_ERROR_CONFIG = -10
} alarm_result_t;

/* 单条报警的持久数据格式；字段布局属于版本化存储镜像。 */
typedef struct
{
    uint32_t id; /* 入队成功后分配的事件 ID。 */
    uint32_t uptime_ms; /* 按下时的单调毫秒快照。 */
    uint32_t utc_seconds; /* 仅 ALARM_TIME_UTC 置位时可用于历史补报。 */
    uint16_t battery_mv; /* ALARM_TELEMETRY_VOLTAGE 置位时有效。 */
    uint8_t battery_percent; /* ALARM_TELEMETRY_PERCENT 置位时有效。 */
    uint8_t csq; /* ALARM_TELEMETRY_CSQ 置位时有效。 */
    uint8_t valid; /* 遥测和 UTC 有效位。 */
    uint8_t event_type; /* 上报协议中的事件类型。 */
    uint8_t reserved[2]; /* 保持当前持久格式布局。 */
} alarm_event_t;

/* 队列镜像含产品身份和格式版本；不得直接清空陌生或损坏镜像。 */
typedef struct
{
    uint32_t magic; /* 镜像格式标识。 */
    uint32_t product_id; /* 只接受当前报警产品的镜像。 */
    uint32_t next_id; /* 下一条成功入队事件使用的 ID。 */
    uint32_t next_sequence; /* 下次发送使用的持久序号计数。 */
    uint32_t crc; /* 镜像完整性校验值。 */
    uint16_t version; /* 持久格式版本。 */
    uint16_t bytes; /* 镜像实际字节数。 */
    uint16_t count; /* 当前待业务确认事件数。 */
    uint16_t reserved;
    alarm_event_t events[ALARM_CAPACITY];
} alarm_storage_image_t;

typedef storage_interface_t alarm_storage_port_t;

typedef struct
{
    alarm_storage_image_t image; /* 最近完整提交的队列状态。 */
    alarm_storage_image_t staging; /* 写入前构造的新镜像，失败不覆盖 image。 */
    alarm_storage_port_t port;
    bool ready; /* 完成介质身份与完整性校验后置位。 */
} alarm_event_store_t;

/* 只负责把事件交给传输队列；true 不表示收到云端业务确认。 */
typedef bool (*alarm_send_callback_t)(void *user, const alarm_event_t *event, uint16_t sequence);
/* 可选发送条件检查；false 只保留此事件，不阻挡后面已满足条件的事件。 */
typedef bool (*alarm_event_ready_callback_t)(void *user, const alarm_event_t *event);

/* 重试上下文只跟踪一个可发送事件；事件删除仍由持久队列完成。 */
typedef struct
{
    alarm_event_store_t *store;
    alarm_send_callback_t send;
    alarm_event_ready_callback_t event_ready; /* NULL 时按原队列顺序发送；不得修改队列。 */
    void *user;
    uint32_t event_id; /* 当前等待确认的事件；较早的待补报记录可以继续保留。 */
    uint32_t sent_at; /* 最近成功入发送队列的时刻。 */
    uint32_t retry_at; /* 退避到期后才允许重试。 */
    uint32_t confirmation_ms; /* 等待业务回执的超时门限。 */
    uint32_t minimum_ms; /* 重试退避下限。 */
    uint32_t maximum_ms; /* 重试退避上限。 */
    uint32_t backoff; /* 当前退避时长。 */
    uint16_t sequence; /* 最近一次真实发送尝试的协议序号。 */
    bool inflight; /* 已发送但尚未收到匹配业务回执。 */
    bool retry_wait; /* 等待退避期结束。 */
    int last_error; /* 最近一次发送或业务确认故障。 */
    /* 当前事件本次运行期间的全部已入发送队列序号；跨重启用持久计数防串单。 */
    uint8_t attempts[8192];
} alarm_event_reporter_t;

/*-------------------------------------------function---------------------------------------------*/
/* 仅确认空白的介质才初始化；不覆盖陌生产品、损坏或旧版镜像。 */
int alarm_store_open(alarm_event_store_t *store, uint32_t product_id, const alarm_storage_port_t *port);
int alarm_store_enqueue(alarm_event_store_t *store, const alarm_event_t *event, uint32_t *id);
int alarm_store_remove(alarm_event_store_t *store, uint32_t id);
int alarm_store_sequence(alarm_event_store_t *store, uint16_t *sequence);
/* 返回待确认记录数；存储未就绪返回负值，不能当作空队列。 */
int alarm_store_pending(const alarm_event_store_t *store);
void alarm_reporter_init(alarm_event_reporter_t *context, alarm_event_store_t *store, alarm_send_callback_t send, void *user,
                      uint32_t confirmation_ms, uint32_t minimum_retry_ms, uint32_t maximum_retry_ms);
void alarm_reporter_poll(alarm_event_reporter_t *context, bool online, uint32_t now);
/* 接受当前事件各次真实尝试的成功回执；落盘删除后才输出completed_id。 */
int alarm_reporter_on_platform_confirmation(alarm_event_reporter_t *context, uint16_t sequence, uint8_t response, uint32_t now,
                    uint32_t *completed_id);
void alarm_reporter_send_failed(alarm_event_reporter_t *context, uint16_t sequence, uint32_t now);
