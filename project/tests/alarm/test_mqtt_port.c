/*------------------------------------------includes--------------------------------------------*/
#include "test_support.h"
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
static uint32_t now_ms;
static unsigned received;
static unsigned tx_done;
static unsigned faults;
static unsigned online_count;

/*-------------------------------------------function---------------------------------------------*/
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
    (void)option;
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
    ++c->id;
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
    ++c->id;
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
    c->state = CM_MQTT_STATE_DISCONNECTED;
    return 0;
}

/*******************************************************************************
* Function Name  : cm_mqtt_client_get_msgid
* Description    : 查询最新请求编号
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
* Input          : topic/topic_len/payload/size/user - 数据
* Output         : 接收计数
* Return         : 成功
* Attention      : 断言SDK临时缓冲没有被继续引用
*******************************************************************************/
static kw_cloud_result_t message(const char *topic, size_t topic_len, const uint8_t *payload,
                                 size_t size, void *user)
{
    (void)user;
    assert(topic_len == 4 && memcmp(topic, "down", 4) == 0);
    assert(size == 6 && memcmp(payload, "abcdef", 6) == 0);
    ++received;
    return KW_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : tx_result
* Description    : 捕获传输回执
* Input          : cookie/result/user - 结果
* Output         : 计数
* Return         : 无
* Attention      : 不修改业务存储
*******************************************************************************/
static void tx_result(uint32_t cookie, kw_cloud_result_t result, void *user)
{
    (void)user;
    assert(cookie == 42);
    if (result == KW_CLOUD_OK)
    {
        ++tx_done;
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
static void connect_ready(ml_mqtt_t *m)
{
    int qos = 1;
    client.state = CM_MQTT_STATE_CONNECTED;
    assert(client.callbacks.connack_cb(&client, 0, 0) == 0);
    ml_poll(m, now_ms);
    assert(!ml_online(m));
    assert(client.callbacks.suback_cb(&client, client.id, 1, &qos) == 0);
    ml_poll(m, now_ms);
    assert(ml_online(m));
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
    kw_cloud_config_t config;
    kw_cloud_callbacks_t callbacks = {state_changed, message, NULL, tx_result, NULL};
    ml_mqtt_t *m;
    char first[] = "abc";
    char last[] = "def";
    unsigned i;
    int qos = 1;
    services.system.millis = clock_now;
    services.system.fault = fault;
    assert(ml_mqtt_create(&services));
    m = ((kw_transport_t *)services.transport)->user;
    kw_cloud_config_init(&config);
    strcpy(config.local_imei, "123456789012345");
    strcpy(config.broker_host, "test.invalid");
    strcpy(config.client_id, "test-client");
    strcpy(config.platform_up_topic, "up");
    strcpy(config.platform_down_topic, "down");
    assert(ml_start(m, &config, &callbacks) == KW_CLOUD_OK);
    ml_poll(m, 0);
    assert(!ml_online(m));
    connect_ready(m);
    assert(online_count == 1);
    assert(client.callbacks.publish_cb(&client, 5, "down", 6, 3, first) == 0);
    first[0] = 'X';
    ml_poll(m, 0);
    assert(received == 0);
    assert(client.callbacks.publish_cb(&client, 5, NULL, 6, 3, last) == 0);
    ml_poll(m, 0);
    assert(received == 1);
    assert(ml_publish(m, "up", (const uint8_t *)"alarm", 5, 1, false, 42) == KW_CLOUD_OK);
    assert(tx_done == 0);
    assert(client.callbacks.puback_cb(&client, client.id, 0) == 0);
    ml_poll(m, 0);
    assert(tx_done == 1);

    /* Old queued publish must be discarded across disconnect/reconnect generations. */
    assert(client.callbacks.publish_cb(&client, 6, "down", 6, 6, "abcdef") == 0);
    client.callbacks.connack_cb(&client, 0, CM_MQTT_CONN_STATE_NET_ERR);
    client.callbacks.connack_cb(&client, 0, 0);
    ml_poll(m, 0);
    assert(received == 1 && !ml_online(m));
    client.callbacks.suback_cb(&client, client.id, 1, &qos);
    ml_poll(m, 0);
    assert(ml_online(m));
    /* Queue overflow invalidates all pending events, including a delayed SUBACK. */
    for (i = 0; i < ML_MQTT_EVENTS; ++i)
    {
        assert(client.callbacks.suback_cb(&client, client.id, 1, &qos) == 0);
    }
    assert(client.callbacks.suback_cb(&client, client.id, 1, &qos) == -1);
    ml_poll(m, 0);
    assert(faults > 0);
    assert(!ml_online(m));
    while (osMessageQueueGetCount(m->events))
    {
        ml_poll(m, now_ms);
    }
    now_ms = 5000;
    connect_ready(m);
    assert(ml_publish(m, "up", (const uint8_t *)"alarm", 5, 1, false, 42) == KW_CLOUD_OK);
    now_ms += config.command_timeout_ms;
    ml_poll(m, now_ms);
    assert(!m->publishing && tx_done == 1);
    client.callbacks.puback_cb(&client, client.id, 0);
    ml_poll(m, now_ms);
    assert(tx_done == 1);
    assert(ml_stop(m));
    assert(!ml_online(m));
    assert(ml_start(m, &config, &callbacks) == KW_CLOUD_OK);
    ml_poll(m, now_ms);
    assert(m->connecting);
    now_ms += config.command_timeout_ms;
    ml_poll(m, now_ms);
    assert(!m->connecting && !ml_online(m));
    puts("CM MQTT: SUBACK gate, fragments, deep copy, PUBACK, generation and overflow OK");
    return 0;
}
