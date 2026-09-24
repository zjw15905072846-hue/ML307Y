#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "mqtt/kaiwan_cloud.h"
/*-------------------------------------------define---------------------------------------------*/
#define MQTT_RECEIVE_MORE 0 /* 当前消息尚有后续分段。 */
#define MQTT_RECEIVE_COMPLETE 1 /* 已接收完整负载，允许上送业务解析。 */
#define MQTT_RECEIVE_INVALID (-1) /* 分段身份、长度或主题无效，整条消息丢弃。 */
#define MQTT_RECEIVE_STALE (-2) /* 回调来自旧连接代数。 */

/*-------------------------------------------typedef---------------------------------------------*/
/* 同一时刻只缓存一条 MQTT 下行消息，内容始终受固定缓冲容量限制。 */
typedef struct
{
    uint32_t generation; /* 重连后递增的连接代数，用于排除旧回调。 */
    uint16_t message_id; /* 当前分段消息的 MQTT 标识。 */
    size_t total; /* 首段声明的总负载长度。 */
    size_t used; /* 已复制到 payload 的字节数。 */
    bool active; /* 正在接收尚未完整的消息。 */
    char topic[KAIWAN_CLOUD_TOPIC_SIZE];
    uint8_t payload[KAIWAN_CLOUD_MAXIMUM_PAYLOAD_SIZE];
} mqtt_receive_state_t;

/*-------------------------------------------function---------------------------------------------*/
/* 重连时丢弃旧分段，并绑定新的连接代数。 */
void mqtt_receive_reset(mqtt_receive_state_t *receive, uint32_t generation);
/* 按序收集分段；只有 COMPLETE 时 payload 才可作为完整业务报文使用。 */
int mqtt_receive_feed(mqtt_receive_state_t *receive, uint32_t generation, uint16_t message_id, const char *topic,
                 size_t total, const void *payload, size_t size);
