#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "kw_cloud.h"
/*-------------------------------------------define---------------------------------------------*/
#define MQTT_RX_MORE 0
#define MQTT_RX_COMPLETE 1
#define MQTT_RX_INVALID (-1)
#define MQTT_RX_STALE (-2)

/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    uint32_t generation;
    uint16_t message_id;
    size_t total;
    size_t used;
    bool active;
    char topic[KW_CLOUD_TOPIC_SIZE];
    uint8_t payload[KW_CLOUD_MAX_PAYLOAD_SIZE];
} mqtt_rx_t;

/*-------------------------------------------function---------------------------------------------*/
void mqtt_rx_reset(mqtt_rx_t *rx, uint32_t generation);
int mqtt_rx_feed(mqtt_rx_t *rx, uint32_t generation, uint16_t message_id, const char *topic,
                 size_t total, const void *payload, size_t size);
