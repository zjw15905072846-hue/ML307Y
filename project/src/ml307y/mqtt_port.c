/*------------------------------------------includes--------------------------------------------*/
#include "ml307y/ml307y_port.h"
#include "ml307y/diag_uart.h"
#include "mqtt/kaiwan_cloud.h"
#include "mqtt/mqtt_config.h"
#include "mqtt/mqtt_receive.h"
#include "cm_mqtt.h"
#include "cm_modem.h"
#include "cm_ssl.h"
#include "cm_os.h"
#include "cm_mem.h"
#include <string.h>
/*-------------------------------------------define---------------------------------------------*/
#define ML307Y_MQTT_EVENTS 24 /* SDK 回调投递到后台的事件队列容量。 */
#define ML307Y_MQTT_TRACE_INTERVAL_MS 5000U /* 状态查询诊断限速，不改变连接重试节奏。 */
#define ML307Y_MQTT_ADDRESS_CAPACITY 46U /* 保存实际连接的 IPv4 或 IPv6 文本。 */

/*-------------------------------------------typedef---------------------------------------------*/
/* 供应商回调只产生事件，状态机统一在 ml307y_poll 中推进。 */
typedef enum
{
    ML307Y_CONNECTION, /* 连接状态变化。 */
    ML307Y_SUBSCRIBED, /* 下行 Topic 订阅确认。 */
    ML307Y_PUBLISHED,  /* MQTT 传输确认。 */
    ML307Y_TIMEOUT,    /* 包级超时。 */
    ML307Y_RECEIVED    /* 下行消息分段。 */
} ml307y_mqtt_event_kind_t;

typedef struct
{
    ml307y_mqtt_event_kind_t kind;
    uint32_t generation; /* 回调产生时的连接代数，旧连接事件会被丢弃。 */
    int result;
    int local_address_result; /* 成功连接时的地址查询结果；4/6 为有效 IP 版本。 */
    char local_address[ML307Y_MQTT_ADDRESS_CAPACITY]; /* 对应本事件的源地址快照。 */
    uint16_t id;
    size_t total;
    size_t length;
    char topic[KAIWAN_CLOUD_TOPIC_SIZE];
    uint8_t *payload; /* 回调里复制的分段，由后台消费后释放。 */
} ml307y_mqtt_event_t;

typedef struct
{
    kaiwan_transport_t interface;
    system_interface_t *system;
    cm_mqtt_client_t *client;
    osMessageQueueId_t events;
    kaiwan_cloud_config_t config;
    kaiwan_cloud_callbacks_t callbacks;
    cm_mqtt_connect_options_t options;
    cm_mqtt_client_cb_t sdk_callbacks;
    mqtt_receive_state_t receive;
    uint32_t callback_generation; /* SDK 回调侧的连接代数。 */
    uint32_t generation;          /* 后台当前接受的连接代数。 */
    uint32_t callback_error;      /* 队列溢出或复制失败标志。 */
    uint32_t event_pending; /* 可合并的通知，不依赖应用队列是否有空位。 */
    void (*notify)(void *);
    void *notify_argument;
    uint32_t next_connect;
    uint32_t next_stop; /* 停止失败五秒后重试，禁止重新连接。 */
    uint32_t phase_started;
    uint32_t transmit_started;
    uint32_t next_trace; /* 最早允许再次打印 SDK 状态查询的位置。 */
    uint32_t cookie;
    uint16_t subscription_id;
    uint16_t publish_id;
    bool configured;
    bool wanted;
    bool connecting;
    bool subscribed;
    bool publishing;
    bool polling_started; /* 首轮打印原子交换前后位置，后续轮询不重复输出。 */
    uint8_t *transmit_payload; /* 唯一在途消息的深拷贝，传输结束后释放。 */
    char transmit_topic[KAIWAN_CLOUD_TOPIC_SIZE];
} ml307y_mqtt_state_t;

/*-------------------------------------------variables-------------------------------------------*/
/* 仅将 SDK 客户端指针映射到寿命稳定的独立上下文。 */
static ml307y_mqtt_state_t *mqtt_clients[CM_MQTT_CLIENT_MAX];

/*-------------------------------------------function---------------------------------------------*/
static void ml307y_signal(ml307y_mqtt_state_t *mqtt_state);
/*******************************************************************************
* Function Name  : ml307y_mqtt_diagnostic
* Description    : 复用统一串口入口记录 SDK 调用前后及回调结果
* Input          : mqtt_state - 传输上下文；stage - 阶段；result - SDK 返回值
* Output         : 云流程诊断
* Return         : 无
* Attention      : 仅输出固定阶段名和返回码，不输出账号、密码或密钥
*******************************************************************************/
static void ml307y_mqtt_diagnostic(ml307y_mqtt_state_t *mqtt_state, const char *stage, int result)
{
    if (mqtt_state->system->diagnostic)
    {
        mqtt_state->system->diagnostic(stage, mqtt_state->generation, result);
    }
}

