/*------------------------------------------includes--------------------------------------------*/
#include "test_support.h"
#include <stdarg.h>
#include "../../src/ml307y/mqtt_port.c"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
struct _cm_mqtt_client_t
{
    cm_mqtt_client_cb_t callbacks;
    int state;
    unsigned short id;
};

typedef struct
{
    unsigned capacity;
    unsigned bytes;
    unsigned count;
    uint8_t *data;
} mock_queue_t;

/*-------------------------------------------variables-------------------------------------------*/
static cm_mqtt_client_t client;
static unsigned short sent_id; /* SDK 先使用当前编号，再递增下一编号。 */
static uint32_t now_ms;
static unsigned received;
static unsigned transmit_done;
static unsigned faults;
static unsigned online_count;
static unsigned wake_notifications;
static int configured_ping_seconds;
static bool reject_ping_option;
static bool reject_disconnect;
static unsigned disconnect_calls;
static int local_address_result = 4;
static char local_address_text[46] = "10.37.151.65";
static unsigned local_address_queries;
static unsigned local_address_logs;
static char last_address_log[160];

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : project_mqtt_local_address
* Description    : 模拟底包查询的实际 MQTT 源地址
* Input          : sdk_client - SDK 客户端；address/capacity - 输出缓冲
* Output         : address - 当前查询结果的副本
* Return         : local_address_result
* Attention      : 可改变返回内容验证回调快照不会在后台被替换
*******************************************************************************/
int project_mqtt_local_address(void *sdk_client, char *address, size_t capacity)
{
    assert(sdk_client == &client && capacity >= sizeof(local_address_text));
    ++local_address_queries;
    if (client.state != CM_MQTT_STATE_CONNECTED)
    {
        address[0] = '\0';
        return -2;
    }
    memcpy(address, local_address_text, sizeof(local_address_text));
    return local_address_result;
}

/*******************************************************************************
* Function Name  : ml307y_uart_diag_printf
* Description    : 捕获统一串口入口的实际连接地址日志
* Input          : format - 格式；可变参数 - 日志内容
* Output         : last_address_log - 最近一行打印
* Return         : 格式化长度
* Attention      : 测试只验证文本和时机，不访问 UART 或真实网络
*******************************************************************************/
int ml307y_uart_diag_printf(const char *format, ...)
{
    va_list arguments;
    int length;
    va_start(arguments, format);
    length = vsnprintf(last_address_log, sizeof(last_address_log), format, arguments);
    va_end(arguments);
    ++local_address_logs;
    return length;
}

/*******************************************************************************
* Function Name  : notify_background
* Description    : 记录正常事件和队列溢出的唤醒通知
* Input          : argument - 计数器地址
* Output         : 唤醒次数
* Return         : 无
* Attention      : 通知只唤醒后台，不能替代业务回执
*******************************************************************************/
static void notify_background(void *argument)
{
    ++*(unsigned *)argument;
}
/*******************************************************************************
* Function Name  : mock_exchange
* Description    : 模拟原子交换的顺序语义
* Input          : p/value - 输入
* Output         : p
* Return         : 旧值
* Attention      : 仅单线程测试
*******************************************************************************/
uint32_t mock_exchange(uint32_t *p, uint32_t value)
{
    uint32_t previous = *p;
    *p = value;
    return previous;
}

/*******************************************************************************
* Function Name  : osMessageQueueNew
* Description    : 创建有界测试队列
* Input          : count/bytes/attr - 配置
* Output         : 堆缓冲
* Return         : 句柄
* Attention      : 不创建线程
*******************************************************************************/
osMessageQueueId_t osMessageQueueNew(uint32_t count, uint32_t bytes,
                                     const osMessageQueueAttr_t *attr)
{
    mock_queue_t *q = calloc(1, sizeof(*q));
    (void)attr;
    assert(q);
    q->capacity = count;
    q->bytes = bytes;
    q->data = calloc(count, bytes);
    assert(q->data);
    return q;
}

/*******************************************************************************
* Function Name  : osMessageQueuePut
* Description    : 模拟队列满时拒绝
* Input          : queue/message/priority/timeout - 请求
* Output         : 队列
* Return         : 0或负值
* Attention      : 深拷贝消息结构
*******************************************************************************/
osStatus_t osMessageQueuePut(osMessageQueueId_t queue, const void *message, uint8_t priority,
                             uint32_t timeout)
{
    mock_queue_t *q = queue;
    (void)priority;
    (void)timeout;
    if (q->count == q->capacity)
    {
        return -1;
    }
    memcpy(q->data + q->count++ * q->bytes, message, q->bytes);
    return osOK;
}

