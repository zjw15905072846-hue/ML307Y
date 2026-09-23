/*------------------------------------------includes--------------------------------------------*/
#include "kw_cloud.h"
#include "mqtt_config.h"
#include <string.h>
#include <stdio.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : kw_cloud_text_length
* Description    : 在固定容量内寻找字符串结束符，避免对外部配置使用无界strlen
* Input          : text - 待检查字符串
*                  capacity - 缓存容量
*                  length - 输出长度
* Output         : length - 定位到的结束符位置
* Return         : 1 - 找到结束符；0 - 为空、非法或未找到
* Attention      : 只在capacity范围内查找，杜绝越界读取
*******************************************************************************/
int kw_cloud_text_length(const char *text, size_t capacity, size_t *length)
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
* Function Name  : kw_cloud_imei_is_valid
* Description    : 校验15位纯数字IMEI及结尾字符
* Input          : imei - 15位IMEI字符串
* Output         : 无
* Return         : 1 - 合法；0 - 为空或含非数字
* Attention      : 前15位必须全为数字，末位必须是'\0'
*******************************************************************************/
static int kw_cloud_imei_is_valid(const char imei[KW_CLOUD_IMEI_SIZE])
{
    size_t index;

    if (imei == NULL)
    {
        return 0;
    }
    for (index = 0U; index < (KW_CLOUD_IMEI_SIZE - 1U); ++index)
    {
        if ((imei[index] < '0') || (imei[index] > '9'))
        {
            return 0;
        }
    }
    return imei[KW_CLOUD_IMEI_SIZE - 1U] == '\0';
}

/*******************************************************************************
* Function Name  : kw_cloud_config_init
* Description    : 填充MQTT协议、超时和重连参数的安全默认值
* Input          : config - 待填充的配置
* Output         : config - 已填默认值的配置
* Return         : 无
* Attention      : 先清零再统一赋值
*******************************************************************************/
void kw_cloud_config_init(kw_cloud_config_t *config)
{
    if (config == NULL)
    {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->role = KW_ROLE_BRANCH;
    config->route_mode = KW_ROUTE_PLATFORM_FORWARD;
    config->broker_port = 1883U;
    config->mqtt_version = 4U;
    config->qos = 1U;
    config->clean_session = true;
    config->keepalive_seconds = 60U;
    config->command_timeout_ms = 30000U;
    config->yield_ms = 100U;
    config->pdp_poll_ms = 1000U;
    config->reconnect_min_ms = 1000U;
    config->reconnect_max_ms = 60000U;
}

/*******************************************************************************
* Function Name  : kw_cloud_make_platform_topics
* Description    : 使用IMEI构建平台上下行Topic
* Input          : config - 目标配置
*                  imei - 本机IMEI
* Output         : config - 填充local_imei及platform_up/down_topic
* Return         : KW_CLOUD_OK - 成功；KW_CLOUD_ERR_ARGUMENT / KW_CLOUD_ERR_OVERSIZE - 失败
* Attention      : 生成 iot/devices/{imei}/sys/fire/aesdata/up|down
*******************************************************************************/
kw_cloud_result_t kw_cloud_make_platform_topics(kw_cloud_config_t *config,
                                                const char imei[KW_CLOUD_IMEI_SIZE])
{
    int up_length;
    int down_length;

    if ((config == NULL) || !kw_cloud_imei_is_valid(imei))
    {
        return KW_CLOUD_ERR_ARGUMENT;
    }
    memcpy(config->local_imei, imei, KW_CLOUD_IMEI_SIZE);
    up_length = snprintf(config->platform_up_topic, sizeof(config->platform_up_topic),
                         "iot/devices/%s/sys/fire/aesdata/up", imei);
    down_length = snprintf(config->platform_down_topic, sizeof(config->platform_down_topic),
                           "iot/devices/%s/sys/fire/aesdata/down", imei);
    if ((up_length <= 0) || ((size_t)up_length >= sizeof(config->platform_up_topic)) ||
        (down_length <= 0) || ((size_t)down_length >= sizeof(config->platform_down_topic)))
    {
        config->platform_up_topic[0] = '\0';
        config->platform_down_topic[0] = '\0';
        return KW_CLOUD_ERR_OVERSIZE;
    }
    return KW_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : kw_cloud_validate_config
* Description    : 在创建任务前完整验证云配置
* Input          : config - 待校验的配置
* Output         : 无
* Return         : KW_CLOUD_OK - 通过；KW_CLOUD_ERR_ARGUMENT / KW_CLOUD_ERR_CONFIG - 失败
* Attention      : 拒绝不完整私有路由配置与非法枚举/超时值
*******************************************************************************/
kw_cloud_result_t kw_cloud_validate_config(const kw_cloud_config_t *config)
{
    size_t length;

    if (config == NULL)
    {
        return KW_CLOUD_ERR_ARGUMENT;
    }
    if (!kw_cloud_text_length(config->broker_host, sizeof(config->broker_host), &length) ||
        (length == 0U) || (config->broker_port == 0U) ||
        !kw_cloud_text_length(config->client_id, sizeof(config->client_id), &length) ||
        (length == 0U) ||
        !kw_cloud_text_length(config->platform_down_topic, sizeof(config->platform_down_topic),
                              &length) ||
        (length == 0U) ||
        !kw_cloud_text_length(config->platform_up_topic, sizeof(config->platform_up_topic),
                              &length) ||
        (length == 0U))
    {
        return KW_CLOUD_ERR_CONFIG;
    }
    if ((config->role > KW_ROLE_BRANCH) || (config->route_mode > KW_ROUTE_PRIVATE_TOPIC) ||
        !kw_cloud_imei_is_valid(config->local_imei) ||
        ((config->mqtt_version != 3U) && (config->mqtt_version != 4U)) || (config->qos > 2U) ||
        (config->keepalive_seconds == 0U) || (config->command_timeout_ms == 0U) ||
        (config->yield_ms == 0U) || (config->yield_ms > (uint32_t)INT32_MAX) ||
        (config->pdp_poll_ms == 0U) || (config->reconnect_min_ms == 0U) ||
        (config->reconnect_max_ms < config->reconnect_min_ms) ||
        (config->use_tls && (config->tls_config == NULL)))
    {
        return KW_CLOUD_ERR_CONFIG;
    }
    if (!kw_cloud_text_length(config->username, sizeof(config->username), &length) ||
        !kw_cloud_text_length(config->password, sizeof(config->password), &length) ||
        !kw_cloud_text_length(config->peer_rx_topic, sizeof(config->peer_rx_topic), &length) ||
        !kw_cloud_text_length(config->peer_tx_topic, sizeof(config->peer_tx_topic), &length))
    {
        return KW_CLOUD_ERR_CONFIG;
    }
    if (config->route_mode == KW_ROUTE_PRIVATE_TOPIC)
    {
        if ((config->peer_rx_topic[0] == '\0') || (config->peer_tx_topic[0] == '\0') ||
            ((config->role == KW_ROLE_BRANCH) && !kw_cloud_imei_is_valid(config->host_imei)))
        {
            return KW_CLOUD_ERR_CONFIG;
        }
    }
    return KW_CLOUD_OK;
}