/*******************************************************************************
* Function Name  : ml307y_report_local_address
* Description    : 通过统一 UART0 入口打印本次 MQTT 实际使用的源地址
* Input          : event - 成功连接回调保存的地址快照
* Output         : IP 版本、本机地址或明确的查询失败结果
* Return         : 无
* Attention      : 打印携带事件代数的快照，短暂连接断开也保留记录，不推进旧连接业务
*******************************************************************************/
static void ml307y_report_local_address(const ml307y_mqtt_event_t *event)
{
    if ((event->local_address_result == 4 || event->local_address_result == 6) && event->local_address[0])
    {
        ml307y_uart_diag_printf("[project][mqtt-local-ip] generation=%lu family=%s local=%.*s",
                                (unsigned long)event->generation,
                                event->local_address_result == 4 ? "IPv4" : "IPv6",
                                (int)sizeof(event->local_address) - 1, event->local_address);
    }
    else
    {
        ml307y_uart_diag_printf("[project][mqtt-local-ip] generation=%lu family=unavailable local=unavailable result=%d",
                                (unsigned long)event->generation,
                                event->local_address_result);
    }
}

/*******************************************************************************
* Function Name  : ml307y_lookup
* Description    : 从SDK客户端定位独立传输上下文
* Input          : client - SDK客户端
* Output         : 无
* Return         : 对应上下文或NULL
* Attention      : 上下文在固件运行期间不释放，避免异步销毁悬空
*******************************************************************************/
static ml307y_mqtt_state_t *ml307y_lookup(cm_mqtt_client_t *client)
{
    unsigned index;
    for (index = 0; index < CM_MQTT_CLIENT_MAX; ++index)
    {
        if (mqtt_clients[index] && mqtt_clients[index]->client == client)
        {
            return mqtt_clients[index];
        }
    }
    return NULL;
}

/*******************************************************************************
* Function Name  : ml307y_post
* Description    : 将SDK事件非阻塞投递到所属后台任务
* Input          : mqtt_state - 客户端；event - 已复制数据
* Output         : 事件队列或错误标记
* Return         : 0成功；-1失败
* Attention      : 失败时释放接收缓冲，不生成业务确认
*******************************************************************************/
static int ml307y_post(ml307y_mqtt_state_t *mqtt_state, ml307y_mqtt_event_t *event)
{
    if (!mqtt_state || osMessageQueuePut(mqtt_state->events, event, 0, 0) != osOK)
    {
        /* 控制事件没有接收缓冲，SDK 禁止释放 NULL。 */
        if (event->payload != NULL)
        {
            cm_free(event->payload);
        }
        if (mqtt_state)
        {
            __atomic_store_n(&mqtt_state->callback_error, 1U, __ATOMIC_RELEASE);
            ml307y_signal(mqtt_state);
        }
        return -1;
    }
    ml307y_signal(mqtt_state);
    return 0;
}

/*******************************************************************************
* Function Name  : ml307y_signal
* Description    : 在事件或错误产生后发布可合并的唤醒通知
* Input          : mqtt_state - 传输上下文
* Output         : 原子待处理标志和后台通知
* Return         : 无
* Attention      : 回调中不执行业务；即使队列满也保留待处理状态
*******************************************************************************/
static void ml307y_signal(ml307y_mqtt_state_t *mqtt_state)
{
    __atomic_store_n(&mqtt_state->event_pending, 1U, __ATOMIC_RELEASE);
    if (mqtt_state->notify)
    {
        mqtt_state->notify(mqtt_state->notify_argument);
    }
}

/*******************************************************************************
* Function Name  : ml307y_set_notify
* Description    : 启动 SDK 连接前绑定后台唤醒入口
* Input          : user - 传输上下文；notify/argument - 通知及参数
* Output         : 保存通知入口
* Return         : 无
* Attention      : 连接活动期间不重新绑定
*******************************************************************************/
static void ml307y_set_notify(void *user, void (*notify)(void *), void *argument)
{
    ml307y_mqtt_state_t *mqtt_state = user;
    mqtt_state->notify_argument = argument;
    mqtt_state->notify = notify;
}