/*******************************************************************************
* Function Name  : osMessageQueueGet
* Description    : 按FIFO取出消息
* Input          : queue/message/priority/timeout - 参数
* Output         : message
* Return         : 状态
* Attention      : 无等待
*******************************************************************************/
osStatus_t osMessageQueueGet(osMessageQueueId_t queue, void *message, uint8_t *priority,
                             uint32_t timeout)
{
    mock_queue_t *q = queue;
    (void)priority;
    (void)timeout;
    if (!q->count)
    {
        return -1;
    }
    memcpy(message, q->data, q->bytes);
    --q->count;
    memmove(q->data, q->data + q->bytes, q->count * q->bytes);
    return osOK;
}

/*******************************************************************************
* Function Name  : osMessageQueueGetCount
* Description    : 查询测试队列长度
* Input          : queue - 队列
* Output         : 无
* Return         : 数量
* Attention      : 只读
*******************************************************************************/
uint32_t osMessageQueueGetCount(osMessageQueueId_t queue)
{
    return ((mock_queue_t *)queue)->count;
}

/*******************************************************************************
* Function Name  : osMessageQueueDelete
* Description    : 释放测试队列
* Input          : queue - 队列
* Output         : 释放缓冲
* Return         : 0
* Attention      : 仅测试
*******************************************************************************/
osStatus_t osMessageQueueDelete(osMessageQueueId_t queue)
{
    free(((mock_queue_t *)queue)->data);
    free(queue);
    return 0;
}

/*******************************************************************************
* Function Name  : cm_mqtt_client_create
* Description    : 返回当前测试客户端
* Input          : 无
* Output         : 客户端
* Return         : 句柄
* Attention      : 不联网
*******************************************************************************/
cm_mqtt_client_t *cm_mqtt_client_create(void)
{
    return &client;
}

/*******************************************************************************
* Function Name  : cm_mqtt_client_set_opt
* Description    : 保存真实SDK声明对应的回调
* Input          : c/option/param - 参数
* Output         : 回调副本
* Return         : 0
* Attention      : 其他选项不访问真实SDK
*******************************************************************************/
int cm_mqtt_client_set_opt(cm_mqtt_client_t *c, cm_mqtt_option_e option, void *param)
{
    if (option == CM_MQTT_OPT_EVENT)
    {
        c->callbacks = *(cm_mqtt_client_cb_t *)param;
    }
    if (option == CM_MQTT_OPT_PING_CYCLE)
    {
        if (reject_ping_option)
        {
            return -1;
        }
        configured_ping_seconds = *(int *)param;
    }
    return 0;
}

/*******************************************************************************
* Function Name  : cm_mqtt_client_connect
* Description    : 模拟异步连接请求
* Input          : c/option - 客户端配置
* Output         : 连接中
* Return         : 0
* Attention      : 回调由测试显式触发
*******************************************************************************/
int cm_mqtt_client_connect(cm_mqtt_client_t *c, cm_mqtt_connect_options_t *option)
{
    assert(configured_ping_seconds > 0);
    assert(configured_ping_seconds == option->keepalive);
    c->state = CM_MQTT_STATE_CONNECTING;
    return 0;
}

/*******************************************************************************
* Function Name  : cm_mqtt_client_subscribe
* Description    : 分配订阅ID但不立即确认
* Input          : c/topic/qos/count - 参数
* Output         : 递增包ID
* Return         : 1
* Attention      : 必须等待SUBACK
*******************************************************************************/
int cm_mqtt_client_subscribe(cm_mqtt_client_t *c, const char *topic[], const char qos[], int count)
{
    assert(count == 1 && qos[0] == 1 && topic[0]);
    sent_id = c->id;
    c->id = c->id == 65535 ? 1 : c->id + 1;
    return 1;
}

/*******************************************************************************
* Function Name  : cm_mqtt_client_publish
* Description    : 模拟异步发布成功入队
* Input          : c/topic/payload/size/flags - 请求
* Output         : 包ID
* Return         : 报文长度
* Attention      : 不触发业务确认
*******************************************************************************/
int cm_mqtt_client_publish(cm_mqtt_client_t *c, const char *topic, const char *payload, int size,
                           char flags)
{
    assert(topic && payload && (flags & CM_MQTT_QOS_1));
    sent_id = c->id;
    c->id = c->id == 65535 ? 1 : c->id + 1;
    return size;
}

