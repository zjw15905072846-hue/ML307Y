/*------------------------------------------includes--------------------------------------------*/
#include "ml307y_port.h"
#include "kw_cloud.h"
#include "mqtt_config.h"
#include "mqtt_rx.h"
#include "cm_mqtt.h"
#include "cm_modem.h"
#include "cm_ssl.h"
#include "cm_os.h"
#include <stdlib.h>
#include <string.h>
/*-------------------------------------------define---------------------------------------------*/
#define ML_MQTT_EVENTS 24

/*-------------------------------------------typedef---------------------------------------------*/
typedef enum
{
    ML_CONNECTION,
    ML_SUBSCRIBED,
    ML_PUBLISHED,
    ML_TIMEOUT,
    ML_RECEIVED
} ml_mqtt_event_kind_t;

typedef struct
{
    ml_mqtt_event_kind_t kind;
    uint32_t generation;
    int result;
    uint16_t id;
    size_t total;
    size_t length;
    char topic[KW_CLOUD_TOPIC_SIZE];
    uint8_t *payload;
} ml_mqtt_event_t;

typedef struct
{
    kw_transport_t interface;
    system_if_t *system;
    cm_mqtt_client_t *client;
    osMessageQueueId_t events;
    kw_cloud_config_t config;
    kw_cloud_callbacks_t callbacks;
    cm_mqtt_connect_options_t options;
    cm_mqtt_client_cb_t sdk_callbacks;
    mqtt_rx_t rx;
    uint32_t callback_generation;
    uint32_t generation;
    uint32_t callback_error;
    uint32_t next_connect;
    uint32_t phase_started;
    uint32_t tx_started;
    uint32_t cookie;
    uint16_t subscription_id;
    uint16_t publish_id;
    bool configured;
    bool wanted;
    bool connecting;
    bool subscribed;
    bool publishing;
    uint8_t *tx_payload;
    char tx_topic[KW_CLOUD_TOPIC_SIZE];
} ml_mqtt_t;

/*-------------------------------------------variables-------------------------------------------*/
/* Registry only maps vendor client pointers to independent, lifetime-stable contexts. */
static ml_mqtt_t *s_clients[CM_MQTT_CLIENT_MAX];

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : ml_lookup
* Description    : 从SDK客户端定位独立传输上下文
* Input          : client - SDK客户端
* Output         : 无
* Return         : 对应上下文或NULL
* Attention      : 上下文在固件运行期间不释放，避免异步销毁悬空
*******************************************************************************/
static ml_mqtt_t *ml_lookup(cm_mqtt_client_t *client)
{
    unsigned i;
    for (i = 0; i < CM_MQTT_CLIENT_MAX; ++i)
    {
        if (s_clients[i] && s_clients[i]->client == client)
        {
            return s_clients[i];
        }
    }
    return NULL;
}

/*******************************************************************************
* Function Name  : ml_post
* Description    : 将SDK事件非阻塞投递到所属后台任务
* Input          : m - 客户端；event - 已复制数据
* Output         : 事件队列或错误标记
* Return         : 0成功；-1失败
* Attention      : 失败时释放接收缓冲，不生成业务确认
*******************************************************************************/
static int ml_post(ml_mqtt_t *m, ml_mqtt_event_t *event)
{
    if (!m || osMessageQueuePut(m->events, event, 0, 0) != osOK)
    {
        free(event->payload);
        if (m)
        {
            __atomic_store_n(&m->callback_error, 1U, __ATOMIC_RELEASE);
        }
        return -1;
    }
    return 0;
}

/*******************************************************************************
* Function Name  : ml_connection_cb
* Description    : 捕获连接变化并标注回调代数
* Input          : client - SDK客户端；session - 保留；result - 连接状态
* Output         : 后台连接事件
* Return         : 投递状态
* Attention      : SDK回调中不执行产品业务或存储
*******************************************************************************/
static int ml_connection_cb(cm_mqtt_client_t *client, int session, int result)
{
    ml_mqtt_t *m = ml_lookup(client);
    ml_mqtt_event_t event = {0};
    (void)session;
    if (!m)
    {
        return -1;
    }
    event.kind = ML_CONNECTION;
    event.result = result;
    event.generation = __atomic_add_fetch(&m->callback_generation, 1U, __ATOMIC_ACQ_REL);
    return ml_post(m, &event);
}

