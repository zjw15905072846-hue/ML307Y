#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "storage_if.h"
#include "key.h"
#include "indicator.h"
/*-------------------------------------------define---------------------------------------------*/
#define AL_CAPACITY 96U /* 单份报警镜像可保存的事件数。 */
#define AL_STORAGE_EMPTY 1 /* 已验证介质为空白，才允许创建初始镜像。 */
/* valid 位分别表示真实电压、电量、信号及发生时 UTC 可用。 */
#define AL_TELEMETRY_VOLTAGE 1U
#define AL_TELEMETRY_PERCENT 2U
#define AL_TELEMETRY_CSQ 4U
#define AL_TIME_UTC 8U

/*-------------------------------------------typedef---------------------------------------------*/
/* 报警队列与重试层使用的明确结果，不合并介质损坏与读写故障。 */
typedef enum
{
    AL_OK = 0,
    AL_ERR_ARGUMENT = -1,
    AL_ERR_STORAGE = -2,
    AL_ERR_FULL = -3,
    AL_ERR_FOREIGN = -4,
    AL_ERR_CORRUPT = -5,
    AL_ERR_STALE = -6,
    AL_ERR_REJECTED = -7,
    AL_ERR_EXHAUSTED = -8,
    AL_ERR_NOT_READY = -9,
    AL_ERR_CONFIG = -10
} al_result_t;

/* 单条报警的持久数据格式；字段布局属于版本化存储镜像。 */
typedef struct
{
    uint32_t id; /* 入队成功后分配的事件 ID。 */
    uint32_t uptime_ms; /* 按下时的单调毫秒快照。 */
    uint32_t utc_seconds; /* 仅 AL_TIME_UTC 置位时可用于历史补报。 */
    uint16_t battery_mv; /* AL_TELEMETRY_VOLTAGE 置位时有效。 */
    uint8_t battery_percent; /* AL_TELEMETRY_PERCENT 置位时有效。 */
    uint8_t csq; /* AL_TELEMETRY_CSQ 置位时有效。 */
    uint8_t valid; /* 遥测和 UTC 有效位。 */
    uint8_t event_type; /* 上报协议中的事件类型。 */
    uint8_t reserved[2]; /* 保持当前持久格式布局。 */
} al_event_t;

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
    al_event_t events[AL_CAPACITY];
} al_image_t;

typedef storage_if_t al_storage_port_t;

typedef struct
{
    al_image_t image; /* 最近完整提交的队列状态。 */
    al_image_t staging; /* 写入前构造的新镜像，失败不覆盖 image。 */
    al_storage_port_t port;
    bool ready; /* 完成介质身份与完整性校验后置位。 */
} al_store_t;

/* 只负责把事件交给传输队列；true 不表示收到云端业务确认。 */
typedef bool (*al_send_fn)(void *user, const al_event_t *event, uint16_t sequence);

/* 重试上下文只跟踪当前队首；事件删除仍由持久队列完成。 */
typedef struct
{
    al_store_t *store;
    al_send_fn send;
    void *user;
    uint32_t event_id; /* 当前等待确认的队首事件。 */
    uint32_t sent_at; /* 最近成功入发送队列的时刻。 */
    uint32_t retry_at; /* 退避到期后才允许重试。 */
    uint32_t ack_ms; /* 等待业务回执的超时门限。 */
    uint32_t min_ms; /* 重试退避下限。 */
    uint32_t max_ms; /* 重试退避上限。 */
    uint32_t backoff; /* 当前退避时长。 */
    uint16_t sequence; /* 最近一次真实发送尝试的协议序号。 */
    bool inflight; /* 已发送但尚未收到匹配业务回执。 */
    bool retry_wait; /* 等待退避期结束。 */
    int last_error; /* 最近一次发送或业务确认故障。 */
    /* 当前队首本次运行期间的全部已入发送队列序号；跨重启用持久计数防串单。 */
    uint8_t attempts[8192];
} al_reporter_t;

/*-------------------------------------------function---------------------------------------------*/
/* 对稳定输入发出一次边沿事件；使用无符号差值处理毫秒回绕。 */
al_key_event_t al_key_sample(al_key_t *ctx, bool pressed, uint32_t now, uint32_t debounce_ms);
/* 启动或替换当前提示；event_id=0表示未成功持久化，不接受成功回执。 */
void al_indicator_start(al_indicator_t *ctx, const al_pattern_t *pattern, uint32_t event_id,
                        uint32_t now);
void al_indicator_ack(al_indicator_t *ctx, uint32_t event_id);
/* 显式停止共用序列；本产品不以提前成功调用此接口打断前4秒。 */
void al_indicator_stop(al_indicator_t *ctx);
al_output_t al_indicator_tick(al_indicator_t *ctx, uint32_t now);
/* 仅确认空白的介质才初始化；不覆盖陌生产品、损坏或旧版镜像。 */
int al_store_open(al_store_t *store, uint32_t product_id, const al_storage_port_t *port);
int al_store_enqueue(al_store_t *store, const al_event_t *event, uint32_t *id);
int al_store_remove(al_store_t *store, uint32_t id);
int al_store_sequence(al_store_t *store, uint16_t *sequence);
/* 返回待确认记录数；存储未就绪返回负值，不能当作空队列。 */
int al_store_pending(const al_store_t *store);
void al_reporter_init(al_reporter_t *ctx, al_store_t *store, al_send_fn send, void *user,
                      uint32_t ack_ms, uint32_t min_retry_ms, uint32_t max_retry_ms);
void al_reporter_poll(al_reporter_t *ctx, bool online, uint32_t now);
/* 接受当前事件各次真实尝试的成功回执；落盘删除后才输出completed_id。 */
int al_reporter_ack(al_reporter_t *ctx, uint16_t sequence, uint8_t response, uint32_t now,
                    uint32_t *completed_id);
void al_reporter_send_failed(al_reporter_t *ctx, uint16_t sequence, uint32_t now);
