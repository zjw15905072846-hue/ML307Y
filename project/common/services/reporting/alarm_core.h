#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "storage_if.h"
#include "key.h"
#include "indicator.h"
/*-------------------------------------------define---------------------------------------------*/
#define AL_CAPACITY 96U
#define AL_STORAGE_EMPTY 1
#define AL_TELEMETRY_VOLTAGE 1U
#define AL_TELEMETRY_PERCENT 2U
#define AL_TELEMETRY_CSQ 4U
#define AL_TIME_UTC 8U

/*-------------------------------------------typedef---------------------------------------------*/
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

typedef struct
{
    uint32_t id;
    uint32_t uptime_ms;
    uint32_t utc_seconds;
    uint16_t battery_mv;
    uint8_t battery_percent;
    uint8_t csq;
    uint8_t valid;
    uint8_t event_type;
    uint8_t reserved[2];
} al_event_t;

typedef struct
{
    uint32_t magic;
    uint32_t product_id;
    uint32_t next_id;
    uint32_t next_sequence;
    uint32_t crc;
    uint16_t version;
    uint16_t bytes;
    uint16_t count;
    uint16_t reserved;
    al_event_t events[AL_CAPACITY];
} al_image_t;

typedef storage_if_t al_storage_port_t;

typedef struct
{
    al_image_t image;
    al_image_t staging;
    al_storage_port_t port;
    bool ready;
} al_store_t;

typedef bool (*al_send_fn)(void *user, const al_event_t *event, uint16_t sequence);

typedef struct
{
    al_store_t *store;
    al_send_fn send;
    void *user;
    uint32_t event_id;
    uint32_t sent_at;
    uint32_t retry_at;
    uint32_t ack_ms;
    uint32_t min_ms;
    uint32_t max_ms;
    uint32_t backoff;
    uint16_t sequence;
    bool inflight;
    bool retry_wait;
    int last_error;
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
