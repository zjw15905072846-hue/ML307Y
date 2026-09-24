/*------------------------------------------includes--------------------------------------------*/
#include "mqtt/mqtt_receive.h"
#include <string.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : mqtt_receive_reset
* Description    : 清除旧连接残留分段消息
* Input          : receive - 组包上下文；generation - 当前连接代数
* Output         : receive - 空组包状态
* Return         : 无
* Attention      : 每次连接代数改变均调用
*******************************************************************************/
void mqtt_receive_reset(mqtt_receive_state_t *receive, uint32_t generation)
{
    memset(receive, 0, sizeof(*receive));
    receive->generation = generation;
}

/*******************************************************************************
* Function Name  : mqtt_receive_feed
* Description    : 在容量限制内按序合并一个MQTT下行消息
* Input          : receive - 上下文；generation/message_id - 关联；topic - 主题；total - 总长；payload/size - 分段
* Output         : 完整消息保存在receive中
* Return         : 0未完成；1完成；负值为过期或非法分段
* Attention      : 错误时丢弃整条消息，绝不把部分报文作为业务回执
*******************************************************************************/
int mqtt_receive_feed(mqtt_receive_state_t *receive, uint32_t generation, uint16_t message_id, const char *topic,
                 size_t total, const void *payload, size_t size)
{
    size_t topic_length = 0;
    /* 连接切换后拒绝旧分段，避免跨连接拼成一条假消息。 */
    if (generation != receive->generation)
    {
        return MQTT_RECEIVE_STALE;
    }
    if (!payload || !size || !total || total > sizeof(receive->payload) || size > total)
    {
        receive->active = false;
        return MQTT_RECEIVE_INVALID;
    }
    if (topic)
    {
        while (topic_length < sizeof(receive->topic) && topic[topic_length])
        {
            ++topic_length;
        }
        if (topic_length == sizeof(receive->topic))
        {
            receive->active = false;
            return MQTT_RECEIVE_INVALID;
        }
    }
    /* 首段必须带 Topic；后续分段可省略，但消息身份不能变化。 */
    if (!receive->active)
    {
        if (!topic_length)
        {
            return MQTT_RECEIVE_INVALID;
        }
        receive->message_id = message_id;
        receive->total = total;
        receive->used = 0;
        memcpy(receive->topic, topic, topic_length + 1);
        receive->active = true;
    }
    /* 任一分段不连续或超出总长度时，丢弃正在组装的整条消息。 */
    if (message_id != receive->message_id || total != receive->total ||
        (topic_length && strcmp(topic, receive->topic) != 0) || size > receive->total - receive->used)
    {
        receive->active = false;
        return MQTT_RECEIVE_INVALID;
    }
    memcpy(receive->payload + receive->used, payload, size);
    receive->used += size;
    if (receive->used == receive->total)
    {
        receive->active = false;
        return MQTT_RECEIVE_COMPLETE;
    }
    return MQTT_RECEIVE_MORE;
}