/*******************************************************************************
* Function Name  : ml307y_next_wait
* Description    : 计算网络事件、连接和发送超时的最近处理期限
* Input          : user - 传输上下文；now - 当前单调毫秒
* Output         : 无
* Return         : 等待毫秒；UINT32_MAX 表示等待 SDK 事件
* Attention      : SDK 自行保活；有待处理事件不得无限等待
*******************************************************************************/
static uint32_t ml307y_next_wait(void *user, uint32_t now)
{
    ml307y_mqtt_state_t *mqtt_state = user;
    uint32_t wait = UINT32_MAX;
    uint32_t candidate;
    if (__atomic_load_n(&mqtt_state->event_pending, __ATOMIC_ACQUIRE) ||
        __atomic_load_n(&mqtt_state->callback_error, __ATOMIC_ACQUIRE) ||
        osMessageQueueGetCount(mqtt_state->events))
    {
        return 0;
    }
    if (mqtt_state->publishing)
    {
        candidate = mqtt_state->transmit_started + mqtt_state->config.command_timeout_ms;
        wait = (int32_t)(candidate - now) <= 0 ? 0U : candidate - now;
    }
    if (mqtt_state->connecting)
    {
        candidate = mqtt_state->phase_started + mqtt_state->config.command_timeout_ms;
        candidate = (int32_t)(candidate - now) <= 0 ? 0U : candidate - now;
        if (candidate < wait)
        {
            wait = candidate;
        }
    }
    else if (mqtt_state->wanted && !mqtt_state->subscribed)
    {
        candidate = (int32_t)(mqtt_state->next_connect - now) <= 0 ? 0U : mqtt_state->next_connect - now;
        if (candidate < wait)
        {
            wait = candidate;
        }
    }
    return wait;
}

/*******************************************************************************
* Function Name  : ml307y_connection_callback
* Description    : 捕获连接变化并标注回调代数
* Input          : client - SDK客户端；session - 保留；result - 连接状态
* Output         : 后台连接事件
* Return         : 投递状态
* Attention      : SDK回调中不执行产品业务或存储
*******************************************************************************/
static int ml307y_connection_callback(cm_mqtt_client_t *client, int session, int result)
{
    ml307y_mqtt_state_t *mqtt_state = ml307y_lookup(client);
    ml307y_mqtt_event_t event = {0};
    (void)session;
    if (!mqtt_state)
    {
        return -1;
    }
    event.kind = ML307Y_CONNECTION;
    event.result = result;
    if (result == CM_MQTT_CONN_STATE_SUCCESS)
    {
        /* SDK 循环任务此时尚未处理后续断开，先复制实际 socket 的源地址。 */
        event.local_address_result = project_mqtt_local_address(client, event.local_address,
                                                               sizeof(event.local_address));
    }
    event.generation = __atomic_add_fetch(&mqtt_state->callback_generation, 1U, __ATOMIC_ACQ_REL);
    return ml307y_post(mqtt_state, &event);
}

/*******************************************************************************
* Function Name  : ml307y_subscribed_callback
* Description    : 复制订阅确认，不提前声明在线
* Input          : client - 客户端；id - 包编号；count/qos - 结果
* Output         : 订阅事件
* Return         : 投递状态
* Attention      : 只有全部主题订阅成功才上线
*******************************************************************************/
static int ml307y_subscribed_callback(cm_mqtt_client_t *client, unsigned short id, int count, int qos[])
{
    ml307y_mqtt_state_t *mqtt_state = ml307y_lookup(client);
    ml307y_mqtt_event_t event = {0};
    if (!mqtt_state)
    {
        return -1;
    }
    event.kind = ML307Y_SUBSCRIBED;
    event.id = id;
    event.generation = __atomic_load_n(&mqtt_state->callback_generation, __ATOMIC_ACQUIRE);
    event.result = count == 1 && qos && qos[0] >= 0 && qos[0] <= 2 ? 0 : -1;
    return ml307y_post(mqtt_state, &event);
}

/*******************************************************************************
* Function Name  : ml307y_published_callback
* Description    : 捕获MQTT PUBACK用于传输结果
* Input          : client - 客户端；id - 包编号；dup - 重复标记
* Output         : 传输事件
* Return         : 投递状态
* Attention      : 不删除报警记录
*******************************************************************************/
static int ml307y_published_callback(cm_mqtt_client_t *client, unsigned short id, char dup)
{
    ml307y_mqtt_state_t *mqtt_state = ml307y_lookup(client);
    ml307y_mqtt_event_t event = {0};
    (void)dup;
    if (!mqtt_state)
    {
        return -1;
    }
    event.kind = ML307Y_PUBLISHED;
    event.id = id;
    event.generation = __atomic_load_n(&mqtt_state->callback_generation, __ATOMIC_ACQUIRE);
    return ml307y_post(mqtt_state, &event);
}

/*******************************************************************************
* Function Name  : ml307y_timeout_cb
* Description    : 捕获SDK报文超时
* Input          : client - 客户端；id - 包编号
* Output         : 后台超时事件
* Return         : 投递状态
* Attention      : 由后台按当前连接及包编号判断
*******************************************************************************/
static int ml307y_timeout_cb(cm_mqtt_client_t *client, unsigned short id)
{
    ml307y_mqtt_state_t *mqtt_state = ml307y_lookup(client);
    ml307y_mqtt_event_t event = {0};
    if (!mqtt_state)
    {
        return -1;
    }
    event.kind = ML307Y_TIMEOUT;
    event.id = id;
    event.generation = __atomic_load_n(&mqtt_state->callback_generation, __ATOMIC_ACQUIRE);
    return ml307y_post(mqtt_state, &event);
}

