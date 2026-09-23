/*------------------------------------------includes--------------------------------------------*/
#include "product_if.h"
#include "alarm_button/alarm_button.h"
#include "alarm_button/product_config.h"
#include "alarm_button/provisioning.h"
#include "mqtt/kw_cloud.h"
#include "kaiwan/kw_protocol.h"
#include "kaiwan/kw_session.h"
#include "alarm_button/handset_payload.h"
#include <stdio.h>
#include <string.h>
/*-------------------------------------------define---------------------------------------------*/
#define ALARM_CONTROL_COOKIE 0x10000U /* 区分注册/心跳发送结果与报警序号。 */
#define ALARM_WORKER_QUEUE 32U        /* 前台到后台的消息容量。 */
#define ALARM_UI_QUEUE 64U            /* 后台到前台的结果容量。 */

/*-------------------------------------------typedef---------------------------------------------*/
/* 前后台只通过消息传递请求和结果，避免跨任务直接改业务状态。 */
typedef enum
{
    AR_PRESS,     /* 前台确认按下，请后台保存。 */
    AR_WAKE,      /* 按键或心跳唤醒后台。 */
    AR_SAVED,     /* 持久化完成结果。 */
    AR_CONFIRMED, /* 平台业务回执完成。 */
    AR_IDLE,      /* 后台关闭连接并确认静止。 */
    AR_HEARTBEAT, /* 心跳业务回执完成。 */
    AR_ERROR      /* 后台故障通知。 */
} alarm_message_kind_t;

typedef struct
{
    alarm_message_kind_t kind;
    uint32_t request; /* 保存请求编号，防止旧结果影响新按键。 */
    uint32_t event_id; /* 成功持久化后的事件 ID。 */
    int result;        /* 保存或后台错误码。 */
    al_event_t event;  /* 入队时拷贝的按键事件快照。 */
} alarm_message_t;

typedef struct
{
    product_services_t *services;
    kw_transport_t *transport;
    void *worker_queue; /* 前台提交按键请求。 */
    void *ui_queue;     /* 后台回传保存、确认和故障。 */
    ab_app_t app;
    al_store_t store;
    al_reporter_t reporter;
    kh_identity_t identity;
    kw_cloud_config_t cloud;
    kw_protocol_config_t protocol;
    kw_protocol_workspace_t codec;
    al_event_t telemetry;
    uint32_t restored_max_id; /* 启动时已有事件的 ID 上界，用于区分历史补报。 */
    uint32_t last_heartbeat;
    uint32_t control_since;
    uint32_t control_retry;
    uint32_t identity_since;
    uint32_t last_worker_request;
    uint32_t next_ui_heartbeat;
    uint16_t control_sequence; /* 注册/心跳的独立序号。 */
    uint8_t control_command;   /* 当前等待回执的控制命令。 */
    bool registered;
    bool heartbeat_needed;
    bool control_waiting;
    bool config_ok;
    bool identity_ready;
    bool cloud_started;
    bool worker_ready;
    bool parked;
    bool cloud_stopping;
    int last_fault;
    uint8_t frame[KW_PROTOCOL_MAX_FRAME_SIZE];
    uint8_t body[KH_REGISTER_BYTES];
    char json[KW_PROTOCOL_MAX_JSON_SIZE];
} alarm_runtime_t;

/*-------------------------------------------variables-------------------------------------------*/
/* 编译配置提供的占位密钥；全零配置会阻止云连接。 */
static const uint8_t s_key[16] = AB_AES_KEY_BYTES;

/*-------------------------------------------function---------------------------------------------*/

/*******************************************************************************
* Function Name  : ar_now
* Description    : 通过注入接口读取单调毫秒
* Input          : r - 产品运行上下文
* Output         : 无
* Return         : 当前毫秒
* Attention      : 不依赖厂商头文件
*******************************************************************************/
static uint32_t ar_now(alarm_runtime_t *r)
{
    return r->services->system.millis(r->services->system.user);
}

