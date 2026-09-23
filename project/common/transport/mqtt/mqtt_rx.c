/*------------------------------------------includes--------------------------------------------*/
#include "mqtt_rx.h"
#include <string.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : mqtt_rx_reset
* Description    : 清除旧连接残留分段消息
* Input          : rx - 组包上下文；generation - 当前连接代数
* Output         : rx - 空组包状态
* Return         : 无
* Attention      : 每次连接代数改变均调用
*******************************************************************************/
void mqtt_rx_reset(mqtt_rx_t *rx, uint32_t generation)
{
    memset(rx, 0, sizeof(*rx));
    rx->generation = generation;
}

/*******************************************************************************
* Function Name  : mqtt_rx_feed
* Description    : 在容量限制内按序合并一个MQTT下行消息
* Input          : rx - 上下文；generation/message_id - 关联；topic - 主题；total - 总长；payload/size - 分段
* Output         : 完整消息保存在rx中
* Return         : 0未完成；1完成；负值为过期或非法分段
* Attention      : 错误时丢弃整条消息，绝不把部分报文作为业务回执
*******************************************************************************/
int mqtt_rx_feed(mqtt_rx_t *rx, uint32_t generation, uint16_t message_id, const char *topic,
                 size_t total, const void *payload, size_t size)
{
    size_t topic_length = 0;
    if (generation != rx->generation)
    {
        return MQTT_RX_STALE;
    }
    if (!payload || !size || !total || total > sizeof(rx->payload) || size > total)
    {
        rx->active = false;
        return MQTT_RX_INVALID;
    }
    if (topic)
    {
        while (topic_length < sizeof(rx->topic) && topic[topic_length])
        {
            ++topic_length;
        }
        if (topic_length == sizeof(rx->topic))
        {
            rx->active = false;
            return MQTT_RX_INVALID;
        }
    }
    if (!rx->active)
    {
        if (!topic_length)
        {
            return MQTT_RX_INVALID;
        }
        rx->message_id = message_id;
        rx->total = total;
        rx->used = 0;
        memcpy(rx->topic, topic, topic_length + 1);
        rx->active = true;
    }
    if (message_id != rx->message_id || total != rx->total ||
        (topic_length && strcmp(topic, rx->topic) != 0) || size > rx->total - rx->used)
    {
        rx->active = false;
        return MQTT_RX_INVALID;
    }
    memcpy(rx->payload + rx->used, payload, size);
    rx->used += size;
    if (rx->used == rx->total)
    {
        rx->active = false;
        return MQTT_RX_COMPLETE;
    }
    return MQTT_RX_MORE;
}
