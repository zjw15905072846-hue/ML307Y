/*------------------------------------------includes--------------------------------------------*/
#include "mqtt/kaiwan_cloud.h"
#include "mqtt/mqtt_config.h"
#include <string.h>
#include <stdio.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : kaiwan_cloud_text_length
* Description    : 在固定容量内寻找字符串结束符，避免对外部配置使用无界strlen
* Input          : text - 待检查字符串
*                  capacity - 缓存容量
*                  length - 输出长度
* Output         : length - 定位到的结束符位置
* Return         : 1 - 找到结束符；0 - 为空、非法或未找到
* Attention      : 只在capacity范围内查找，杜绝越界读取
*******************************************************************************/
int kaiwan_cloud_text_length(const char *text, size_t capacity, size_t *length)
{
    size_t index;

    if ((text == NULL) || (length == NULL) || (capacity == 0U))
    {
        return 0;
    }
    for (index = 0U; index < capacity; ++index)
    {
        if (text[index] == '\0')
        {
            *length = index;
            return 1;
        }
    }
    return 0;
}

/*******************************************************************************
* Function Name  : kaiwan_cloud_imei_is_valid
* Description    : 校验15位纯数字IMEI及结尾字符
* Input          : imei - 15位IMEI字符串
* Output         : 无
* Return         : 1 - 合法；0 - 为空或含非数字
* Attention      : 前15位必须全为数字，末位必须是'\0'
*******************************************************************************/
static int kaiwan_cloud_imei_is_valid(const char imei[KAIWAN_CLOUD_IMEI_SIZE])
{
    size_t index;

    if (imei == NULL)
    {
        return 0;
    }
    for (index = 0U; index < (KAIWAN_CLOUD_IMEI_SIZE - 1U); ++index)
    {
        if ((imei[index] < '0') || (imei[index] > '9'))
        {
            return 0;
        }
    }
    return imei[KAIWAN_CLOUD_IMEI_SIZE - 1U] == '\0';
}