/*******************************************************************************
* Function Name  : ml307y_receive_callback
* Description    : 在SDK缓冲有效期内复制受限分段数据
* Input          : client/id/topic - 消息身份；total/length - 长度；payload - 内容
* Output         : 拥有独立缓冲的接收事件
* Return         : 0已排队；-1拒绝
* Attention      : 不解析未完整的铠湾回执
*******************************************************************************/
static int ml307y_receive_callback(cm_mqtt_client_t *client, unsigned short id, char *topic, int total,
                         int length, char *payload)
{
    ml307y_mqtt_state_t *mqtt_state = ml307y_lookup(client);
    ml307y_mqtt_event_t event = {0};
    size_t topic_length = 0;
    if (!mqtt_state)
    {
        return -1;
    }
    if (total <= 0 || length <= 0 || length > total || total > (int)KAIWAN_CLOUD_MAXIMUM_PAYLOAD_SIZE ||
        !payload || (topic && !kaiwan_cloud_text_length(topic, sizeof(event.topic), &topic_length)))
    {
        __atomic_store_n(&mqtt_state->callback_error, 1U, __ATOMIC_RELEASE);
        ml307y_signal(mqtt_state);
        return -1;
    }
    /* SDK 回调缓冲只在本次调用有效，分段必须复制后入队。 */
    event.payload = cm_malloc((size_t)length);
    if (!event.payload)
    {
        __atomic_store_n(&mqtt_state->callback_error, 1U, __ATOMIC_RELEASE);
        ml307y_signal(mqtt_state);
        return -1;
    }
    event.kind = ML307Y_RECEIVED;
    event.generation = __atomic_load_n(&mqtt_state->callback_generation, __ATOMIC_ACQUIRE);
    event.id = id;
    event.total = (size_t)total;
    event.length = (size_t)length;
    if (topic)
    {
        memcpy(event.topic, topic, topic_length + 1);
    }
    memcpy(event.payload, payload, (size_t)length);
    return ml307y_post(mqtt_state, &event);
}

/*******************************************************************************
* Function Name  : ml307y_transmit_done
* Description    : 完成一次传输回调并释放发送副本
* Input          : mqtt_state - 传输上下文；result - 传输结果
* Output         : 清除发送占用并通知调用方
* Return         : 无
* Attention      : 该结果不是平台业务确认
*******************************************************************************/
static void ml307y_transmit_done(ml307y_mqtt_state_t *mqtt_state, kaiwan_cloud_result_t result)
{
    uint32_t cookie = mqtt_state->cookie;
    if (!mqtt_state->publishing)
    {
        return;
    }
    mqtt_state->publishing = false;
    cm_free(mqtt_state->transmit_payload);
    mqtt_state->transmit_payload = NULL;
    if (mqtt_state->callbacks.on_publish_result)
    {
        mqtt_state->callbacks.on_publish_result(cookie, result, mqtt_state->callbacks.user);
    }
}

/*******************************************************************************
* Function Name  : ml307y_offline
* Description    : 撤销上线状态并终止本连接发送占用
* Input          : mqtt_state - 上下文
* Output         : 连接和组包状态
* Return         : 无
* Attention      : 未确认报警由产品保留重试
*******************************************************************************/
static void ml307y_offline(ml307y_mqtt_state_t *mqtt_state)
{
    bool was_online = mqtt_state->subscribed;
    mqtt_state->subscribed = false;
    mqtt_state->connecting = false;
    mqtt_state->subscription_id = 0;
    mqtt_receive_reset(&mqtt_state->receive, mqtt_state->generation);
    ml307y_transmit_done(mqtt_state, KAIWAN_CLOUD_ERROR_NETWORK);
    if (was_online && mqtt_state->callbacks.on_state_changed)
    {
        mqtt_state->callbacks.on_state_changed(false, mqtt_state->callbacks.user);
    }
}