/*******************************************************************************
* Function Name  : ml_subscribed_cb
* Description    : 复制订阅确认，不提前声明在线
* Input          : client - 客户端；id - 包编号；count/qos - 结果
* Output         : 订阅事件
* Return         : 投递状态
* Attention      : 只有全部主题订阅成功才上线
*******************************************************************************/
static int ml_subscribed_cb(cm_mqtt_client_t *client, unsigned short id, int count, int qos[])
{
    ml_mqtt_t *m = ml_lookup(client);
    ml_mqtt_event_t event = {0};
    if (!m)
    {
        return -1;
    }
    event.kind = ML_SUBSCRIBED;
    event.id = id;
    event.generation = __atomic_load_n(&m->callback_generation, __ATOMIC_ACQUIRE);
    event.result = count == 1 && qos && qos[0] >= 0 && qos[0] <= 2 ? 0 : -1;
    return ml_post(m, &event);
}

/*******************************************************************************
* Function Name  : ml_published_cb
* Description    : 捕获MQTT PUBACK用于传输结果
* Input          : client - 客户端；id - 包编号；dup - 重复标记
* Output         : 传输事件
* Return         : 投递状态
* Attention      : 不删除报警记录
*******************************************************************************/
static int ml_published_cb(cm_mqtt_client_t *client, unsigned short id, char dup)
{
    ml_mqtt_t *m = ml_lookup(client);
    ml_mqtt_event_t event = {0};
    (void)dup;
    if (!m)
    {
        return -1;
    }
    event.kind = ML_PUBLISHED;
    event.id = id;
    event.generation = __atomic_load_n(&m->callback_generation, __ATOMIC_ACQUIRE);
    return ml_post(m, &event);
}

/*******************************************************************************
* Function Name  : ml_timeout_cb
* Description    : 捕获SDK报文超时
* Input          : client - 客户端；id - 包编号
* Output         : 后台超时事件
* Return         : 投递状态
* Attention      : 由后台按当前连接及包编号判断
*******************************************************************************/
static int ml_timeout_cb(cm_mqtt_client_t *client, unsigned short id)
{
    ml_mqtt_t *m = ml_lookup(client);
    ml_mqtt_event_t event = {0};
    if (!m)
    {
        return -1;
    }
    event.kind = ML_TIMEOUT;
    event.id = id;
    event.generation = __atomic_load_n(&m->callback_generation, __ATOMIC_ACQUIRE);
    return ml_post(m, &event);
}

/*******************************************************************************
* Function Name  : ml_receive_cb
* Description    : 在SDK缓冲有效期内复制受限分段数据
* Input          : client/id/topic - 消息身份；total/length - 长度；payload - 内容
* Output         : 拥有独立缓冲的接收事件
* Return         : 0已排队；-1拒绝
* Attention      : 不解析未完整的铠湾回执
*******************************************************************************/
static int ml_receive_cb(cm_mqtt_client_t *client, unsigned short id, char *topic, int total,
                         int length, char *payload)
{
    ml_mqtt_t *m = ml_lookup(client);
    ml_mqtt_event_t event = {0};
    size_t topic_length = 0;
    if (!m)
    {
        return -1;
    }
    if (total <= 0 || length <= 0 || length > total || total > (int)KW_CLOUD_MAX_PAYLOAD_SIZE ||
        !payload || (topic && !kw_cloud_text_length(topic, sizeof(event.topic), &topic_length)))
    {
        __atomic_store_n(&m->callback_error, 1U, __ATOMIC_RELEASE);
        return -1;
    }
    event.payload = malloc((size_t)length);
    if (!event.payload)
    {
        __atomic_store_n(&m->callback_error, 1U, __ATOMIC_RELEASE);
        return -1;
    }
    event.kind = ML_RECEIVED;
    event.generation = __atomic_load_n(&m->callback_generation, __ATOMIC_ACQUIRE);
    event.id = id;
    event.total = (size_t)total;
    event.length = (size_t)length;
    if (topic)
    {
        memcpy(event.topic, topic, topic_length + 1);
    }
    memcpy(event.payload, payload, (size_t)length);
    return ml_post(m, &event);
}

/*******************************************************************************
* Function Name  : ml_tx_done
* Description    : 完成一次传输回调并释放发送副本
* Input          : m - 传输上下文；result - 传输结果
* Output         : 清除发送占用并通知调用方
* Return         : 无
* Attention      : 该结果不是平台业务确认
*******************************************************************************/
static void ml_tx_done(ml_mqtt_t *m, kw_cloud_result_t result)
{
    uint32_t cookie = m->cookie;
    if (!m->publishing)
    {
        return;
    }
    m->publishing = false;
    free(m->tx_payload);
    m->tx_payload = NULL;
    if (m->callbacks.on_publish_result)
    {
        m->callbacks.on_publish_result(cookie, result, m->callbacks.user);
    }
}