/*******************************************************************************
* Function Name  : kaiwan_cloud_config_init
* Description    : 填充MQTT协议、超时和重连参数的安全默认值
* Input          : config - 待填充的配置
* Output         : config - 已填默认值的配置
* Return         : 无
* Attention      : 先清零再统一赋值
*******************************************************************************/
void kaiwan_cloud_config_init(kaiwan_cloud_config_t *config)
{
    if (config == NULL)
    {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->role = KAIWAN_ROLE_BRANCH;
    config->route_mode = KAIWAN_ROUTE_PLATFORM_FORWARD;
    config->broker_port = 1883U;
    config->mqtt_version = 4U;
    config->qos = 1U;
    config->clean_session = true;
    config->keepalive_seconds = 60U;
    config->command_timeout_ms = 30000U;
    config->yield_ms = 100U;
    config->pdp_poll_ms = 1000U;
    config->reconnect_minimum_ms = 1000U;
    config->reconnect_maximum_ms = 60000U;
}

/*******************************************************************************
* Function Name  : kaiwan_cloud_make_platform_topics
* Description    : 使用IMEI构建平台上下行Topic
* Input          : config - 目标配置
*                  imei - 本机IMEI
* Output         : config - 填充local_imei及platform_up/down_topic
* Return         : KAIWAN_CLOUD_OK - 成功；KAIWAN_CLOUD_ERROR_ARGUMENT / KAIWAN_CLOUD_ERROR_OVERSIZE - 失败
* Attention      : 生成 iot/devices/{imei}/sys/fire/aesdata/up|down
*******************************************************************************/
kaiwan_cloud_result_t kaiwan_cloud_make_platform_topics(kaiwan_cloud_config_t *config,
                                                const char imei[KAIWAN_CLOUD_IMEI_SIZE])
{
    int up_length;
    int down_length;

    if ((config == NULL) || !kaiwan_cloud_imei_is_valid(imei))
    {
        return KAIWAN_CLOUD_ERROR_ARGUMENT;
    }
    /* 主题只由核验过的 15 位 IMEI 构造，避免把任意配置拼进路径。 */
    memcpy(config->local_imei, imei, KAIWAN_CLOUD_IMEI_SIZE);
    up_length = snprintf(config->platform_up_topic, sizeof(config->platform_up_topic),
                         "iot/devices/%s/sys/fire/aesdata/up", imei);
    down_length = snprintf(config->platform_down_topic, sizeof(config->platform_down_topic),
                           "iot/devices/%s/sys/fire/aesdata/down", imei);
    /* snprintf 截断时清空两个 Topic，防止留下半个可用配置。 */
    if ((up_length <= 0) || ((size_t)up_length >= sizeof(config->platform_up_topic)) ||
        (down_length <= 0) || ((size_t)down_length >= sizeof(config->platform_down_topic)))
    {
        config->platform_up_topic[0] = '\0';
        config->platform_down_topic[0] = '\0';
        return KAIWAN_CLOUD_ERROR_OVERSIZE;
    }
    return KAIWAN_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : kaiwan_cloud_validate_config
* Description    : 在创建任务前完整验证云配置
* Input          : config - 待校验的配置
* Output         : 无
* Return         : KAIWAN_CLOUD_OK - 通过；KAIWAN_CLOUD_ERROR_ARGUMENT / KAIWAN_CLOUD_ERROR_CONFIG - 失败
* Attention      : 拒绝不完整私有路由配置与非法枚举/超时值
*******************************************************************************/
kaiwan_cloud_result_t kaiwan_cloud_validate_config(const kaiwan_cloud_config_t *config)
{
    size_t length;

    if (config == NULL)
    {
        return KAIWAN_CLOUD_ERROR_ARGUMENT;
    }
    if (!kaiwan_cloud_text_length(config->broker_host, sizeof(config->broker_host), &length) ||
        (length == 0U) || (config->broker_port == 0U) ||
        !kaiwan_cloud_text_length(config->client_id, sizeof(config->client_id), &length) ||
        (length == 0U) ||
        !kaiwan_cloud_text_length(config->platform_down_topic, sizeof(config->platform_down_topic),
                              &length) ||
        (length == 0U) ||
        !kaiwan_cloud_text_length(config->platform_up_topic, sizeof(config->platform_up_topic),
                              &length) ||
        (length == 0U))
    {
        return KAIWAN_CLOUD_ERROR_CONFIG;
    }
    if ((config->role > KAIWAN_ROLE_BRANCH) || (config->route_mode > KAIWAN_ROUTE_PRIVATE_TOPIC) ||
        !kaiwan_cloud_imei_is_valid(config->local_imei) ||
        ((config->mqtt_version != 3U) && (config->mqtt_version != 4U)) || (config->qos > 2U) ||
        (config->keepalive_seconds == 0U) || (config->command_timeout_ms == 0U) ||
        (config->yield_ms == 0U) || (config->yield_ms > (uint32_t)INT32_MAX) ||
        (config->pdp_poll_ms == 0U) || (config->reconnect_minimum_ms == 0U) ||
        (config->reconnect_maximum_ms < config->reconnect_minimum_ms) ||
        (config->use_tls && (config->tls_config == NULL)))
    {
        return KAIWAN_CLOUD_ERROR_CONFIG;
    }
    if (!kaiwan_cloud_text_length(config->username, sizeof(config->username), &length) ||
        !kaiwan_cloud_text_length(config->password, sizeof(config->password), &length) ||
        !kaiwan_cloud_text_length(config->peer_receive_topic, sizeof(config->peer_receive_topic), &length) ||
        !kaiwan_cloud_text_length(config->peer_transmit_topic, sizeof(config->peer_transmit_topic), &length))
    {
        return KAIWAN_CLOUD_ERROR_CONFIG;
    }
    /* 私有中转还需要双向 Topic；分机必须知道主机 IMEI。 */
    if (config->route_mode == KAIWAN_ROUTE_PRIVATE_TOPIC)
    {
        if ((config->peer_receive_topic[0] == '\0') || (config->peer_transmit_topic[0] == '\0') ||
            ((config->role == KAIWAN_ROLE_BRANCH) && !kaiwan_cloud_imei_is_valid(config->host_imei)))
        {
            return KAIWAN_CLOUD_ERROR_CONFIG;
        }
    }
    return KAIWAN_CLOUD_OK;
}