/*******************************************************************************
* Function Name  : cm_mqtt_client_disconnect
* Description    : 模拟异步停止但延迟断开回调
* Input          : c - 客户端
* Output         : 状态
* Return         : 0
* Attention      : 测试旧排队事件不能再上线
*******************************************************************************/
int cm_mqtt_client_disconnect(cm_mqtt_client_t *c)
{
    ++disconnect_calls;
    if (reject_disconnect)
    {
        return -1;
    }
    c->state = CM_MQTT_STATE_DISCONNECTED;
    return 0;
}

/*******************************************************************************
* Function Name  : cm_mqtt_client_get_msgid
* Description    : 查询下一次发送使用的编号
* Input          : c - 客户端
* Output         : 无
* Return         : 编号
* Attention      : 只读
*******************************************************************************/
int cm_mqtt_client_get_msgid(cm_mqtt_client_t *c)
{
    return c->id;
}

/*******************************************************************************
* Function Name  : cm_mqtt_client_get_state
* Description    : 查询连接状态
* Input          : c - 客户端
* Output         : 无
* Return         : 状态
* Attention      : 只读
*******************************************************************************/
int cm_mqtt_client_get_state(cm_mqtt_client_t *c)
{
    return c->state;
}

/*******************************************************************************
* Function Name  : cm_modem_get_pdp_state
* Description    : 提供已激活PDP
* Input          : cid - PDP号
* Output         : 无
* Return         : 1
* Attention      : 没有实际蜂窝网络
*******************************************************************************/
int32_t cm_modem_get_pdp_state(uint16_t cid)
{
    assert(cid == 1);
    return 1;
}

/*******************************************************************************
* Function Name  : cm_ssl_setopt
* Description    : 模拟TLS配置接口
* Input          : id/type/value - 配置
* Output         : 无
* Return         : 0
* Attention      : 不证明真实TLS握手
*******************************************************************************/
int32_t cm_ssl_setopt(int32_t id, cm_ssl_param_type_e type, void *value)
{
    (void)id;
    (void)type;
    (void)value;
    return 0;
}

/*******************************************************************************
* Function Name  : clock_now
* Description    : 提供测试时间
* Input          : user - 保留
* Output         : 无
* Return         : 毫秒
* Attention      : 可控单调时间
*******************************************************************************/
static uint32_t clock_now(void *user)
{
    (void)user;
    return now_ms;
}

/*******************************************************************************
* Function Name  : fault
* Description    : 记录错误回调
* Input          : module/error - 错误
* Output         : 计数
* Return         : 无
* Attention      : 不吞掉失败断言
*******************************************************************************/
static void fault(const char *module, int error)
{
    (void)module;
    (void)error;
    ++faults;
}

/*******************************************************************************
* Function Name  : state_changed
* Description    : 记录订阅完成后的在线通知
* Input          : online/user - 状态
* Output         : 计数
* Return         : 无
* Attention      : CONNACK不能触发成功
*******************************************************************************/
static void state_changed(bool online, void *user)
{
    (void)user;
    if (online)
    {
        ++online_count;
    }
}