/*******************************************************************************
* Function Name  : ar_ui_post
* Description    : 可靠通知前台持久化或回执结果
* Input          : r - 上下文；message - 消息
* Output         : 前台队列
* Return         : 无
* Attention      : 只由后台调用，前台不等待后台
*******************************************************************************/
static void ar_ui_post(alarm_runtime_t *r, const alarm_message_t *message)
{
    if (!r->services->system.queue_put(r->ui_queue, message, SYSTEM_WAIT_FOREVER))
    {
        r->services->system.fault("alarm-ui-queue", AL_ERR_FULL);
    }
}

/*******************************************************************************
* Function Name  : ar_fault
* Description    : 将后台错误作为明确产品故障通知前台
* Input          : user - 运行上下文；error - 错误码
* Output         : 错误消息及诊断
* Return         : 无
* Attention      : 抑制重复同码日志，不包含凭据
*******************************************************************************/
static void ar_fault(void *user, int error)
{
    alarm_runtime_t *r = user;
    alarm_message_t message = {0};
    if (r->last_fault == error)
    {
        return;
    }
    r->last_fault = error;
    message.kind = AR_ERROR;
    message.result = error;
    r->services->system.fault("alarm-worker", error);
    ar_ui_post(r, &message);
}

/*******************************************************************************
* Function Name  : ar_frame_send
* Description    : 复用现有CRC和AES编码，统一产品发送入口
* Input          : r - 运行上下文；command/sequence - 协议头；body/length - 数据体；cookie - 关联
* Output         : 编码后深拷贝进传输队列
* Return         : true - 已入传输队列；false - 编码或入队失败
* Attention      : 工作区仅由产品任务使用；每次使用新IV
*******************************************************************************/
static bool ar_frame_send(alarm_runtime_t *r, uint8_t command, uint16_t sequence,
                          const uint8_t *body, uint16_t length, uint32_t cookie)
{
    size_t frame_len;
    size_t json_len;
    uint8_t iv[16];
    if (!r->services->system.random(iv, sizeof(iv)) ||
        kw_protocol_build_frame(&r->protocol, sequence, command, body, length, r->frame,
                                sizeof(r->frame), &frame_len) != KW_OK)
    {
        return false;
    }
    if (kw_protocol_wrap_json(&r->protocol, &r->codec, r->frame, frame_len, iv, r->json,
                              sizeof(r->json), &json_len) != KW_OK)
    {
        return false;
    }
    return r->transport->publish(r->transport->user, r->cloud.platform_up_topic,
                                 (const uint8_t *)r->json, json_len, r->cloud.qos, false,
                                 cookie) == KW_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : ar_send_alarm
* Description    : 编码手报并区分恢复记录，未确认历史规则时保留而不猜测
* Input          : user - 运行上下文；event - 持久事件；sequence - 本次分配序号
* Output         : 构造手报事件帧并提交发送
* Return         : true - 进入传输队列；false - 不可发送
* Attention      : 恢复记录的补报规则未经确认时明确保留，不伪造时间
*******************************************************************************/
static bool ar_send_alarm(void *user, const al_event_t *event, uint16_t sequence)
{
    alarm_runtime_t *r = user;
    bool recovered = event->id <= r->restored_max_id;
    bool history = recovered && AB_REPLAY_MODE == 1;
    int length;
    if (recovered && AB_REPLAY_MODE == 0)
    {
        ar_fault(r, AL_ERR_CONFIG);
        return false;
    }
    /* 恢复事件只在已确定历史补报规则时标成历史帧。 */
    length = kh_event_payload(&r->identity, event, history, r->body, sizeof(r->body));
    if (length < 0)
    {
        ar_fault(r, length);
        return false;
    }
    return ar_frame_send(r, history ? KW_CMD_HISTORY : KW_CMD_EVENT, sequence, r->body,
                         (uint16_t)length, sequence);
}

/*******************************************************************************
* Function Name  : ar_control
* Description    : 每次连接注册并发心跳，独立序列不会误删报警队列
* Input          : r - 运行上下文；now - 单调毫秒
* Output         : 推进注册或22小时业务心跳状态
* Return         : 无
* Attention      : 独立保留控制序号，不能误当成报警确认
*******************************************************************************/
static void ar_control(alarm_runtime_t *r, uint32_t now)
{
    int length;
    uint8_t command;
    al_event_t telemetry;
    if (r->control_waiting)
    {
        if ((uint32_t)(now - r->control_since) < 10000)
        {
            return;
        }
        r->control_waiting = false;
        r->control_retry = now + 5000;
    }
    if ((int32_t)(now - r->control_retry) < 0)
    {
        return;
    }
    if (r->registered && !r->heartbeat_needed)
    {
        return;
    }
    telemetry = r->telemetry;
    telemetry.event_type = 1;
    command = r->registered ? KW_CMD_EVENT : KW_CMD_REGISTER;
    length = r->registered
                 ? kh_event_payload(&r->identity, &telemetry, false, r->body, sizeof(r->body))
                 : kh_registration_payload(&r->identity, &telemetry, r->body, sizeof(r->body));
    if (length < 0)
    {
        r->control_retry = now + 5000;
        ar_fault(r, length);
        return;
    }
    if (al_store_sequence(&r->store, &r->control_sequence) != AL_OK)
    {
        r->control_retry = now + 5000;
        ar_fault(r, AL_ERR_STORAGE);
        return;
    }
    r->control_command = command;
    r->control_since = now;
    r->control_waiting = ar_frame_send(r, command, r->control_sequence, r->body, (uint16_t)length,
                                       ALARM_CONTROL_COOKIE | r->control_sequence);
    if (!r->control_waiting)
    {
        r->control_retry = now + 5000;
    }
}

/*******************************************************************************
* Function Name  : ar_configure
* Description    : 加载明确产品配置，未配置或零密钥禁止连接
* Input          : r - 运行上下文
* Output         : 填充协议、传输与未知遥测策略
* Return         : true - 可以尝试接入；false - 配置缺失或未验证
* Attention      : 厂商码须32字节；不得记录或硬编码真实密钥
*******************************************************************************/
static bool ar_configure(alarm_runtime_t *r)
{
    unsigned i;
    uint8_t nonzero = 0;
    kw_cloud_config_init(&r->cloud);
    kw_protocol_config_init(&r->protocol);
    r->identity.firmware = AB_FIRMWARE_VERSION;
    r->identity.unknown_telemetry_verified = AB_UNKNOWN_TELEMETRY_VERIFIED != 0;
    r->identity.unknown_telemetry = AB_UNKNOWN_TELEMETRY_BYTE;
    r->protocol.protocol_version = 0x36;
    r->protocol.manufacturer_id = AB_MANUFACTURER_ID;
    r->protocol.aes_plain_mode =
        AB_AES_HEX_PLAINTEXT ? KW_AES_PLAIN_HEX_FRAME : KW_AES_PLAIN_BINARY_FRAME;
    r->protocol.crc_order = AB_CRC_LITTLE_ENDIAN ? KW_CRC_LITTLE_ENDIAN : KW_CRC_BIG_ENDIAN;
    memcpy(r->protocol.aes_key, s_key, 16);
    for (i = 0; i < 16; i++)
    {
        nonzero |= s_key[i];
    }
    if (sizeof(AB_FACTORY_CODE) != sizeof(r->protocol.factory_code) ||
        sizeof(AB_BROKER_HOST) > sizeof(r->cloud.broker_host) ||
        sizeof(AB_MQTT_USERNAME) > sizeof(r->cloud.username) ||
        sizeof(AB_MQTT_PASSWORD) > sizeof(r->cloud.password))
    {
        return false;
    }
    memcpy(r->protocol.factory_code, AB_FACTORY_CODE, sizeof(AB_FACTORY_CODE));
    memcpy(r->cloud.broker_host, AB_BROKER_HOST, sizeof(AB_BROKER_HOST));
    memcpy(r->cloud.username, AB_MQTT_USERNAME, sizeof(AB_MQTT_USERNAME));
    memcpy(r->cloud.password, AB_MQTT_PASSWORD, sizeof(AB_MQTT_PASSWORD));
    r->cloud.broker_port = AB_BROKER_PORT;
    r->cloud.use_tls = AB_USE_TLS != 0;
    r->cloud.tls_config = AB_TLS_CONFIG;
    r->cloud.reconnect_min_ms = 5000;
    r->cloud.reconnect_max_ms = 30000;
    return AB_CLOUD_ENABLED && AB_PROTOCOL_VERIFIED && nonzero && AB_MANUFACTURER_ID != 0 &&
           r->cloud.broker_host[0] && r->protocol.factory_code[0] &&
           (!r->cloud.use_tls || r->cloud.tls_config != NULL);
}

/*******************************************************************************
* Function Name  : ar_cloud_state
* Description    : 根据完整订阅状态重置平台注册
* Input          : online - 状态；user - 上下文
* Output         : 注册和控制回执状态
* Return         : 无
* Attention      : 传输线程通过poll在后台串行调用
*******************************************************************************/
static void ar_cloud_state(bool online, void *user)
{
    alarm_runtime_t *r = user;
    r->registered = false;
    r->control_waiting = false;
    r->heartbeat_needed = true;
    r->control_retry = ar_now(r);
    if (!online)
    {
        r->control_retry += AB_RETRY_MIN_MS;
    }
}

/*******************************************************************************
* Function Name  : ar_cloud_error
* Description    : 报告接收失败，不生成任何成功回执
* Input          : error - 传输错误；user - 上下文
* Output         : 错误诊断
* Return         : 无
* Attention      : 报警继续由持久队列重试
*******************************************************************************/
static void ar_cloud_error(kw_cloud_result_t error, void *user)
{
    ar_fault(user, error);
}

/*******************************************************************************
* Function Name  : ar_cloud_tx
* Description    : 仅处理传输失败；成功仍等待业务确认
* Input          : cookie - 协议关联；result - 传输结果；user - 上下文
* Output         : 重试或控制状态
* Return         : 无
* Attention      : PUBACK不删除报警、不结束提示
*******************************************************************************/
static void ar_cloud_tx(uint32_t cookie, kw_cloud_result_t result, void *user)
{
    alarm_runtime_t *r = user;
    uint32_t now = ar_now(r);
    if (result == KW_CLOUD_OK)
    {
        return;
    }
    if (cookie & ALARM_CONTROL_COOKIE)
    {
        if (r->control_waiting && (uint16_t)cookie == r->control_sequence)
        {
            r->control_waiting = false;
            r->control_retry = now + AB_RETRY_MIN_MS;
        }
    }
    else
    {
        al_reporter_send_failed(&r->reporter, (uint16_t)cookie, now);
    }
}

/*******************************************************************************
* Function Name  : ar_cloud_message
* Description    : 验证主题、解密、CRC、产品身份和业务回执关联
* Input          : topic/topic_len - 主题；payload/length - 完整报文；user - 上下文
* Output         : 后台队列及前台确认消息
* Return         : KW_CLOUD状态码
* Attention      : 只有业务成功且删除持久完成后才通知前台
*******************************************************************************/
static kw_cloud_result_t ar_cloud_message(const char *topic, size_t topic_len,
                                          const uint8_t *payload, size_t length, void *user)
{
    alarm_runtime_t *r = user;
    kw_frame_view_t frame;
    uint8_t response;
    uint32_t event_id = 0;
    uint32_t now = ar_now(r);
    int result;
    alarm_message_t message = {0};
    if (topic_len != strlen(r->cloud.platform_down_topic) ||
        memcmp(topic, r->cloud.platform_down_topic, topic_len) != 0 ||
        kw_session_decode(&r->protocol, &r->codec, (const char *)payload, length, r->frame,
                          sizeof(r->frame), &frame) != KW_OK ||
        kw_protocol_parse_server_response(&frame, &response) != KW_OK)
    {
        return KW_CLOUD_ERR_ARGUMENT;
    }
    /* 注册/心跳回执单独消费，不能进入报警队列确认路径。 */
    if (r->control_waiting && frame.sequence == r->control_sequence)
    {
        r->control_waiting = false;
        if (response == 0)
        {
            if (r->control_command == KW_CMD_REGISTER)
            {
                r->registered = true;
            }
            else
            {
                r->heartbeat_needed = false;
                r->last_heartbeat = now;
                message.kind = AR_HEARTBEAT;
                ar_ui_post(r, &message);
            }
        }
        else
        {
            r->control_retry = now + AB_RETRY_MIN_MS;
        }
        return KW_CLOUD_OK;
    }
    result = al_reporter_ack(&r->reporter, frame.sequence, response, now, &event_id);
    if (result == AL_OK)
    {
        message.kind = AR_CONFIRMED;
        message.event_id = event_id;
        ar_ui_post(r, &message);
    }
    else if (result != AL_ERR_STALE && result != AL_ERR_REJECTED)
    {
        ar_fault(r, result);
    }
    return KW_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : ar_collect
* Description    : 在后台更新身份和遥测，不阻塞声光
* Input          : r - 上下文；now - 调度时刻
* Output         : 最新有效快照
* Return         : 无
* Attention      : 未核验UTC和电量百分比保持无效
*******************************************************************************/
static void ar_collect(alarm_runtime_t *r, uint32_t now)
{
    device_info_t info;
    uint16_t millivolts;
    if (r->identity_ready && (uint32_t)(now - r->identity_since) < 60000U)
    {
        return;
    }
    if (!r->identity_ready && (uint32_t)(now - r->identity_since) < 5000U)
    {
        return;
    }
    r->identity_since = now;
    memset(&r->telemetry, 0, sizeof(r->telemetry));
    if (r->services->system.identity(&info))
    {
        memcpy(r->identity.imei, info.imei, sizeof(info.imei));
        memcpy(r->identity.imsi, info.imsi, sizeof(info.imsi));
        memcpy(r->identity.iccid, info.iccid, sizeof(info.iccid));
        r->identity_ready = true;
        if (info.csq_valid)
        {
            r->telemetry.csq = info.csq;
            r->telemetry.valid |= AL_TELEMETRY_CSQ;
        }
        if (AB_UTC_VERIFIED && info.utc_seconds)
        {
            r->telemetry.utc_seconds = info.utc_seconds;
            r->telemetry.valid |= AL_TIME_UTC;
        }
    }
    if (r->services->board.vbat && r->services->board.vbat(r->services->board.user, &millivolts))
    {
        r->telemetry.battery_mv = millivolts;
        r->telemetry.valid |= AL_TELEMETRY_VOLTAGE;
    }
    r->telemetry.uptime_ms = ar_now(r);
}

/*******************************************************************************
* Function Name  : ar_start_cloud
* Description    : 身份齐备后配置独立MQTT实例
* Input          : r - 产品上下文
* Output         : 连接请求
* Return         : true已请求
* Attention      : 无真实账号或协议未核验时不会连接
*******************************************************************************/
static bool ar_start_cloud(alarm_runtime_t *r)
{
    kw_cloud_callbacks_t callbacks = {ar_cloud_state, ar_cloud_message, ar_cloud_error, ar_cloud_tx,
                                      r};
    if (!r->config_ok || !r->identity_ready ||
        kw_cloud_make_platform_topics(&r->cloud, r->identity.imei) != KW_CLOUD_OK)
    {
        return false;
    }
    snprintf(r->cloud.client_id, sizeof(r->cloud.client_id), "KW-%s", r->identity.imei);
    return r->transport->start(r->transport->user, &r->cloud, &callbacks) == KW_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : ar_worker_message
* Description    : 处理有效按下并先持久化，再允许发送
* Input          : r - 后台上下文；message - 前台请求
* Output         : 持久队列及保存结果
* Return         : 无
* Attention      : 每次有效重按独立记录；不覆盖已有报警
*******************************************************************************/
static void ar_worker_message(alarm_runtime_t *r, const alarm_message_t *message)
{
    alarm_message_t done = {0};
    al_event_t event;
    int64_t event_utc;
    if (message->kind == AR_WAKE)
    {
        r->parked = false;
        r->heartbeat_needed = true;
        return;
    }
    if (message->kind != AR_PRESS)
    {
        return;
    }
    r->parked = false;
    r->last_worker_request = message->request;
    event = r->telemetry;
    event.id = 0;
    event.event_type = message->event.event_type;
    event.uptime_ms = message->event.uptime_ms;
    /* 以按键时刻与采样时刻的单调时间差换算 UTC；越界则丢弃无效时间。 */
    if (event.valid & AL_TIME_UTC)
    {
        event_utc =
            (int64_t)event.utc_seconds + (int32_t)(event.uptime_ms - r->telemetry.uptime_ms) / 1000;
        if (event_utc <= 0 || event_utc > UINT32_MAX)
        {
            event.valid &= (uint8_t)~AL_TIME_UTC;
            event.utc_seconds = 0;
        }
        else
        {
            event.utc_seconds = (uint32_t)event_utc;
        }
    }
    done.kind = AR_SAVED;
    done.request = message->request;
    done.result =
        r->worker_ready ? al_store_enqueue(&r->store, &event, &done.event_id) : AL_ERR_STORAGE;
    ar_ui_post(r, &done);
    if (done.result != AL_OK)
    {
        ar_fault(r, done.result);
    }
}

/*******************************************************************************
* Function Name  : ar_worker_initialize
* Description    : 恢复队列并绑定可靠上报器
* Input          : r - 后台上下文
* Output         : 队列、产品配置及恢复边界
* Return         : 无
* Attention      : 读取错误绝不重新初始化已有数据
*******************************************************************************/
static void ar_worker_initialize(alarm_runtime_t *r)
{
    int result = al_store_open(&r->store, AB_PRODUCT_ID, &r->services->storage);
    r->worker_ready = result == AL_OK;
    r->config_ok = ar_configure(r);
    r->heartbeat_needed = true;
    r->identity_since = ar_now(r) - 5000U;
    al_reporter_init(&r->reporter, &r->store, ar_send_alarm, r, AB_ACK_TIMEOUT_MS, AB_RETRY_MIN_MS,
                     AB_RETRY_MAX_MS);
    if (r->worker_ready)
    {
        /* 启动后新按键的 ID 必然更大，便于区分断电前遗留记录。 */
        r->restored_max_id = r->store.image.next_id - 1U;
        if (r->services->storage.warning)
        {
            result = r->services->storage.warning(r->services->storage.user);
            if (result)
            {
                r->services->system.fault("snapshot-recovered", result);
            }
        }
    }
    else
    {
        ar_fault(r, result);
    }
    if (!r->config_ok)
    {
        ar_fault(r, AL_ERR_CONFIG);
    }
}

/*******************************************************************************
* Function Name  : ar_worker_step
* Description    : 推进云会话、报警重试和静止握手
* Input          : r - 后台上下文；now - 单调毫秒
* Output         : 后台状态及静止消息
* Return         : 无
* Attention      : 提示结束不停止未确认事件重试
*******************************************************************************/
static void ar_worker_step(alarm_runtime_t *r, uint32_t now)
{
    alarm_message_t message = {0};
    if (r->parked || !r->worker_ready)
    {
        return;
    }
    ar_collect(r, now);
    now = ar_now(r);
    if (!r->cloud_started && r->config_ok && r->identity_ready)
    {
        r->cloud_started = ar_start_cloud(r);
    }
    if (!r->cloud_started)
    {
        return;
    }
    r->transport->poll(r->transport->user, now);
    if (r->cloud_stopping)
    {
        if (r->transport->stop(r->transport->user))
        {
            r->cloud_started = false;
            r->cloud_stopping = false;
            /* 连接真正停下且持久队列为空，才向前台报告可静止。 */
            if (al_store_pending(&r->store) == 0)
            {
                r->parked = true;
                message.kind = AR_IDLE;
                message.request = r->last_worker_request;
                ar_ui_post(r, &message);
            }
        }
        return;
    }
    if (r->transport->online(r->transport->user))
    {
        if (r->registered && (uint32_t)(now - r->last_heartbeat) >= AB_HEARTBEAT_MS)
        {
            r->heartbeat_needed = true;
        }
        ar_control(r, now);
        al_reporter_poll(&r->reporter, r->registered && !r->control_waiting, now);
    }
    else
    {
        al_reporter_poll(&r->reporter, false, now);
    }
    if (r->reporter.last_error != AL_OK && r->reporter.last_error != AL_ERR_REJECTED)
    {
        ar_fault(r, r->reporter.last_error);
    }
    if (r->registered && !r->heartbeat_needed && !r->control_waiting &&
        al_store_pending(&r->store) == 0)
    {
        r->cloud_stopping = true;
        (void)r->transport->stop(r->transport->user);
    }
}

/*******************************************************************************
* Function Name  : ar_worker_task
* Description    : 后台独占存储和云协议状态
* Input          : argument - 运行上下文
* Output         : 持久化、重试及回执通知
* Return         : 不返回
* Attention      : 网络及文件延迟不会阻塞前台任务
*******************************************************************************/
static void ar_worker_task(void *argument)
{
    alarm_runtime_t *r = argument;
    alarm_message_t message;
    unsigned drained;
    ar_worker_initialize(r);
    for (;;)
    {
        if (r->services->system.queue_get(r->worker_queue, &message,
                                          r->parked ? SYSTEM_WAIT_FOREVER : 20U))
        {
            ar_worker_message(r, &message);
        }
        for (drained = 0;
             drained < 7 && r->services->system.queue_get(r->worker_queue, &message, 0); ++drained)
        {
            ar_worker_message(r, &message);
        }
        ar_worker_step(r, ar_now(r));
    }
}

/*******************************************************************************
* Function Name  : ar_submit_event
* Description    : 只排队有效按键事件，不进行持久化
* Input          : user - 运行上下文；request - 请求号；event - 事件快照
* Output         : 后台队列
* Return         : AL_OK已排队；AL_ERR_FULL队列满
* Attention      : 排队成功不等于持久成功
*******************************************************************************/
static int ar_submit_event(void *user, uint32_t request, const al_event_t *event)
{
    alarm_runtime_t *r = user;
    alarm_message_t message = {0};
    message.kind = AR_PRESS;
    message.request = request;
    message.event = *event;
    return r->services->system.queue_put(r->worker_queue, &message, 0) ? AL_OK : AL_ERR_FULL;
}

/*******************************************************************************
* Function Name  : ar_ui_output
* Description    : 把统一声光状态交给唯一板级入口
* Input          : user - 上下文；led/buzzer - 状态
* Output         : 板输出
* Return         : true成功
* Attention      : 只有前台调用
*******************************************************************************/
static bool ar_ui_output(void *user, bool led, bool buzzer)
{
    alarm_runtime_t *r = user;
    return r->services->board.outputs(r->services->board.user, led, buzzer);
}

/*******************************************************************************
* Function Name  : ar_ui_fault
* Description    : 输出前台故障诊断
* Input          : user - 运行上下文；error - 错误码
* Output         : 诊断日志
* Return         : 无
* Attention      : 不向自身队列阻塞投递
*******************************************************************************/
static void ar_ui_fault(void *user, int error)
{
    alarm_runtime_t *r = user;
    r->services->system.fault("alarm-ui", error);
}

/*******************************************************************************
* Function Name  : ar_key_wakeup
* Description    : 从板中断唤醒前台
* Input          : user - 运行上下文
* Output         : 零等待前台消息
* Return         : 无
* Attention      : 仅唤醒，消抖仍由前台负责
*******************************************************************************/
static void ar_key_wakeup(void *user)
{
    alarm_runtime_t *r = user;
    alarm_message_t message = {0};
    message.kind = AR_WAKE;
    (void)r->services->system.queue_put(r->ui_queue, &message, 0);
}

/*******************************************************************************
* Function Name  : ar_ui_message
* Description    : 按事件身份消费后台结果
* Input          : r - 前台上下文；message - 结果；now - 毫秒
* Output         : 提示、保存数量及休眠条件
* Return         : 无
* Attention      : 旧静止消息不能许可新请求休眠
*******************************************************************************/
static void ar_ui_message(alarm_runtime_t *r, const alarm_message_t *message, uint32_t now)
{
    switch (message->kind)
    {
    case AR_SAVED:
        ab_saved(&r->app, message->request, message->event_id, message->result);
        break;
    case AR_CONFIRMED:
        ab_confirmed(&r->app, message->event_id);
        break;
    case AR_IDLE:
        /* 旧静止通知不能让新按键进入休眠。 */
        if (message->request == r->app.latest_request && !r->app.pending_saves)
        {
            r->app.background_idle = true;
        }
        break;
    case AR_HEARTBEAT:
        r->next_ui_heartbeat = now + AB_HEARTBEAT_MS;
        break;
    case AR_ERROR:
        r->app.last_error = message->result;
        r->app.background_idle = false;
        break;
    default:
        break;
    }
}

/*******************************************************************************
* Function Name  : ar_ui_task
* Description    : 独立采样按键并推进声光，不调用存储或网络
* Input          : argument - 运行上下文
* Output         : 报警提示及后台提交
* Return         : 不返回
* Attention      : 只在后台完全静止且唤醒已核验时释放工作锁
*******************************************************************************/
static void ar_ui_task(void *argument)
{
    alarm_runtime_t *r = argument;
    system_if_t *system = &r->services->system;
    alarm_message_t message;
    bool pressed;
    bool sleep_allowed;
    uint32_t now;
    uint32_t wait_ms;
    unsigned drained;
    r->next_ui_heartbeat = ar_now(r) + AB_HEARTBEAT_MS;
    for (;;)
    {
        now = ar_now(r);
        for (drained = 0; drained < 8 && system->queue_get(r->ui_queue, &message, 0); ++drained)
        {
            ar_ui_message(r, &message, now);
        }
        if (r->services->board.read_key(r->services->board.user, &pressed))
        {
            ab_poll(&r->app, pressed, now, NULL);
        }
        else
        {
            /* 读键失败沿用稳定状态，避免把硬件故障当作一次松手。 */
            r->app.last_error = AL_ERR_NOT_READY;
            r->app.key.candidate = r->app.key.stable;
            ab_poll(&r->app, r->app.key.stable, now, NULL);
        }
        if ((int32_t)(now - r->next_ui_heartbeat) >= 0)
        {
            memset(&message, 0, sizeof(message));
            message.kind = AR_WAKE;
            r->app.background_idle = false;
            if (system->queue_put(r->worker_queue, &message, 0))
            {
                r->next_ui_heartbeat = now + AB_HEARTBEAT_MS;
            }
            else
            {
                r->app.last_error = AL_ERR_FULL;
            }
        }
        /* 只有唤醒能力经过板级核验且业务已静止，才释放工作电源锁。 */
        sleep_allowed = r->services->board.wake_verified && r->services->board.set_wakeup &&
                        ab_can_sleep(&r->app);
        wait_ms = sleep_allowed ? (uint32_t)(r->next_ui_heartbeat - now) : 5U;
        system->power_hold(system->user, !sleep_allowed);
        if (system->queue_get(r->ui_queue, &message, wait_ms))
        {
            system->power_hold(system->user, true);
            ar_ui_message(r, &message, ar_now(r));
        }
        else
        {
            system->power_hold(system->user, true);
        }
    }
}

/*******************************************************************************
* Function Name  : alarm_product_start
* Description    : 创建一个报警产品的独立前后台上下文
* Input          : services - 已绑定服务
* Output         : 两个任务及独立队列
* Return         : 无
* Attention      : 必须先完成板级核验与底包身份检查
*******************************************************************************/
void alarm_product_start(product_services_t *services)
{
    alarm_runtime_t *r;
    ab_config_t config = ab_default_config();
    ab_io_t io;
    if (!services || !services->board.ready || !services->transport)
    {
        return;
    }
    r = services->system.allocate(sizeof(*r));
    if (!r)
    {
        services->system.fault("alarm-allocation", AL_ERR_NOT_READY);
        return;
    }
    memset(r, 0, sizeof(*r));
    r->services = services;
    r->transport = services->transport;
    r->worker_queue = services->system.queue_create(ALARM_WORKER_QUEUE, sizeof(alarm_message_t));
    r->ui_queue = services->system.queue_create(ALARM_UI_QUEUE, sizeof(alarm_message_t));
    io.user = r;
    io.output = ar_ui_output;
    io.fault = ar_ui_fault;
    io.submit_event = ar_submit_event;
    if (!r->worker_queue || !r->ui_queue || ab_init(&r->app, &config, &io, NULL, NULL) != AL_OK)
    {
        services->system.fault("alarm-initialize", AL_ERR_NOT_READY);
        return;
    }
    if (services->board.set_wakeup)
    {
        services->board.set_wakeup(services->board.user, ar_key_wakeup, r);
    }
    if (!services->system.thread_start("alarm-ui", ar_ui_task, r, 8192U, true))
    {
        services->system.fault("alarm-ui-start", AL_ERR_NOT_READY);
        return;
    }
    if (!services->system.thread_start("alarm-worker", ar_worker_task, r, 16384U, false))
    {
        ar_fault(r, AL_ERR_NOT_READY);
        return;
    }
}
