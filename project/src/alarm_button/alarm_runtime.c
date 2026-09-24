/*------------------------------------------includes--------------------------------------------*/
#include "product_interface.h"
#include "alarm_button/alarm_button.h"
#include "alarm_button/product_config.h"
#include "alarm_button/provisioning.h"
#include "mqtt/kaiwan_cloud.h"
#include "kaiwan/kaiwan_protocol.h"
#include "kaiwan/kaiwan_session.h"
#include "alarm_button/handset_payload.h"
#include <stdio.h>
#include <string.h>
/*-------------------------------------------define---------------------------------------------*/
#define ALARM_CONTROL_COOKIE 0x10000U /* 区分注册/心跳发送结果与报警序号。 */
#define ALARM_BACKGROUND_QUEUE_CAPACITY 32U        /* 前台到后台的消息容量。 */
#define ALARM_FRONT_QUEUE_CAPACITY 64U            /* 后台到前台的结果容量。 */

/*-------------------------------------------typedef---------------------------------------------*/
/* 前后台只通过消息传递请求和结果，避免跨任务直接改业务状态。 */
typedef enum
{
    ALARM_MSG_SAVE_EVENT,          /* 前台确认按下，请后台保存。 */
    ALARM_MSG_WAKE,                /* 按键或心跳唤醒后台。 */
    ALARM_MSG_SAVE_RESULT,         /* 持久化完成结果。 */
    ALARM_MSG_EVENT_CONFIRMED,     /* 平台业务回执完成。 */
    ALARM_MSG_BACKGROUND_IDLE,     /* 后台关闭连接并确认静止。 */
    ALARM_MSG_HEARTBEAT_CONFIRMED, /* 心跳业务回执完成。 */
    ALARM_MSG_BACKGROUND_ERROR    /* 后台故障通知。 */
} alarm_message_kind_t;

typedef struct
{
    alarm_message_kind_t kind;
    uint32_t request; /* 保存请求编号，防止旧结果影响新按键。 */
    uint32_t event_id; /* 成功持久化后的事件 ID。 */
    int result;        /* 保存或后台错误码。 */
    alarm_event_t event;  /* 入队时拷贝的按键事件快照。 */
} alarm_message_t;

typedef struct
{
    product_services_t *services;
    kaiwan_transport_t *transport;
    void *background_queue; /* 前台提交按键请求。 */
    void *front_queue;     /* 后台回传保存、确认和故障。 */
    alarm_button_state_t button_state;
    alarm_event_store_t store;
    alarm_event_reporter_t reporter;
    kaiwan_handset_identity_t identity;
    kaiwan_cloud_config_t cloud;
    kaiwan_protocol_config_t protocol;
    kaiwan_protocol_workspace_t codec;
    alarm_event_t telemetry;
    uint32_t restored_maximum_id; /* 启动时已有事件的 ID 上界，用于区分历史补报。 */
    uint32_t last_heartbeat;
    uint32_t control_sent_at_ms;
    uint32_t next_control_retry_ms;
    uint32_t identity_since;
    uint32_t last_handled_request_id;
    uint32_t next_heartbeat_ms;
    uint16_t control_sequence; /* 注册/心跳的独立序号。 */
    uint8_t control_command;   /* 当前等待回执的控制命令。 */
    bool platform_registered;
    bool heartbeat_needed;
    bool waiting_control_reply;
    bool cloud_config_valid;
    bool identity_ready;
    bool cloud_started;
    bool storage_ready;
    bool startup_ready; /* 两个任务均创建成功后才允许执行产品业务。 */
    bool startup_failed; /* 部分任务已创建但启动失败时，使其长期阻塞。 */
    bool background_waiting;
    bool cloud_stopping;
    int last_fault;
    uint8_t frame[KAIWAN_PROTOCOL_MAXIMUM_FRAME_SIZE];
    uint8_t body[KAIWAN_HANDSET_REGISTER_BYTES];
    char json[KAIWAN_PROTOCOL_MAXIMUM_JSON_SIZE];
} alarm_runtime_t;

/*-------------------------------------------variables-------------------------------------------*/
/* 编译配置提供的占位密钥；全零配置会阻止云连接。 */
static const uint8_t alarm_aes_key[16] = ALARM_BUTTON_AES_KEY_BYTES;

/*-------------------------------------------function---------------------------------------------*/

/*******************************************************************************
* Function Name  : alarm_now_ms
* Description    : 通过注入接口读取单调毫秒
* Input          : runtime - 产品运行上下文
* Output         : 无
* Return         : 当前毫秒
* Attention      : 不依赖厂商头文件
*******************************************************************************/
static uint32_t alarm_now_ms(alarm_runtime_t *runtime)
{
    return runtime->services->system.millis(runtime->services->system.user);
}

/*******************************************************************************
* Function Name  : alarm_send_result_to_front
* Description    : 可靠通知前台持久化或回执结果
* Input          : runtime - 上下文；message - 消息
* Output         : 前台队列
* Return         : 无
* Attention      : 只由后台调用，前台不等待后台
*******************************************************************************/
static void alarm_send_result_to_front(alarm_runtime_t *runtime, const alarm_message_t *message)
{
    if (!runtime->services->system.queue_put(runtime->front_queue, message, SYSTEM_WAIT_FOREVER))
    {
        runtime->services->system.fault("alarm-ui-queue", ALARM_ERROR_FULL);
    }
}