/*******************************************************************************
* Function Name  : ml307y_start
* Description    : 保存配置并请求连接，SDK客户端在上下文寿命内复用
* Input          : user - 传输；config - 配置；callbacks - 业务回调
* Output         : 异步连接请求
* Return         : KAIWAN_CLOUD状态码
* Attention      : 目前QoS1；凭据不写入日志
*******************************************************************************/
static kaiwan_cloud_result_t ml307y_start(void *user, const kaiwan_cloud_config_t *config,
                                  const kaiwan_cloud_callbacks_t *callbacks)
{
    ml307y_mqtt_state_t *mqtt_state = user;
    int enabled;
    int channel;
    int ping_seconds;
    uint8_t yes = 1;
    uint8_t no = 0;
    uint8_t version = 255;
    const kaiwan_tls_config_t *tls;
    if (!mqtt_state || !callbacks || kaiwan_cloud_validate_config(config) != KAIWAN_CLOUD_OK || config->qos != 1 ||
        config->peer_receive_topic[0])
    {
        return KAIWAN_CLOUD_ERROR_CONFIG;
    }
    if (mqtt_state->wanted)
    {
        return KAIWAN_CLOUD_ERROR_STATE;
    }
    mqtt_state->config = *config;
    mqtt_state->callbacks = *callbacks;
    /* CONNECT 的 keepalive 与底包实际 PING 定时器必须使用同一周期。 */
    ping_seconds = (int)config->keepalive_seconds;
    if (cm_mqtt_client_set_opt(mqtt_state->client, CM_MQTT_OPT_PING_CYCLE, &ping_seconds) != 0)
    {
        return KAIWAN_CLOUD_ERROR_CONFIG;
    }
    enabled = config->use_tls ? 1 : 0;
    if (cm_mqtt_client_set_opt(mqtt_state->client, CM_MQTT_OPT_SSL_ENABLE, &enabled) != 0)
    {
        return KAIWAN_CLOUD_ERROR_CONFIG;
    }
    if (enabled)
    {
        tls = config->tls_config;
        if (!tls || tls->channel >= 6 || !tls->ca_file || !tls->ca_file[0])
        {
            return KAIWAN_CLOUD_ERROR_CONFIG;
        }
        channel = (int)tls->channel;
        if (cm_ssl_setopt(channel, CM_SSL_PARAM_VERIFY, &yes) != 0 ||
            cm_ssl_setopt(channel, CM_SSL_PARAM_VERSION, &version) != 0 ||
            cm_ssl_setopt(channel, CM_SSL_PARAM_IGNORE_STAMP, &no) != 0 ||
            cm_ssl_setopt(channel, CM_SSL_PARAM_IGNORE_VERIFY, &no) != 0 ||
            cm_ssl_setopt(channel, CM_SSL_PARAM_SNI, &yes) != 0 ||
            cm_ssl_setopt(channel, CM_SSL_PARAM_CA_CERT_FILENAME, (void *)tls->ca_file) != 0 ||
            cm_mqtt_client_set_opt(mqtt_state->client, CM_MQTT_OPT_SSL_ID, &channel) != 0)
        {
            return KAIWAN_CLOUD_ERROR_CONFIG;
        }
    }
    memset(&mqtt_state->options, 0, sizeof(mqtt_state->options));
    mqtt_state->options.hostname = mqtt_state->config.broker_host;
    mqtt_state->options.hostport = mqtt_state->config.broker_port;
    mqtt_state->options.clientid = mqtt_state->config.client_id;
    mqtt_state->options.username = mqtt_state->config.username;
    mqtt_state->options.password = mqtt_state->config.password;
    mqtt_state->options.keepalive = mqtt_state->config.keepalive_seconds;
    mqtt_state->options.clean_session = mqtt_state->config.clean_session;
    mqtt_state->configured = true;
    mqtt_state->wanted = true;
    mqtt_state->next_connect = mqtt_state->system->millis(mqtt_state->system->user);
    return KAIWAN_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : ml307y_online
* Description    : 查询完成订阅后的在线状态
* Input          : user - 传输上下文
* Output         : 无
* Return         : true允许业务发送
* Attention      : 连接成功但未收到SUBACK时为false
*******************************************************************************/
static bool ml307y_online(void *user)
{
    return ((ml307y_mqtt_state_t *)user)->subscribed;
}

/*******************************************************************************
* Function Name  : ml307y_publish
* Description    : 复制数据并提交唯一在途QoS1消息
* Input          : user - 传输；topic/payload/size - 报文；qos/retained - 选项；cookie - 关联
* Output         : 保存发送副本
* Return         : KAIWAN_CLOUD状态码
* Attention      : 业务回执由产品独立判断
*******************************************************************************/
static kaiwan_cloud_result_t ml307y_publish(void *user, const char *topic, const uint8_t *payload,
                                    size_t size, uint8_t qos, bool retained, uint32_t cookie)
{
    ml307y_mqtt_state_t *mqtt_state = user;
    size_t topic_length;
    int result;
    if (!mqtt_state->subscribed)
    {
        return KAIWAN_CLOUD_ERROR_STATE;
    }
    if (mqtt_state->publishing)
    {
        return KAIWAN_CLOUD_ERROR_QUEUE;
    }
    if (!payload || !size || size > KAIWAN_CLOUD_MAXIMUM_PAYLOAD_SIZE || qos != 1 ||
        !kaiwan_cloud_text_length(topic, sizeof(mqtt_state->transmit_topic), &topic_length) || !topic_length ||
        topic_length + size + 16 >= 4096)
    {
        return KAIWAN_CLOUD_ERROR_ARGUMENT;
    }
    mqtt_state->transmit_payload = cm_malloc(size);
    if (!mqtt_state->transmit_payload)
    {
        return KAIWAN_CLOUD_ERROR_MEMORY;
    }
    memcpy(mqtt_state->transmit_payload, payload, size);
    memcpy(mqtt_state->transmit_topic, topic, topic_length + 1);
    mqtt_state->cookie = cookie;
    mqtt_state->publishing = true;
    mqtt_state->transmit_started = mqtt_state->system->millis(mqtt_state->system->user);
    /* SDK 发送后递增编号，必须在提交前记录本次报文编号。 */
    result = cm_mqtt_client_get_msgid(mqtt_state->client);
    if (result <= 0)
    {
        ml307y_transmit_done(mqtt_state, KAIWAN_CLOUD_ERROR_MQTT);
        return KAIWAN_CLOUD_ERROR_MQTT;
    }
    mqtt_state->publish_id = (uint16_t)result;
    result = cm_mqtt_client_publish(mqtt_state->client, mqtt_state->transmit_topic, (const char *)mqtt_state->transmit_payload, (int)size,
                                    CM_MQTT_QOS_1 | (retained ? CM_MQTT_RETAIN_1 : 0));
    if (result < 0)
    {
        ml307y_transmit_done(mqtt_state, KAIWAN_CLOUD_ERROR_MQTT);
        return KAIWAN_CLOUD_ERROR_MQTT;
    }
    return KAIWAN_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : ml307y_poll
* Description    : 串行分派有限回调并推进连接订阅及超时
* Input          : user - 传输上下文；now - 单调毫秒
* Output         : 连接状态及用户回调
* Return         : 无
* Attention      : 仅后台调用；旧代数事件一律释放丢弃
*******************************************************************************/
static void ml307y_poll(void *user, uint32_t now)
{
    ml307y_mqtt_state_t *mqtt_state = user;
    ml307y_mqtt_event_t event;
    unsigned count;
    int result;
    const char *topic;
    char qos = 1;
    bool first_poll = !mqtt_state->polling_started;
    bool trace;
    (void)__atomic_exchange_n(&mqtt_state->event_pending, 0U, __ATOMIC_ACQ_REL);
    if (first_poll)
    {
        mqtt_state->polling_started = true;
        ml307y_mqtt_diagnostic(mqtt_state, "mqtt-poll-enter", 0);
    }
    if (__atomic_exchange_n(&mqtt_state->callback_error, 0U, __ATOMIC_ACQ_REL))
    {
        /* 回调队列出错时，先使旧事件代数失效，再请求异步断开。 */
        mqtt_state->generation = __atomic_add_fetch(&mqtt_state->callback_generation, 1U, __ATOMIC_ACQ_REL);
        mqtt_state->next_connect = now + mqtt_state->config.reconnect_minimum_ms;
        ml307y_offline(mqtt_state);
        (void)cm_mqtt_client_disconnect(mqtt_state->client);
        mqtt_state->system->fault("mqtt-callback-queue", KAIWAN_CLOUD_ERROR_QUEUE);
    }
    if (first_poll)
    {
        ml307y_mqtt_diagnostic(mqtt_state, "mqtt-poll-atomic-ok", 0);
    }
    for (count = 0; count < 8 && osMessageQueueGet(mqtt_state->events, &event, NULL, 0) == osOK; ++count)
    {
        if (event.kind == ML307Y_CONNECTION && event.result == CM_MQTT_CONN_STATE_SUCCESS)
        {
            /* 快照属于回调产生时的连接；即使随后断开，也记录这次实际连接。 */
            ml307y_report_local_address(&event);
        }
        if (event.generation != __atomic_load_n(&mqtt_state->callback_generation, __ATOMIC_ACQUIRE))
        {
            if (event.payload != NULL)
            {
                cm_free(event.payload);
            }
            continue;
        }
        if (event.kind == ML307Y_CONNECTION)
        {
            ml307y_mqtt_diagnostic(mqtt_state, "mqtt-connack", event.result);
            mqtt_state->generation = event.generation;
            ml307y_offline(mqtt_state);
            mqtt_state->next_connect = now + mqtt_state->config.reconnect_minimum_ms;
            if (mqtt_state->wanted && event.result == 0)
            {
                topic = mqtt_state->config.platform_down_topic;
                result = cm_mqtt_client_get_msgid(mqtt_state->client);
                if (result <= 0)
                {
                    ml307y_mqtt_diagnostic(mqtt_state, "mqtt-subscribe-id-error", result);
                    (void)cm_mqtt_client_disconnect(mqtt_state->client);
                    continue;
                }
                mqtt_state->subscription_id = (uint16_t)result;
                ml307y_mqtt_diagnostic(mqtt_state, "mqtt-subscribe-id", result);
                result = cm_mqtt_client_subscribe(mqtt_state->client, &topic, &qos, 1);
                ml307y_mqtt_diagnostic(mqtt_state, "mqtt-subscribe-result", result);
                if (result >= 0)
                {
                    mqtt_state->connecting = true;
                    mqtt_state->phase_started = now;
                }
                else
                {
                    (void)cm_mqtt_client_disconnect(mqtt_state->client);
                }
            }
        }
        else if (event.generation == mqtt_state->generation && mqtt_state->wanted)
        {
            if (event.kind == ML307Y_SUBSCRIBED)
            {
                ml307y_mqtt_diagnostic(mqtt_state, "mqtt-suback-id", event.id);
                ml307y_mqtt_diagnostic(mqtt_state, "mqtt-suback-result", event.result);
            }
            if (event.kind == ML307Y_SUBSCRIBED && mqtt_state->connecting && event.id == mqtt_state->subscription_id)
            {
                ml307y_mqtt_diagnostic(mqtt_state, "mqtt-suback", event.result);
                if (event.result == 0 && !mqtt_state->subscribed)
                {
                    mqtt_state->subscribed = true;
                    mqtt_state->connecting = false;
                    if (mqtt_state->callbacks.on_state_changed)
                    {
                        mqtt_state->callbacks.on_state_changed(true, mqtt_state->callbacks.user);
                    }
                }
                else if (event.result != 0)
                {
                    ml307y_offline(mqtt_state);
                    (void)cm_mqtt_client_disconnect(mqtt_state->client);
                }
            }
            else if ((event.kind == ML307Y_PUBLISHED || event.kind == ML307Y_TIMEOUT) && mqtt_state->publishing &&
                     event.id == mqtt_state->publish_id)
            {
                ml307y_transmit_done(mqtt_state, event.kind == ML307Y_PUBLISHED ? KAIWAN_CLOUD_OK : KAIWAN_CLOUD_ERROR_MQTT);
            }
            else if (event.kind == ML307Y_RECEIVED && mqtt_state->subscribed)
            {
                result = mqtt_receive_feed(&mqtt_state->receive, event.generation, event.id,
                                      event.topic[0] ? event.topic : NULL, event.total,
                                      event.payload, event.length);
                /* 只将完整组装的下行负载交给产品解析。 */
                if (result == MQTT_RECEIVE_COMPLETE && mqtt_state->callbacks.on_message)
                {
                    result = mqtt_state->callbacks.on_message(mqtt_state->receive.topic, strlen(mqtt_state->receive.topic),
                                                     mqtt_state->receive.payload, mqtt_state->receive.total, mqtt_state->callbacks.user);
                }
                if (result < 0 && mqtt_state->callbacks.on_receive_error)
                {
                    mqtt_state->callbacks.on_receive_error(KAIWAN_CLOUD_ERROR_MQTT, mqtt_state->callbacks.user);
                }
            }
        }
        if (event.payload != NULL)
        {
            cm_free(event.payload);
        }
    }
    if (mqtt_state->publishing && (uint32_t)(now - mqtt_state->transmit_started) >= mqtt_state->config.command_timeout_ms)
    {
        ml307y_transmit_done(mqtt_state, KAIWAN_CLOUD_ERROR_MQTT);
    }
    if (mqtt_state->connecting && (uint32_t)(now - mqtt_state->phase_started) >= mqtt_state->config.command_timeout_ms)
    {
        ml307y_mqtt_diagnostic(mqtt_state, "mqtt-phase-timeout", KAIWAN_CLOUD_ERROR_MQTT);
        ml307y_offline(mqtt_state);
        (void)cm_mqtt_client_disconnect(mqtt_state->client);
        mqtt_state->next_connect = now + mqtt_state->config.reconnect_minimum_ms;
    }
    if (mqtt_state->wanted && !mqtt_state->subscribed && !mqtt_state->connecting && (int32_t)(now - mqtt_state->next_connect) >= 0)
    {
        trace = first_poll || (int32_t)(now - mqtt_state->next_trace) >= 0;
        if (trace)
        {
            mqtt_state->next_trace = now + ML307Y_MQTT_TRACE_INTERVAL_MS;
            ml307y_mqtt_diagnostic(mqtt_state, "mqtt-state-query", 0);
        }
        result = cm_mqtt_client_get_state(mqtt_state->client);
        if (trace)
        {
            ml307y_mqtt_diagnostic(mqtt_state, "mqtt-state-result", result);
        }
        if (result != CM_MQTT_STATE_DISCONNECTED)
        {
            return;
        }
        mqtt_state->next_connect = now + mqtt_state->config.reconnect_minimum_ms;
        ml307y_mqtt_diagnostic(mqtt_state, "mqtt-pdp-query", 0);
        result = cm_modem_get_pdp_state(1);
        ml307y_mqtt_diagnostic(mqtt_state, "mqtt-pdp-result", result);
        if (result == 1)
        {
            ml307y_mqtt_diagnostic(mqtt_state, "mqtt-connect-enter", 0);
            result = cm_mqtt_client_connect(mqtt_state->client, &mqtt_state->options);
            ml307y_mqtt_diagnostic(mqtt_state, "mqtt-connect-result", result);
            if (result == 0)
            {
                mqtt_state->connecting = true;
                mqtt_state->phase_started = now;
            }
        }
    }
}

/*******************************************************************************
* Function Name  : ml307y_stop
* Description    : 请求停止并等待SDK断开及回调排空
* Input          : user - 传输上下文
* Output         : 断开状态
* Return         : true完全静止；false仍需poll
* Attention      : 不异步销毁客户端，避免回调使用已释放对象
*******************************************************************************/
static bool ml307y_stop(void *user)
{
    ml307y_mqtt_state_t *mqtt_state = user;
    uint32_t now = mqtt_state->system->millis(mqtt_state->system->user);
    bool first = mqtt_state->wanted;
    if (first)
    {
        mqtt_state->wanted = false;
        mqtt_state->generation = __atomic_add_fetch(&mqtt_state->callback_generation, 1U, __ATOMIC_ACQ_REL);
        ml307y_offline(mqtt_state);
        mqtt_state->next_stop = now;
    }
    if (first || (cm_mqtt_client_get_state(mqtt_state->client) != CM_MQTT_STATE_DISCONNECTED &&
                  (int32_t)(now - mqtt_state->next_stop) >= 0))
    {
        int result = cm_mqtt_client_disconnect(mqtt_state->client);
        mqtt_state->next_stop = now + 5000U;
        if (result < 0)
        {
            ml307y_mqtt_diagnostic(mqtt_state, "mqtt-stop-error", result);
        }
    }
    return cm_mqtt_client_get_state(mqtt_state->client) == CM_MQTT_STATE_DISCONNECTED &&
           osMessageQueueGetCount(mqtt_state->events) == 0 && !mqtt_state->publishing;
}

/*******************************************************************************
* Function Name  : ml307y_mqtt_create
* Description    : 为当前产品创建显式传输实例
* Input          : services - 产品服务
* Output         : services.transport
* Return         : true成功
* Attention      : 最多使用SDK允许的客户端数量，不连接任何服务器
*******************************************************************************/
bool ml307y_mqtt_create(product_services_t *services)
{
    ml307y_mqtt_state_t *mqtt_state;
    unsigned slot;
    for (slot = 0; slot < CM_MQTT_CLIENT_MAX && mqtt_clients[slot]; ++slot)
    {
    }
    if (slot == CM_MQTT_CLIENT_MAX)
    {
        return false;
    }
    mqtt_state = cm_calloc(1, sizeof(*mqtt_state));
    if (!mqtt_state)
    {
        return false;
    }
    mqtt_state->events = osMessageQueueNew(ML307Y_MQTT_EVENTS, sizeof(ml307y_mqtt_event_t), NULL);
    if (!mqtt_state->events)
    {
        cm_free(mqtt_state);
        return false;
    }
    mqtt_state->client = cm_mqtt_client_create();
    if (!mqtt_state->client)
    {
        osMessageQueueDelete(mqtt_state->events);
        cm_free(mqtt_state);
        return false;
    }
    mqtt_state->system = &services->system;
    mqtt_clients[slot] = mqtt_state;
    mqtt_state->sdk_callbacks.connack_cb = ml307y_connection_callback;
    mqtt_state->sdk_callbacks.suback_cb = ml307y_subscribed_callback;
    mqtt_state->sdk_callbacks.puback_cb = ml307y_published_callback;
    mqtt_state->sdk_callbacks.publish_cb = ml307y_receive_callback;
    mqtt_state->sdk_callbacks.timeout_cb = ml307y_timeout_cb;
    if (cm_mqtt_client_set_opt(mqtt_state->client, CM_MQTT_OPT_EVENT, &mqtt_state->sdk_callbacks) != 0)
    {
        /* 保留上下文寿命，回调注册失败则不启用产品。 */
        return false;
    }
    mqtt_state->interface.user = mqtt_state;
    mqtt_state->interface.start = ml307y_start;
    mqtt_state->interface.poll = ml307y_poll;
    mqtt_state->interface.online = ml307y_online;
    mqtt_state->interface.stop = ml307y_stop;
    mqtt_state->interface.publish = ml307y_publish;
    mqtt_state->interface.set_notify = ml307y_set_notify;
    mqtt_state->interface.next_wait = ml307y_next_wait;
    services->transport = &mqtt_state->interface;
    return true;
}