/*******************************************************************************
* Function Name  : ml_offline
* Description    : 撤销上线状态并终止本连接发送占用
* Input          : m - 上下文
* Output         : 连接和组包状态
* Return         : 无
* Attention      : 未确认报警由产品保留重试
*******************************************************************************/
static void ml_offline(ml_mqtt_t *m)
{
    bool was_online = m->subscribed;
    m->subscribed = false;
    m->connecting = false;
    m->subscription_id = 0;
    mqtt_rx_reset(&m->rx, m->generation);
    ml_tx_done(m, KW_CLOUD_ERR_NETWORK);
    if (was_online && m->callbacks.on_state_changed)
    {
        m->callbacks.on_state_changed(false, m->callbacks.user);
    }
}

/*******************************************************************************
* Function Name  : ml_start
* Description    : 保存配置并请求连接，SDK客户端在上下文寿命内复用
* Input          : user - 传输；config - 配置；callbacks - 业务回调
* Output         : 异步连接请求
* Return         : KW_CLOUD状态码
* Attention      : 目前QoS1；凭据不写入日志
*******************************************************************************/
static kw_cloud_result_t ml_start(void *user, const kw_cloud_config_t *config,
                                  const kw_cloud_callbacks_t *callbacks)
{
    ml_mqtt_t *m = user;
    int enabled;
    int channel;
    uint8_t yes = 1;
    uint8_t no = 0;
    uint8_t version = 255;
    const kw_tls_config_t *tls;
    if (!m || !callbacks || kw_cloud_validate_config(config) != KW_CLOUD_OK || config->qos != 1 ||
        config->peer_rx_topic[0])
    {
        return KW_CLOUD_ERR_CONFIG;
    }
    if (m->wanted)
    {
        return KW_CLOUD_ERR_STATE;
    }
    m->config = *config;
    m->callbacks = *callbacks;
    enabled = config->use_tls ? 1 : 0;
    if (cm_mqtt_client_set_opt(m->client, CM_MQTT_OPT_SSL_ENABLE, &enabled) != 0)
    {
        return KW_CLOUD_ERR_CONFIG;
    }
    if (enabled)
    {
        tls = config->tls_config;
        if (!tls || tls->channel >= 6 || !tls->ca_file || !tls->ca_file[0])
        {
            return KW_CLOUD_ERR_CONFIG;
        }
        channel = (int)tls->channel;
        if (cm_ssl_setopt(channel, CM_SSL_PARAM_VERIFY, &yes) != 0 ||
            cm_ssl_setopt(channel, CM_SSL_PARAM_VERSION, &version) != 0 ||
            cm_ssl_setopt(channel, CM_SSL_PARAM_IGNORE_STAMP, &no) != 0 ||
            cm_ssl_setopt(channel, CM_SSL_PARAM_IGNORE_VERIFY, &no) != 0 ||
            cm_ssl_setopt(channel, CM_SSL_PARAM_SNI, &yes) != 0 ||
            cm_ssl_setopt(channel, CM_SSL_PARAM_CA_CERT_FILENAME, (void *)tls->ca_file) != 0 ||
            cm_mqtt_client_set_opt(m->client, CM_MQTT_OPT_SSL_ID, &channel) != 0)
        {
            return KW_CLOUD_ERR_CONFIG;
        }
    }
    memset(&m->options, 0, sizeof(m->options));
    m->options.hostname = m->config.broker_host;
    m->options.hostport = m->config.broker_port;
    m->options.clientid = m->config.client_id;
    m->options.username = m->config.username;
    m->options.password = m->config.password;
    m->options.keepalive = m->config.keepalive_seconds;
    m->options.clean_session = m->config.clean_session;
    m->configured = true;
    m->wanted = true;
    m->next_connect = m->system->millis(m->system->user);
    return KW_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : ml_online
* Description    : 查询完成订阅后的在线状态
* Input          : user - 传输上下文
* Output         : 无
* Return         : true允许业务发送
* Attention      : 连接成功但未收到SUBACK时为false
*******************************************************************************/
static bool ml_online(void *user)
{
    return ((ml_mqtt_t *)user)->subscribed;
}

/*******************************************************************************
* Function Name  : ml_publish
* Description    : 复制数据并提交唯一在途QoS1消息
* Input          : user - 传输；topic/payload/size - 报文；qos/retained - 选项；cookie - 关联
* Output         : 保存发送副本
* Return         : KW_CLOUD状态码
* Attention      : 业务回执由产品独立判断
*******************************************************************************/
static kw_cloud_result_t ml_publish(void *user, const char *topic, const uint8_t *payload,
                                    size_t size, uint8_t qos, bool retained, uint32_t cookie)
{
    ml_mqtt_t *m = user;
    size_t topic_length;
    int result;
    if (!m->subscribed)
    {
        return KW_CLOUD_ERR_STATE;
    }
    if (m->publishing)
    {
        return KW_CLOUD_ERR_QUEUE;
    }
    if (!payload || !size || size > KW_CLOUD_MAX_PAYLOAD_SIZE || qos != 1 ||
        !kw_cloud_text_length(topic, sizeof(m->tx_topic), &topic_length) || !topic_length ||
        topic_length + size + 16 >= 4096)
    {
        return KW_CLOUD_ERR_ARGUMENT;
    }
    m->tx_payload = malloc(size);
    if (!m->tx_payload)
    {
        return KW_CLOUD_ERR_MEMORY;
    }
    memcpy(m->tx_payload, payload, size);
    memcpy(m->tx_topic, topic, topic_length + 1);
    m->cookie = cookie;
    m->publishing = true;
    m->tx_started = m->system->millis(m->system->user);
    result = cm_mqtt_client_publish(m->client, m->tx_topic, (const char *)m->tx_payload, (int)size,
                                    CM_MQTT_QOS_1 | (retained ? CM_MQTT_RETAIN_1 : 0));
    if (result < 0)
    {
        ml_tx_done(m, KW_CLOUD_ERR_MQTT);
        return KW_CLOUD_ERR_MQTT;
    }
    result = cm_mqtt_client_get_msgid(m->client);
    if (result <= 0)
    {
        ml_tx_done(m, KW_CLOUD_ERR_MQTT);
        return KW_CLOUD_ERR_MQTT;
    }
    m->publish_id = (uint16_t)result;
    return KW_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : ml_poll
* Description    : 串行分派有限回调并推进连接订阅及超时
* Input          : user - 传输上下文；now - 单调毫秒
* Output         : 连接状态及用户回调
* Return         : 无
* Attention      : 仅后台调用；旧代数事件一律释放丢弃
*******************************************************************************/
static void ml_poll(void *user, uint32_t now)
{
    ml_mqtt_t *m = user;
    ml_mqtt_event_t event;
    unsigned count;
    int result;
    const char *topic;
    char qos = 1;
    if (__atomic_exchange_n(&m->callback_error, 0U, __ATOMIC_ACQ_REL))
    {
        /* Invalidate every queued event before requesting the asynchronous disconnect. */
        m->generation = __atomic_add_fetch(&m->callback_generation, 1U, __ATOMIC_ACQ_REL);
        m->next_connect = now + m->config.reconnect_min_ms;
        ml_offline(m);
        (void)cm_mqtt_client_disconnect(m->client);
        m->system->fault("mqtt-callback-queue", KW_CLOUD_ERR_QUEUE);
    }
    for (count = 0; count < 8 && osMessageQueueGet(m->events, &event, NULL, 0) == osOK; ++count)
    {
        if (event.generation != __atomic_load_n(&m->callback_generation, __ATOMIC_ACQUIRE))
        {
            free(event.payload);
            continue;
        }
        if (event.kind == ML_CONNECTION)
        {
            m->generation = event.generation;
            ml_offline(m);
            m->next_connect = now + m->config.reconnect_min_ms;
            if (m->wanted && event.result == 0)
            {
                topic = m->config.platform_down_topic;
                result = cm_mqtt_client_subscribe(m->client, &topic, &qos, 1);
                if (result >= 0)
                {
                    m->subscription_id = (uint16_t)cm_mqtt_client_get_msgid(m->client);
                    m->connecting = true;
                    m->phase_started = now;
                }
                else
                {
                    (void)cm_mqtt_client_disconnect(m->client);
                }
            }
        }
        else if (event.generation == m->generation && m->wanted)
        {
            if (event.kind == ML_SUBSCRIBED && m->connecting && event.id == m->subscription_id)
            {
                if (event.result == 0 && !m->subscribed)
                {
                    m->subscribed = true;
                    m->connecting = false;
                    if (m->callbacks.on_state_changed)
                    {
                        m->callbacks.on_state_changed(true, m->callbacks.user);
                    }
                }
                else if (event.result != 0)
                {
                    ml_offline(m);
                    (void)cm_mqtt_client_disconnect(m->client);
                }
            }
            else if ((event.kind == ML_PUBLISHED || event.kind == ML_TIMEOUT) && m->publishing &&
                     event.id == m->publish_id)
            {
                ml_tx_done(m, event.kind == ML_PUBLISHED ? KW_CLOUD_OK : KW_CLOUD_ERR_MQTT);
            }
            else if (event.kind == ML_RECEIVED && m->subscribed)
            {
                result = mqtt_rx_feed(&m->rx, event.generation, event.id,
                                      event.topic[0] ? event.topic : NULL, event.total,
                                      event.payload, event.length);
                if (result == MQTT_RX_COMPLETE && m->callbacks.on_message)
                {
                    result = m->callbacks.on_message(m->rx.topic, strlen(m->rx.topic),
                                                     m->rx.payload, m->rx.total, m->callbacks.user);
                }
                if (result < 0 && m->callbacks.on_receive_error)
                {
                    m->callbacks.on_receive_error(KW_CLOUD_ERR_MQTT, m->callbacks.user);
                }
            }
        }
        free(event.payload);
    }
    if (m->publishing && (uint32_t)(now - m->tx_started) >= m->config.command_timeout_ms)
    {
        ml_tx_done(m, KW_CLOUD_ERR_MQTT);
    }
    if (m->connecting && (uint32_t)(now - m->phase_started) >= m->config.command_timeout_ms)
    {
        ml_offline(m);
        (void)cm_mqtt_client_disconnect(m->client);
        m->next_connect = now + m->config.reconnect_min_ms;
    }
    if (m->wanted && !m->subscribed && !m->connecting && (int32_t)(now - m->next_connect) >= 0 &&
        cm_mqtt_client_get_state(m->client) == CM_MQTT_STATE_DISCONNECTED)
    {
        m->next_connect = now + m->config.reconnect_min_ms;
        if (cm_modem_get_pdp_state(1) == 1)
        {
            if (cm_mqtt_client_connect(m->client, &m->options) == 0)
            {
                m->connecting = true;
                m->phase_started = now;
            }
        }
    }
}

/*******************************************************************************
* Function Name  : ml_stop
* Description    : 请求停止并等待SDK断开及回调排空
* Input          : user - 传输上下文
* Output         : 断开状态
* Return         : true完全静止；false仍需poll
* Attention      : 不异步销毁客户端，避免回调使用已释放对象
*******************************************************************************/
static bool ml_stop(void *user)
{
    ml_mqtt_t *m = user;
    if (m->wanted)
    {
        m->wanted = false;
        m->generation = __atomic_add_fetch(&m->callback_generation, 1U, __ATOMIC_ACQ_REL);
        ml_offline(m);
        (void)cm_mqtt_client_disconnect(m->client);
    }
    return cm_mqtt_client_get_state(m->client) == CM_MQTT_STATE_DISCONNECTED &&
           osMessageQueueGetCount(m->events) == 0 && !m->publishing;
}

/*******************************************************************************
* Function Name  : ml_mqtt_create
* Description    : 为当前产品创建显式传输实例
* Input          : services - 产品服务
* Output         : services.transport
* Return         : true成功
* Attention      : 最多使用SDK允许的客户端数量，不连接任何服务器
*******************************************************************************/
bool ml_mqtt_create(product_services_t *services)
{
    ml_mqtt_t *m;
    unsigned slot;
    for (slot = 0; slot < CM_MQTT_CLIENT_MAX && s_clients[slot]; ++slot)
    {
    }
    if (slot == CM_MQTT_CLIENT_MAX)
    {
        return false;
    }
    m = calloc(1, sizeof(*m));
    if (!m)
    {
        return false;
    }
    m->events = osMessageQueueNew(ML_MQTT_EVENTS, sizeof(ml_mqtt_event_t), NULL);
    if (!m->events)
    {
        free(m);
        return false;
    }
    m->client = cm_mqtt_client_create();
    if (!m->client)
    {
        osMessageQueueDelete(m->events);
        free(m);
        return false;
    }
    m->system = &services->system;
    s_clients[slot] = m;
    m->sdk_callbacks.connack_cb = ml_connection_cb;
    m->sdk_callbacks.suback_cb = ml_subscribed_cb;
    m->sdk_callbacks.puback_cb = ml_published_cb;
    m->sdk_callbacks.publish_cb = ml_receive_cb;
    m->sdk_callbacks.timeout_cb = ml_timeout_cb;
    if (cm_mqtt_client_set_opt(m->client, CM_MQTT_OPT_EVENT, &m->sdk_callbacks) != 0)
    {
        /* Keep lifetime-stable context; startup fails without enabling the product. */
        return false;
    }
    m->interface.user = m;
    m->interface.start = ml_start;
    m->interface.poll = ml_poll;
    m->interface.online = ml_online;
    m->interface.stop = ml_stop;
    m->interface.publish = ml_publish;
    services->transport = &m->interface;
    return true;
}