/*******************************************************************************
* Function Name  : alarm_report_error
* Description    : 将后台错误作为明确产品故障通知前台
* Input          : user - 运行上下文；error - 错误码
* Output         : 错误消息及诊断
* Return         : 无
* Attention      : 抑制重复同码日志，不包含凭据
*******************************************************************************/
static void alarm_report_error(void *user, int error)
{
    alarm_runtime_t *runtime = user;
    alarm_message_t message = {0};
    if (runtime->last_fault == error)
    {
        return;
    }
    runtime->last_fault = error;
    message.kind = ALARM_MSG_BACKGROUND_ERROR;
    message.result = error;
    runtime->services->system.fault("alarm-worker", error);
    alarm_send_result_to_front(runtime, &message);
}

/*******************************************************************************
* Function Name  : alarm_publish_frame
* Description    : 复用现有CRC和AES编码，统一产品发送入口
* Input          : runtime - 运行上下文；command/sequence - 协议头；body/length - 数据体；cookie - 关联
* Output         : 编码后深拷贝进传输队列
* Return         : true - 已入传输队列；false - 编码或入队失败
* Attention      : 工作区仅由产品任务使用；每次使用新IV
*******************************************************************************/
static bool alarm_publish_frame(alarm_runtime_t *runtime, uint8_t command, uint16_t sequence,
                          const uint8_t *body, uint16_t length, uint32_t cookie)
{
    size_t frame_length;
    size_t json_length;
    uint8_t iv[16];
    if (!runtime->services->system.random(iv, sizeof(iv)) ||
        kaiwan_protocol_build_frame(&runtime->protocol, sequence, command, body, length, runtime->frame,
                                sizeof(runtime->frame), &frame_length) != KAIWAN_OK)
    {
        return false;
    }
    if (kaiwan_protocol_wrap_json(&runtime->protocol, &runtime->codec, runtime->frame, frame_length, iv, runtime->json,
                              sizeof(runtime->json), &json_length) != KAIWAN_OK)
    {
        return false;
    }
    return runtime->transport->publish(runtime->transport->user, runtime->cloud.platform_up_topic,
                                 (const uint8_t *)runtime->json, json_length, runtime->cloud.qos, false,
                                 cookie) == KAIWAN_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : alarm_publish_event
* Description    : 编码手报并区分恢复记录，未确认历史规则时保留而不猜测
* Input          : user - 运行上下文；event - 持久事件；sequence - 本次分配序号
* Output         : 构造手报事件帧并提交发送
* Return         : true - 进入传输队列；false - 不可发送
* Attention      : 恢复记录的补报规则未经确认时明确保留，不伪造时间
*******************************************************************************/
static bool alarm_publish_event(void *user, const alarm_event_t *event, uint16_t sequence)
{
    alarm_runtime_t *runtime = user;
    bool recovered = event->id <= runtime->restored_maximum_id;
    bool history = recovered && ALARM_BUTTON_REPLAY_MODE == 1;
    int length;
    if (recovered && ALARM_BUTTON_REPLAY_MODE == 0)
    {
        alarm_report_error(runtime, ALARM_ERROR_CONFIG);
        return false;
    }
    /* 恢复事件只在已确定历史补报规则时标成历史帧。 */
    length = kaiwan_handset_event_payload(&runtime->identity, event, history, runtime->body, sizeof(runtime->body));
    if (length < 0)
    {
        alarm_report_error(runtime, length);
        return false;
    }
    return alarm_publish_frame(runtime, history ? KAIWAN_COMMAND_HISTORY : KAIWAN_COMMAND_EVENT, sequence, runtime->body,
                         (uint16_t)length, sequence);
}

/*******************************************************************************
* Function Name  : alarm_send_registration_or_heartbeat
* Description    : 每次连接注册并发心跳，独立序列不会误删报警队列
* Input          : runtime - 运行上下文；now - 单调毫秒
* Output         : 推进注册或22小时业务心跳状态
* Return         : 无
* Attention      : 独立保留控制序号，不能误当成报警确认
*******************************************************************************/
static void alarm_send_registration_or_heartbeat(alarm_runtime_t *runtime, uint32_t now)
{
    int length;
    uint8_t command;
    alarm_event_t telemetry;
    if (runtime->waiting_control_reply)
    {
        if ((uint32_t)(now - runtime->control_sent_at_ms) < 10000)
        {
            return;
        }
        runtime->waiting_control_reply = false;
        runtime->next_control_retry_ms = now + 5000;
    }
    if ((int32_t)(now - runtime->next_control_retry_ms) < 0)
    {
        return;
    }
    if (runtime->platform_registered && !runtime->heartbeat_needed)
    {
        return;
    }
    telemetry = runtime->telemetry;
    telemetry.event_type = 1;
    command = runtime->platform_registered ? KAIWAN_COMMAND_EVENT : KAIWAN_COMMAND_REGISTER;
    length = runtime->platform_registered
                 ? kaiwan_handset_event_payload(&runtime->identity, &telemetry, false, runtime->body, sizeof(runtime->body))
                 : kaiwan_handset_registration_payload(&runtime->identity, &telemetry, runtime->body, sizeof(runtime->body));
    if (length < 0)
    {
        runtime->next_control_retry_ms = now + 5000;
        alarm_report_error(runtime, length);
        return;
    }
    if (alarm_store_sequence(&runtime->store, &runtime->control_sequence) != ALARM_OK)
    {
        runtime->next_control_retry_ms = now + 5000;
        alarm_report_error(runtime, ALARM_ERROR_STORAGE);
        return;
    }
    runtime->control_command = command;
    runtime->control_sent_at_ms = now;
    runtime->waiting_control_reply = alarm_publish_frame(runtime, command,
        runtime->control_sequence, runtime->body, (uint16_t)length,
        ALARM_CONTROL_COOKIE | runtime->control_sequence);
    if (!runtime->waiting_control_reply)
    {
        runtime->next_control_retry_ms = now + 5000;
    }
}

/*******************************************************************************
* Function Name  : alarm_load_cloud_config
* Description    : 加载明确产品配置，未配置或零密钥禁止连接
* Input          : runtime - 运行上下文
* Output         : 填充协议、传输与未知遥测策略
* Return         : true - 可以尝试接入；false - 配置缺失或未验证
* Attention      : 厂商码须32字节；不得记录或硬编码真实密钥
*******************************************************************************/
static bool alarm_load_cloud_config(alarm_runtime_t *runtime)
{
    unsigned index;
    uint8_t nonzero = 0;
    kaiwan_cloud_config_init(&runtime->cloud);
    kaiwan_protocol_config_init(&runtime->protocol);
    runtime->identity.firmware = ALARM_BUTTON_FIRMWARE_VERSION;
    runtime->identity.unknown_telemetry_verified = ALARM_BUTTON_UNKNOWN_TELEMETRY_VERIFIED != 0;
    runtime->identity.unknown_telemetry = ALARM_BUTTON_UNKNOWN_TELEMETRY_BYTE;
    runtime->protocol.protocol_version = 0x36;
    runtime->protocol.manufacturer_id = ALARM_BUTTON_MANUFACTURER_ID;
    runtime->protocol.aes_plain_mode =
        ALARM_BUTTON_AES_HEX_PLAINTEXT ? KAIWAN_AES_PLAIN_HEX_FRAME : KAIWAN_AES_PLAIN_BINARY_FRAME;
    runtime->protocol.crc_order = ALARM_BUTTON_CRC_LITTLE_ENDIAN ? KAIWAN_CRC_LITTLE_ENDIAN : KAIWAN_CRC_BIG_ENDIAN;
    memcpy(runtime->protocol.aes_key, alarm_aes_key, 16);
    for (index = 0; index < 16; index++)
    {
        nonzero |= alarm_aes_key[index];
    }
    if (sizeof(ALARM_BUTTON_FACTORY_CODE) != sizeof(runtime->protocol.factory_code) ||
        sizeof(ALARM_BUTTON_BROKER_HOST) > sizeof(runtime->cloud.broker_host) ||
        sizeof(ALARM_BUTTON_MQTT_USERNAME) > sizeof(runtime->cloud.username) ||
        sizeof(ALARM_BUTTON_MQTT_PASSWORD) > sizeof(runtime->cloud.password))
    {
        return false;
    }
    memcpy(runtime->protocol.factory_code, ALARM_BUTTON_FACTORY_CODE, sizeof(ALARM_BUTTON_FACTORY_CODE));
    memcpy(runtime->cloud.broker_host, ALARM_BUTTON_BROKER_HOST, sizeof(ALARM_BUTTON_BROKER_HOST));
    memcpy(runtime->cloud.username, ALARM_BUTTON_MQTT_USERNAME, sizeof(ALARM_BUTTON_MQTT_USERNAME));
    memcpy(runtime->cloud.password, ALARM_BUTTON_MQTT_PASSWORD, sizeof(ALARM_BUTTON_MQTT_PASSWORD));
    runtime->cloud.broker_port = ALARM_BUTTON_BROKER_PORT;
    runtime->cloud.use_tls = ALARM_BUTTON_USE_TLS != 0;
    runtime->cloud.tls_config = ALARM_BUTTON_TLS_CONFIG;
    runtime->cloud.reconnect_minimum_ms = 5000;
    runtime->cloud.reconnect_maximum_ms = 30000;
    return ALARM_BUTTON_CLOUD_ENABLED && ALARM_BUTTON_PROTOCOL_VERIFIED && nonzero && ALARM_BUTTON_MANUFACTURER_ID != 0 &&
           runtime->cloud.broker_host[0] && runtime->protocol.factory_code[0] &&
           (!runtime->cloud.use_tls || runtime->cloud.tls_config != NULL);
}

/*******************************************************************************
* Function Name  : alarm_on_cloud_state
* Description    : 根据完整订阅状态重置平台注册
* Input          : online - 状态；user - 上下文
* Output         : 注册和控制回执状态
* Return         : 无
* Attention      : 传输线程通过poll在后台串行调用
*******************************************************************************/
static void alarm_on_cloud_state(bool online, void *user)
{
    alarm_runtime_t *runtime = user;
    runtime->platform_registered = false;
    runtime->waiting_control_reply = false;
    runtime->heartbeat_needed = true;
    runtime->next_control_retry_ms = alarm_now_ms(runtime);
    if (!online)
    {
        runtime->next_control_retry_ms += ALARM_BUTTON_RETRY_MINIMUM_MS;
    }
}

/*******************************************************************************
* Function Name  : alarm_on_cloud_error
* Description    : 报告接收失败，不生成任何成功回执
* Input          : error - 传输错误；user - 上下文
* Output         : 错误诊断
* Return         : 无
* Attention      : 报警继续由持久队列重试
*******************************************************************************/
static void alarm_on_cloud_error(kaiwan_cloud_result_t error, void *user)
{
    alarm_report_error(user, error);
}

/*******************************************************************************
* Function Name  : alarm_on_publish_result
* Description    : 仅处理传输失败；成功仍等待业务确认
* Input          : cookie - 协议关联；result - 传输结果；user - 上下文
* Output         : 重试或控制状态
* Return         : 无
* Attention      : PUBACK不删除报警、不结束提示
*******************************************************************************/
static void alarm_on_publish_result(uint32_t cookie, kaiwan_cloud_result_t result, void *user)
{
    alarm_runtime_t *runtime = user;
    uint32_t now = alarm_now_ms(runtime);
    if (result == KAIWAN_CLOUD_OK)
    {
        return;
    }
    if (cookie & ALARM_CONTROL_COOKIE)
    {
        if (runtime->waiting_control_reply && (uint16_t)cookie == runtime->control_sequence)
        {
            runtime->waiting_control_reply = false;
            runtime->next_control_retry_ms = now + ALARM_BUTTON_RETRY_MINIMUM_MS;
        }
    }
    else
    {
        alarm_reporter_send_failed(&runtime->reporter, (uint16_t)cookie, now);
    }
}

/*******************************************************************************
* Function Name  : alarm_on_cloud_message
* Description    : 验证主题、解密、CRC、产品身份和业务回执关联
* Input          : topic/topic_length - 主题；payload/length - 完整报文；user - 上下文
* Output         : 后台队列及前台确认消息
* Return         : KAIWAN_CLOUD状态码
* Attention      : 只有业务成功且删除持久完成后才通知前台
*******************************************************************************/
static kaiwan_cloud_result_t alarm_on_cloud_message(const char *topic, size_t topic_length,
                                          const uint8_t *payload, size_t length, void *user)
{
    alarm_runtime_t *runtime = user;
    kaiwan_frame_view_t frame;
    uint8_t response;
    uint32_t event_id = 0;
    uint32_t now = alarm_now_ms(runtime);
    int result;
    alarm_message_t message = {0};
    if (topic_length != strlen(runtime->cloud.platform_down_topic) ||
        memcmp(topic, runtime->cloud.platform_down_topic, topic_length) != 0 ||
        kaiwan_session_decode(&runtime->protocol, &runtime->codec, (const char *)payload, length, runtime->frame,
                          sizeof(runtime->frame), &frame) != KAIWAN_OK ||
        kaiwan_protocol_parse_server_response(&frame, &response) != KAIWAN_OK)
    {
        return KAIWAN_CLOUD_ERROR_ARGUMENT;
    }
    /* 注册/心跳回执单独消费，不能进入报警队列确认路径。 */
    if (runtime->waiting_control_reply && frame.sequence == runtime->control_sequence)
    {
        runtime->waiting_control_reply = false;
        if (response == 0)
        {
            if (runtime->control_command == KAIWAN_COMMAND_REGISTER)
            {
                runtime->platform_registered = true;
            }
            else
            {
                runtime->heartbeat_needed = false;
                runtime->last_heartbeat = now;
                message.kind = ALARM_MSG_HEARTBEAT_CONFIRMED;
                alarm_send_result_to_front(runtime, &message);
            }
        }
        else
        {
            runtime->next_control_retry_ms = now + ALARM_BUTTON_RETRY_MINIMUM_MS;
        }
        return KAIWAN_CLOUD_OK;
    }
    result = alarm_reporter_on_platform_confirmation(&runtime->reporter, frame.sequence, response, now, &event_id);
    if (result == ALARM_OK)
    {
        message.kind = ALARM_MSG_EVENT_CONFIRMED;
        message.event_id = event_id;
        alarm_send_result_to_front(runtime, &message);
    }
    else if (result != ALARM_ERROR_STALE && result != ALARM_ERROR_REJECTED)
    {
        alarm_report_error(runtime, result);
    }
    return KAIWAN_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : alarm_collect_device_info
* Description    : 在后台更新身份和遥测，不阻塞声光
* Input          : runtime - 上下文；now - 调度时刻
* Output         : 最新有效快照
* Return         : 无
* Attention      : 未核验UTC和电量百分比保持无效
*******************************************************************************/
static void alarm_collect_device_info(alarm_runtime_t *runtime, uint32_t now)
{
    device_info_t info;
    uint16_t millivolts;
    if (runtime->identity_ready && (uint32_t)(now - runtime->identity_since) < 60000U)
    {
        return;
    }
    if (!runtime->identity_ready && (uint32_t)(now - runtime->identity_since) < 5000U)
    {
        return;
    }
    runtime->identity_since = now;
    memset(&runtime->telemetry, 0, sizeof(runtime->telemetry));
    if (runtime->services->system.identity(&info))
    {
        memcpy(runtime->identity.imei, info.imei, sizeof(info.imei));
        memcpy(runtime->identity.imsi, info.imsi, sizeof(info.imsi));
        memcpy(runtime->identity.iccid, info.iccid, sizeof(info.iccid));
        runtime->identity_ready = true;
        if (info.csq_valid)
        {
            runtime->telemetry.csq = info.csq;
            runtime->telemetry.valid |= ALARM_TELEMETRY_CSQ;
        }
        if (ALARM_BUTTON_UTC_VERIFIED && info.utc_seconds)
        {
            runtime->telemetry.utc_seconds = info.utc_seconds;
            runtime->telemetry.valid |= ALARM_TIME_UTC;
        }
    }
    if (runtime->services->board.battery_voltage && runtime->services->board.battery_voltage(runtime->services->board.user, &millivolts))
    {
        runtime->telemetry.battery_mv = millivolts;
        runtime->telemetry.valid |= ALARM_TELEMETRY_VOLTAGE;
    }
    runtime->telemetry.uptime_ms = alarm_now_ms(runtime);
}

/*******************************************************************************
* Function Name  : alarm_start_cloud
* Description    : 身份齐备后配置独立MQTT实例
* Input          : runtime - 产品上下文
* Output         : 连接请求
* Return         : true已请求
* Attention      : 无真实账号或协议未核验时不会连接
*******************************************************************************/
static bool alarm_start_cloud(alarm_runtime_t *runtime)
{
    kaiwan_cloud_callbacks_t callbacks = {alarm_on_cloud_state, alarm_on_cloud_message,
                                      alarm_on_cloud_error, alarm_on_publish_result,
                                      runtime};
    if (!runtime->cloud_config_valid || !runtime->identity_ready ||
        kaiwan_cloud_make_platform_topics(&runtime->cloud, runtime->identity.imei) != KAIWAN_CLOUD_OK)
    {
        return false;
    }
    snprintf(runtime->cloud.client_id, sizeof(runtime->cloud.client_id), "KW-%s", runtime->identity.imei);
    return runtime->transport->start(runtime->transport->user, &runtime->cloud, &callbacks) == KAIWAN_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : alarm_handle_front_request
* Description    : 处理有效按下并先持久化，再允许发送
* Input          : runtime - 后台上下文；message - 前台请求
* Output         : 持久队列及保存结果
* Return         : 无
* Attention      : 每次有效重按独立记录；不覆盖已有报警
*******************************************************************************/
static void alarm_handle_front_request(alarm_runtime_t *runtime, const alarm_message_t *message)
{
    alarm_message_t done = {0};
    alarm_event_t event;
    int64_t event_utc;
    if (message->kind == ALARM_MSG_WAKE)
    {
        runtime->background_waiting = false;
        runtime->heartbeat_needed = true;
        return;
    }
    if (message->kind != ALARM_MSG_SAVE_EVENT)
    {
        return;
    }
    runtime->background_waiting = false;
    runtime->last_handled_request_id = message->request;
    event = runtime->telemetry;
    event.id = 0;
    event.event_type = message->event.event_type;
    event.uptime_ms = message->event.uptime_ms;
    /* 以按键时刻与采样时刻的单调时间差换算 UTC；越界则丢弃无效时间。 */
    if (event.valid & ALARM_TIME_UTC)
    {
        event_utc =
            (int64_t)event.utc_seconds + (int32_t)(event.uptime_ms - runtime->telemetry.uptime_ms) / 1000;
        if (event_utc <= 0 || event_utc > UINT32_MAX)
        {
            event.valid &= (uint8_t)~ALARM_TIME_UTC;
            event.utc_seconds = 0;
        }
        else
        {
            event.utc_seconds = (uint32_t)event_utc;
        }
    }
    done.kind = ALARM_MSG_SAVE_RESULT;
    done.request = message->request;
    done.result =
        runtime->storage_ready ? alarm_store_enqueue(&runtime->store, &event, &done.event_id) : ALARM_ERROR_STORAGE;
    alarm_send_result_to_front(runtime, &done);
    if (done.result != ALARM_OK)
    {
        alarm_report_error(runtime, done.result);
    }
}

/*******************************************************************************
* Function Name  : alarm_init_background
* Description    : 恢复队列并绑定可靠上报器
* Input          : runtime - 后台上下文
* Output         : 队列、产品配置及恢复边界
* Return         : 无
* Attention      : 读取错误绝不重新初始化已有数据
*******************************************************************************/
static void alarm_init_background(alarm_runtime_t *runtime)
{
    int result = alarm_store_open(&runtime->store, ALARM_BUTTON_PRODUCT_ID, &runtime->services->storage);
    runtime->storage_ready = result == ALARM_OK;
    runtime->cloud_config_valid = alarm_load_cloud_config(runtime);
    runtime->heartbeat_needed = true;
    runtime->identity_since = alarm_now_ms(runtime) - 5000U;
    alarm_reporter_init(&runtime->reporter, &runtime->store, alarm_publish_event, runtime,
                     ALARM_BUTTON_CONFIRMATION_TIMEOUT_MS, ALARM_BUTTON_RETRY_MINIMUM_MS,
                     ALARM_BUTTON_RETRY_MAXIMUM_MS);
    if (runtime->storage_ready)
    {
        /* 启动后新按键的 ID 必然更大，便于区分断电前遗留记录。 */
        runtime->restored_maximum_id = runtime->store.image.next_id - 1U;
        if (runtime->services->storage.warning)
        {
            result = runtime->services->storage.warning(runtime->services->storage.user);
            if (result)
            {
                runtime->services->system.fault("snapshot-recovered", result);
            }
        }
    }
    else
    {
        alarm_report_error(runtime, result);
    }
    if (!runtime->cloud_config_valid)
    {
        alarm_report_error(runtime, ALARM_ERROR_CONFIG);
    }
}

/*******************************************************************************
* Function Name  : alarm_process_background
* Description    : 推进云会话、报警重试和静止握手
* Input          : runtime - 后台上下文；now - 单调毫秒
* Output         : 后台状态及静止消息
* Return         : 无
* Attention      : 提示结束不停止未确认事件重试
*******************************************************************************/
static void alarm_process_background(alarm_runtime_t *runtime, uint32_t now)
{
    alarm_message_t message = {0};
    if (runtime->background_waiting || !runtime->storage_ready)
    {
        return;
    }
    alarm_collect_device_info(runtime, now);
    now = alarm_now_ms(runtime);
    if (!runtime->cloud_started && runtime->cloud_config_valid && runtime->identity_ready)
    {
        runtime->cloud_started = alarm_start_cloud(runtime);
    }
    if (!runtime->cloud_started)
    {
        return;
    }
    runtime->transport->poll(runtime->transport->user, now);
    if (runtime->cloud_stopping)
    {
        if (runtime->transport->stop(runtime->transport->user))
        {
            runtime->cloud_started = false;
            runtime->cloud_stopping = false;
            /* 连接真正停下且持久队列为空，才向前台报告可静止。 */
            if (alarm_store_pending(&runtime->store) == 0)
            {
                runtime->background_waiting = true;
                message.kind = ALARM_MSG_BACKGROUND_IDLE;
                message.request = runtime->last_handled_request_id;
                alarm_send_result_to_front(runtime, &message);
            }
        }
        return;
    }
    if (runtime->transport->online(runtime->transport->user))
    {
        if (runtime->platform_registered && (uint32_t)(now - runtime->last_heartbeat) >= ALARM_BUTTON_HEARTBEAT_MS)
        {
            runtime->heartbeat_needed = true;
        }
        alarm_send_registration_or_heartbeat(runtime, now);
        alarm_reporter_poll(&runtime->reporter, runtime->platform_registered && !runtime->waiting_control_reply, now);
    }
    else
    {
        alarm_reporter_poll(&runtime->reporter, false, now);
    }
    if (runtime->reporter.last_error != ALARM_OK && runtime->reporter.last_error != ALARM_ERROR_REJECTED)
    {
        alarm_report_error(runtime, runtime->reporter.last_error);
    }
    if (runtime->platform_registered && !runtime->heartbeat_needed && !runtime->waiting_control_reply &&
        alarm_store_pending(&runtime->store) == 0)
    {
        runtime->cloud_stopping = true;
        (void)runtime->transport->stop(runtime->transport->user);
    }
}

/*******************************************************************************
* Function Name  : alarm_read_front_requests
* Description    : 等待并处理有限个前台请求
* Input          : runtime - 后台上下文
* Output         : 保存请求或唤醒状态
* Return         : 无
* Attention      : 后台静止时一直等待唤醒；每轮只取有限消息，留时间处理网络
*******************************************************************************/
static void alarm_read_front_requests(alarm_runtime_t *runtime)
{
    alarm_message_t message;
    unsigned drained;
    if (runtime->services->system.queue_get(runtime->background_queue, &message,
                                      runtime->background_waiting ? SYSTEM_WAIT_FOREVER : 20U))
    {
        alarm_handle_front_request(runtime, &message);
    }
    for (drained = 0;
         drained < 7 && runtime->services->system.queue_get(runtime->background_queue, &message, 0); ++drained)
    {
        alarm_handle_front_request(runtime, &message);
    }
}

/*******************************************************************************
* Function Name  : alarm_background_task
* Description    : 后台独占存储和云协议状态
* Input          : argument - 运行上下文
* Output         : 持久化、重试及回执通知
* Return         : 不返回
* Attention      : 网络及文件延迟不会阻塞前台任务
*******************************************************************************/
static void alarm_background_task(void *argument)
{
    alarm_runtime_t *runtime = argument;
    bool initialized = false;
    while (1)
    {
        if (!__atomic_load_n(&runtime->startup_ready, __ATOMIC_ACQUIRE))
        {
            runtime->services->system.delay(
                __atomic_load_n(&runtime->startup_failed, __ATOMIC_ACQUIRE) ? 60000U : 10U);
            continue;
        }
        if (!initialized)
        {
            alarm_init_background(runtime);
            initialized = true;
        }
        alarm_read_front_requests(runtime);
        alarm_process_background(runtime, alarm_now_ms(runtime));
    }
}

/*******************************************************************************
* Function Name  : alarm_queue_save_request
* Description    : 只排队有效按键事件，不进行持久化
* Input          : user - 运行上下文；request - 请求号；event - 事件快照
* Output         : 后台队列
* Return         : ALARM_OK已排队；ALARM_ERROR_FULL队列满
* Attention      : 排队成功不等于持久成功
*******************************************************************************/
static int alarm_queue_save_request(void *user, uint32_t request, const alarm_event_t *event)
{
    alarm_runtime_t *runtime = user;
    alarm_message_t message = {0};
    message.kind = ALARM_MSG_SAVE_EVENT;
    message.request = request;
    message.event = *event;
    return runtime->services->system.queue_put(runtime->background_queue, &message, 0) ? ALARM_OK : ALARM_ERROR_FULL;
}

/*******************************************************************************
* Function Name  : alarm_set_outputs
* Description    : 把统一声光状态交给唯一板级入口
* Input          : user - 上下文；led/buzzer - 状态
* Output         : 板输出
* Return         : true成功
* Attention      : 只有前台调用
*******************************************************************************/
static bool alarm_set_outputs(void *user, bool led, bool buzzer)
{
    alarm_runtime_t *runtime = user;
    return runtime->services->board.outputs(runtime->services->board.user, led, buzzer);
}

/*******************************************************************************
* Function Name  : alarm_report_front_error
* Description    : 输出前台故障诊断
* Input          : user - 运行上下文；error - 错误码
* Output         : 诊断日志
* Return         : 无
* Attention      : 不向自身队列阻塞投递
*******************************************************************************/
static void alarm_report_front_error(void *user, int error)
{
    alarm_runtime_t *runtime = user;
    runtime->services->system.fault("alarm-ui", error);
}

/*******************************************************************************
* Function Name  : alarm_notify_key_wakeup
* Description    : 从板中断唤醒前台
* Input          : user - 运行上下文
* Output         : 零等待前台消息
* Return         : 无
* Attention      : 仅唤醒，消抖仍由前台负责
*******************************************************************************/
static void alarm_notify_key_wakeup(void *user)
{
    alarm_runtime_t *runtime = user;
    alarm_message_t message = {0};
    message.kind = ALARM_MSG_WAKE;
    (void)runtime->services->system.queue_put(runtime->front_queue, &message, 0);
}

/*******************************************************************************
* Function Name  : alarm_handle_background_result
* Description    : 按事件身份消费后台结果
* Input          : runtime - 前台上下文；message - 结果；now - 毫秒
* Output         : 提示、保存数量及休眠条件
* Return         : 无
* Attention      : 旧静止消息不能许可新请求休眠
*******************************************************************************/
static void alarm_handle_background_result(alarm_runtime_t *runtime, const alarm_message_t *message, uint32_t now)
{
    switch (message->kind)
    {
    case ALARM_MSG_SAVE_RESULT:
        alarm_button_on_save_result(&runtime->button_state, message->request, message->event_id, message->result);
        break;
    case ALARM_MSG_EVENT_CONFIRMED:
        alarm_button_on_alarm_confirmed(&runtime->button_state, message->event_id);
        break;
    case ALARM_MSG_BACKGROUND_IDLE:
        /* 旧静止通知不能让新按键进入休眠。 */
        if (message->request == runtime->button_state.latest_request_id && !runtime->button_state.pending_save_count)
        {
            runtime->button_state.background_idle = true;
        }
        break;
    case ALARM_MSG_HEARTBEAT_CONFIRMED:
        runtime->next_heartbeat_ms = now + ALARM_BUTTON_HEARTBEAT_MS;
        break;
    case ALARM_MSG_BACKGROUND_ERROR:
        runtime->button_state.last_error = message->result;
        runtime->button_state.background_idle = false;
        break;
    default:
        break;
    }
}

/*******************************************************************************
* Function Name  : alarm_read_background_results
* Description    : 每轮处理有限个后台结果
* Input          : runtime - 前台上下文；now - 本轮时刻
* Output         : 保存、回执和静止状态
* Return         : 无
* Attention      : 不能让消息处理长期占用声光采样周期
*******************************************************************************/
static void alarm_read_background_results(alarm_runtime_t *runtime, uint32_t now)
{
    alarm_message_t message;
    unsigned drained;
    for (drained = 0;
         drained < 8 && runtime->services->system.queue_get(runtime->front_queue, &message, 0); ++drained)
    {
        alarm_handle_background_result(runtime, &message, now);
    }
}

/*******************************************************************************
* Function Name  : alarm_process_button
* Description    : 读取按键并推进消抖与声光
* Input          : runtime - 前台上下文；now - 本轮时刻
* Output         : 前台报警和声光状态
* Return         : 无
* Attention      : 读键失败沿用稳定状态，不能伪装一次松手
*******************************************************************************/
static void alarm_process_button(alarm_runtime_t *runtime, uint32_t now)
{
    bool pressed;
    if (runtime->services->board.read_key(runtime->services->board.user, &pressed))
    {
        alarm_button_update(&runtime->button_state, pressed, now);
        return;
    }
    runtime->button_state.last_error = ALARM_ERROR_NOT_READY;
    runtime->button_state.key.candidate = runtime->button_state.key.stable;
    alarm_button_update(&runtime->button_state, runtime->button_state.key.stable, now);
}

/*******************************************************************************
* Function Name  : alarm_wake_background_for_heartbeat
* Description    : 心跳到期时唤醒后台
* Input          : runtime - 前台上下文；now - 本轮时刻
* Output         : 后台唤醒消息和下一次心跳时刻
* Return         : 无
* Attention      : 投递失败保留故障状态，下一轮继续尝试
*******************************************************************************/
static void alarm_wake_background_for_heartbeat(alarm_runtime_t *runtime, uint32_t now)
{
    alarm_message_t message = {0};
    if ((int32_t)(now - runtime->next_heartbeat_ms) < 0)
    {
        return;
    }
    message.kind = ALARM_MSG_WAKE;
    runtime->button_state.background_idle = false;
    if (runtime->services->system.queue_put(runtime->background_queue, &message, 0))
    {
        runtime->next_heartbeat_ms = now + ALARM_BUTTON_HEARTBEAT_MS;
    }
    else
    {
        runtime->button_state.last_error = ALARM_ERROR_FULL;
    }
}

/*******************************************************************************
* Function Name  : alarm_wait_for_next_event
* Description    : 根据休眠条件等待下一条消息
* Input          : runtime - 前台上下文；now - 本轮时刻
* Output         : 电源工作锁及可能收到的后台结果
* Return         : 无
* Attention      : 板级唤醒未核验或业务未静止时保持工作锁
*******************************************************************************/
static void alarm_wait_for_next_event(alarm_runtime_t *runtime, uint32_t now)
{
    system_interface_t *system = &runtime->services->system;
    alarm_message_t message;
    bool sleep_allowed = runtime->services->board.wake_verified && runtime->services->board.set_wakeup &&
                         alarm_button_can_sleep(&runtime->button_state);
    uint32_t wait_ms = sleep_allowed ? (uint32_t)(runtime->next_heartbeat_ms - now) : 5U;
    bool received;
    system->power_hold(system->user, !sleep_allowed);
    received = system->queue_get(runtime->front_queue, &message, wait_ms);
    system->power_hold(system->user, true);
    if (received)
    {
        alarm_handle_background_result(runtime, &message, alarm_now_ms(runtime));
    }
}

/*******************************************************************************
* Function Name  : alarm_front_task
* Description    : 独立采样按键并推进声光，不调用存储或网络
* Input          : argument - 运行上下文
* Output         : 报警提示及后台提交
* Return         : 不返回
* Attention      : 只在后台完全静止且唤醒已核验时释放工作锁
*******************************************************************************/
static void alarm_front_task(void *argument)
{
    alarm_runtime_t *runtime = argument;
    bool initialized = false;
    while (1)
    {
        uint32_t now;
        if (!__atomic_load_n(&runtime->startup_ready, __ATOMIC_ACQUIRE))
        {
            runtime->services->system.delay(
                __atomic_load_n(&runtime->startup_failed, __ATOMIC_ACQUIRE) ? 60000U : 10U);
            continue;
        }
        if (!initialized)
        {
            runtime->next_heartbeat_ms = alarm_now_ms(runtime) + ALARM_BUTTON_HEARTBEAT_MS;
            initialized = true;
        }
        now = alarm_now_ms(runtime);
        alarm_read_background_results(runtime, now);
        alarm_process_button(runtime, now);
        alarm_wake_background_for_heartbeat(runtime, now);
        alarm_wait_for_next_event(runtime, now);
    }
}

/*******************************************************************************
* Function Name  : alarm_product_start
* Description    : 创建一个报警产品的独立前后台上下文
* Input          : services - 已绑定服务
* Output         : 两个任务及独立队列
* Return         : true - 两个任务均创建成功；false - 启动失败
* Attention      : 两个任务均创建后才释放启动门闩；失败时已创建任务保持阻塞
*******************************************************************************/
bool alarm_product_start(product_services_t *services)
{
    alarm_runtime_t *runtime;
    alarm_button_config_t config = alarm_button_default_config();
    alarm_button_callbacks_t callbacks;
    if (!services || !services->board.ready || !services->transport)
    {
        return false;
    }
    runtime = services->system.allocate(sizeof(*runtime));
    if (!runtime)
    {
        services->system.fault("alarm-allocation", ALARM_ERROR_NOT_READY);
        return false;
    }
    memset(runtime, 0, sizeof(*runtime));
    runtime->services = services;
    runtime->transport = services->transport;
    runtime->background_queue = services->system.queue_create(ALARM_BACKGROUND_QUEUE_CAPACITY, sizeof(alarm_message_t));
    runtime->front_queue = services->system.queue_create(ALARM_FRONT_QUEUE_CAPACITY, sizeof(alarm_message_t));
    callbacks.user = runtime;
    callbacks.output = alarm_set_outputs;
    callbacks.fault = alarm_report_front_error;
    callbacks.submit_event = alarm_queue_save_request;
    if (!runtime->background_queue || !runtime->front_queue ||
        alarm_button_init(&runtime->button_state, &config, &callbacks) != ALARM_OK)
    {
        services->system.fault("alarm-initialize", ALARM_ERROR_NOT_READY);
        return false;
    }
    if (services->board.set_wakeup)
    {
        services->board.set_wakeup(services->board.user, alarm_notify_key_wakeup, runtime);
    }
    if (!services->system.thread_start("alarm-ui", alarm_front_task, runtime, 8192U, true))
    {
        services->system.fault("alarm-ui-start", ALARM_ERROR_NOT_READY);
        return false;
    }
    if (!services->system.thread_start("alarm-worker", alarm_background_task, runtime, 16384U, false))
    {
        __atomic_store_n(&runtime->startup_failed, true, __ATOMIC_RELEASE);
        services->system.fault("alarm-worker-start", ALARM_ERROR_NOT_READY);
        return false;
    }
    __atomic_store_n(&runtime->startup_ready, true, __ATOMIC_RELEASE);
    return true;
}