/*******************************************************************************
* Function Name  : message
* Description    : 只接收完整并已复制的消息
* Input          : topic/topic_length/payload/size/user - 数据
* Output         : 接收计数
* Return         : 成功
* Attention      : 断言SDK临时缓冲没有被继续引用
*******************************************************************************/
static kaiwan_cloud_result_t message(const char *topic, size_t topic_length, const uint8_t *payload,
                                 size_t size, void *user)
{
    (void)user;
    assert(topic_length == 4 && memcmp(topic, "down", 4) == 0);
    assert(size == 6 && memcmp(payload, "abcdef", 6) == 0);
    ++received;
    return KAIWAN_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : transmit_result
* Description    : 捕获传输回执
* Input          : cookie/result/user - 结果
* Output         : 计数
* Return         : 无
* Attention      : 不修改业务存储
*******************************************************************************/
static void transmit_result(uint32_t cookie, kaiwan_cloud_result_t result, void *user)
{
    (void)user;
    assert(cookie == 42);
    if (result == KAIWAN_CLOUD_OK)
    {
        ++transmit_done;
    }
}

/*******************************************************************************
* Function Name  : connect_ready
* Description    : 完成一次新的连接和订阅
* Input          : m - 上下文
* Output         : 在线
* Return         : 无
* Attention      : 模拟SDK顺序回调
*******************************************************************************/
static void connect_ready(ml307y_mqtt_state_t *m)
{
    int qos = 1;
    unsigned logs_before = local_address_logs;
    unsigned queries_before = local_address_queries;
    char expected_address[46];
    memcpy(expected_address, local_address_text, sizeof(expected_address));
    client.state = CM_MQTT_STATE_CONNECTED;
    assert(client.callbacks.connack_cb(&client, 0, 0) == 0);
    assert(local_address_queries == queries_before + 1U);
    assert(local_address_logs == logs_before);
    strcpy(local_address_text, "changed-after-callback");
    ml307y_poll(m, now_ms);
    assert(local_address_logs == logs_before + 1U);
    assert(strstr(last_address_log, "[project][mqtt-local-ip]"));
    if (local_address_result == 4 || local_address_result == 6)
    {
        assert(strstr(last_address_log, local_address_result == 4 ? "family=IPv4" : "family=IPv6"));
        assert(strstr(last_address_log, expected_address));
        assert(!strstr(last_address_log, "changed-after-callback"));
    }
    else
    {
        assert(strstr(last_address_log, "family=unavailable local=unavailable result=-3"));
        assert(!strstr(last_address_log, expected_address));
    }
    memcpy(local_address_text, expected_address, sizeof(local_address_text));
    assert(!ml307y_online(m));
    assert(client.callbacks.suback_cb(&client, sent_id, 1, &qos) == 0);
    ml307y_poll(m, now_ms);
    assert(ml307y_online(m));
    assert(local_address_logs == logs_before + 1U);
}

/*******************************************************************************
* Function Name  : main
* Description    : 验证真实CM接口适配的异步状态与错误分支
* Input          : 无
* Output         : 断言
* Return         : 0成功
* Attention      : 队列模型不是线程竞争或实网验证
*******************************************************************************/
int main(void)
{
    product_services_t services = {0};
    kaiwan_cloud_config_t config;
    kaiwan_cloud_callbacks_t callbacks = {state_changed, message, NULL, transmit_result, NULL};
    ml307y_mqtt_state_t *m;
    char first[] = "abc";
    char last[] = "def";
    unsigned i;
    unsigned logs_before;
    int qos = 1;
    client.id = 1;
    services.system.millis = clock_now;
    services.system.fault = fault;
    assert(ml307y_mqtt_create(&services));
    m = ((kaiwan_transport_t *)services.transport)->user;
    assert(m->interface.set_notify && m->interface.next_wait);
    m->interface.set_notify(m, notify_background, &wake_notifications);
    kaiwan_cloud_config_init(&config);
    strcpy(config.local_imei, "123456789012345");
    strcpy(config.broker_host, "test.invalid");
    strcpy(config.client_id, "test-client");
    strcpy(config.platform_up_topic, "up");
    strcpy(config.platform_down_topic, "down");
    reject_ping_option = true;
    assert(ml307y_start(m, &config, &callbacks) == KAIWAN_CLOUD_ERROR_CONFIG);
    assert(!m->wanted && !m->configured);
    reject_ping_option = false;
    assert(ml307y_start(m, &config, &callbacks) == KAIWAN_CLOUD_OK);
    assert(configured_ping_seconds == config.keepalive_seconds);
    ml307y_poll(m, 0);
    assert(!ml307y_online(m));
    connect_ready(m);
    assert(online_count == 1);
    assert(wake_notifications == 2U);
    assert(m->interface.next_wait(m, now_ms) == UINT32_MAX);
    i = local_address_logs;
    ml307y_poll(m, now_ms);
    assert(local_address_logs == i);
    assert(client.callbacks.publish_cb(&client, 5, "down", 6, 3, first) == 0);
    first[0] = 'X';
    assert(m->interface.next_wait(m, now_ms) == 0);
    ml307y_poll(m, 0);
    assert(received == 0);
    assert(client.callbacks.publish_cb(&client, 5, NULL, 6, 3, last) == 0);
    ml307y_poll(m, 0);
    assert(received == 1);
    assert(ml307y_publish(m, "up", (const uint8_t *)"alarm", 5, 1, false, 42) == KAIWAN_CLOUD_OK);
    assert(transmit_done == 0);
    assert(m->interface.next_wait(m, now_ms) == config.command_timeout_ms);
    assert(client.callbacks.puback_cb(&client, sent_id, 0) == 0);
    ml307y_poll(m, 0);
    assert(transmit_done == 1);

    /* Old queued publish must be discarded across disconnect/reconnect generations. */
    assert(client.callbacks.publish_cb(&client, 6, "down", 6, 6, "abcdef") == 0);
    client.callbacks.connack_cb(&client, 0, CM_MQTT_CONN_STATE_NET_ERR);
    client.callbacks.connack_cb(&client, 0, 0);
    ml307y_poll(m, 0);
    assert(received == 1 && !ml307y_online(m));
    client.callbacks.suback_cb(&client, sent_id, 1, &qos);
    ml307y_poll(m, 0);
    assert(ml307y_online(m));
    /* Queue overflow invalidates all pending events, including a delayed SUBACK. */
    for (i = 0; i < ML307Y_MQTT_EVENTS; ++i)
    {
        assert(client.callbacks.suback_cb(&client, sent_id, 1, &qos) == 0);
    }
    assert(client.callbacks.suback_cb(&client, sent_id, 1, &qos) == -1);
    i = local_address_logs;
    assert(m->interface.next_wait(m, now_ms) == 0);
    ml307y_poll(m, 0);
    assert(faults > 0);
    assert(!ml307y_online(m));
    assert(local_address_logs == i);
    while (osMessageQueueGetCount(m->events))
    {
        ml307y_poll(m, now_ms);
    }
    now_ms = 5000;
    connect_ready(m);
    assert(ml307y_publish(m, "up", (const uint8_t *)"alarm", 5, 1, false, 42) == KAIWAN_CLOUD_OK);
    now_ms += config.command_timeout_ms;
    ml307y_poll(m, now_ms);
    assert(!m->publishing && transmit_done == 1);
    client.callbacks.puback_cb(&client, sent_id, 0);
    ml307y_poll(m, now_ms);
    assert(transmit_done == 1);
    assert(ml307y_stop(m));
    assert(!ml307y_online(m));
    assert(ml307y_start(m, &config, &callbacks) == KAIWAN_CLOUD_OK);
    ml307y_poll(m, now_ms);
    assert(m->connecting);
    now_ms += config.command_timeout_ms;
    ml307y_poll(m, now_ms);
    assert(!m->connecting && !ml307y_online(m));
    connect_ready(m);
    reject_disconnect = true;
    i = disconnect_calls;
    assert(!ml307y_stop(m));
    assert(!m->wanted && !ml307y_online(m));
    reject_disconnect = false;
    now_ms += 5000U;
    assert(ml307y_stop(m));
    assert(disconnect_calls == i + 2U);
    i = online_count;
    logs_before = local_address_logs;
    /* 停止以后到达的旧 CONNACK/SUBACK 不得重新订阅上线。 */
    assert(client.callbacks.connack_cb(&client, 0, 0) == 0);
    ml307y_poll(m, now_ms);
    assert(!m->wanted && !ml307y_online(m) && online_count == i);
    assert(local_address_logs == logs_before + 1U);
    assert(strstr(last_address_log, "family=unavailable local=unavailable result=-2"));
    /* IPv6 与查询失败均应继续原订阅流程；失败不能沿用上次地址。 */
    assert(ml307y_start(m, &config, &callbacks) == KAIWAN_CLOUD_OK);
    local_address_result = 6;
    strcpy(local_address_text, "240E:87C:887:F8B5::1");
    connect_ready(m);
    /* 成功连接紧接着断开时，保留快照打印，但不把旧事件用于上线。 */
    logs_before = local_address_logs;
    assert(client.callbacks.connack_cb(&client, 0, CM_MQTT_CONN_STATE_SUCCESS) == 0);
    assert(local_address_logs == logs_before);
    assert(client.callbacks.connack_cb(&client, 0, CM_MQTT_CONN_STATE_NET_ERR) == 0);
    ml307y_poll(m, now_ms);
    assert(local_address_logs == logs_before + 1U && !ml307y_online(m));
    assert(strstr(last_address_log, "family=IPv6 local=240E:87C:887:F8B5::1"));
    i = local_address_queries;
    assert(client.callbacks.connack_cb(&client, 0, CM_MQTT_CONN_STATE_NET_ERR) == 0);
    ml307y_poll(m, now_ms);
    assert(local_address_queries == i);
    local_address_result = -3;
    connect_ready(m);
    assert(ml307y_stop(m));
    puts("CM MQTT: SUBACK gate, fragments, deep copy, PUBACK, generation and overflow OK");
    return 0;
}
