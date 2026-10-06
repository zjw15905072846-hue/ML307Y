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
#define ALARM_SAVE_RETRY_MS 1000U /* 本地保存失败的重试间隔，独立于网络业务退避。 */

/*-------------------------------------------typedef---------------------------------------------*/
/* 后台独占网络阶段；前台只发布静止条件与唤醒标志。 */
typedef enum
{
    ALARM_NETWORK_START,
    ALARM_NETWORK_RESTORING,
    ALARM_NETWORK_ONLINE,
    ALARM_NETWORK_STOPPING,
    ALARM_NETWORK_RADIO_OFF,
    ALARM_NETWORK_SLEEP
} alarm_network_phase_t;

/* 前后台只通过消息传递请求和结果，避免跨任务直接改业务状态。 */
typedef enum
{
    ALARM_MSG_SAVE_EVENT,          /* 前台确认按下，请后台保存。 */
    ALARM_MSG_WAKE,                /* 按键或心跳唤醒后台。 */
    ALARM_MSG_SAVE_RESULT,         /* 持久化完成结果。 */
    ALARM_MSG_EVENT_CONFIRMED,     /* 平台业务回执完成。 */
    ALARM_MSG_BACKGROUND_IDLE,     /* 保留连接，当前业务及持久队列均已完成。 */
    ALARM_MSG_NETWORK_EVENT,       /* SDK 事件就绪，仅唤醒后台处理。 */
    ALARM_MSG_HEARTBEAT_CONFIRMED, /* 心跳业务回执完成。 */
    ALARM_MSG_BACKGROUND_ERROR    /* 后台故障通知。 */
} alarm_message_kind_t;

typedef struct
{
    alarm_message_kind_t kind;
    uint32_t request; /* 保存请求编号，防止旧结果影响新按键。 */
    uint32_t event_id; /* 成功持久化后的事件 ID。 */
    uint32_t deadline_ms; /* 后台指定离线等待的绝对期限。 */
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
    alarm_message_t pending_save; /* 当前待落盘请求；后续请求留在后台队列中。 */
    uint32_t next_save_retry_ms;
    bool save_pending; /* 只有持久化成功才完成此请求。 */
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
    bool background_waiting; /* 后台业务静止，不代表 MQTT 已断开。 */
    bool telemetry_requested; /* 只在业务触发时采集。 */
    bool save_failed; /* 保存失败的报警不能用空队列伪装完成。 */
    uint32_t key_pending; /* 中断置位，前台采样前消费。 */
    uint32_t network_pending; /* SDK 事件通知合并标志。 */
    bool cloud_stopping;
    bool offline_enabled; /* 仅显式验证配置启用，不能等同于实板已验证。 */
    bool heartbeat_completed; /* 区分首次心跳与后续可让报警优先的周期心跳。 */
    bool control_storage_failed; /* 控制报文序号保存失败也阻止休眠，成功重试后清除。 */
    bool restart_requested; /* 关网期间按键到达后保留到恢复完成，避免停止等待吞掉事件。 */
    bool retry_sleep; /* 本轮控制业务耗尽尝试预算。 */
    alarm_network_phase_t network_phase;
    uint32_t attempt_started;
    uint32_t stop_started;
    uint32_t offline_deadline;
    uint32_t front_quiet; /* 前台原子发布：声光结束、释放稳定且无待保存请求。 */
    uint32_t button_wake_pending; /* ISR 置位，后台独立消费，避免前台先清除事件。 */
    int last_fault;
    int front_background_error; /* 前台记录后台错误，健康静止通知才可解除。 */
    const char *last_cloud_failure; /* 固定阶段名，用于抑制连续重复故障。 */
    int last_cloud_result;
    uint8_t frame[KAIWAN_PROTOCOL_MAXIMUM_FRAME_SIZE];
    uint8_t body[KAIWAN_HANDSET_REGISTER_BYTES];
    char payload_text[KAIWAN_PROTOCOL_MAXIMUM_JSON_SIZE]; /* 实际 MQTT 文本：HEX 帧或加密 JSON。 */
} alarm_runtime_t;

/*-------------------------------------------variables-------------------------------------------*/
/* 编译配置提供的 AES 密钥；仅加密模式要求非零配置。 */
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
* Function Name  : alarm_cloud_diagnostic
* Description    : 在宏开关允许时输出云业务阶段及关联编号
* Input          : runtime - 上下文；stage - 固定名称；value - 关联编号；result - 结果
* Output         : 可选平台诊断回调
* Return         : 无
* Attention      : 阶段成功只代表该阶段完成，入队成功不代表平台接收
*******************************************************************************/
static void alarm_cloud_diagnostic(alarm_runtime_t *runtime, const char *stage, uint32_t value, int result)
{
    if (ALARM_BUTTON_CLOUD_DIAGNOSTICS && runtime->services->system.diagnostic)
    {
        runtime->services->system.diagnostic(stage, value, result);
    }
}

/*******************************************************************************
* Function Name  : alarm_cloud_failure
* Description    : 输出具体云故障阶段并抑制连续重复的相同故障
* Input          : runtime - 上下文；stage - 固定名称；result - 错误或平台拒绝码
* Output         : 故障诊断及最近故障标记
* Return         : 无
* Attention      : 不通过吞掉错误改变业务重试或持久记录
*******************************************************************************/
static void alarm_cloud_failure(alarm_runtime_t *runtime, const char *stage, int result)
{
    if (!runtime->last_cloud_failure || strcmp(runtime->last_cloud_failure, stage) != 0 ||
        runtime->last_cloud_result != result)
    {
        runtime->services->system.fault(stage, result);
        runtime->last_cloud_failure = stage;
        runtime->last_cloud_result = result;
    }
}

/*******************************************************************************
* Function Name  : alarm_require_cloud_setting
* Description    : 检查一个配置条件，缺失时打印可定位的固定名称
* Input          : runtime - 上下文；valid - 条件是否满足；stage - 配置诊断名
* Output         : 缺配置时输出诊断
* Return         : valid 原值
* Attention      : 不打印宏的实际内容，不把示例参数用作正式参数
*******************************************************************************/
static bool alarm_require_cloud_setting(alarm_runtime_t *runtime, bool valid, const char *stage)
{
    if (!valid)
    {
        alarm_cloud_failure(runtime, stage, ALARM_ERROR_CONFIG);
    }
    return valid;
}

/*******************************************************************************
* Function Name  : alarm_require_telemetry
* Description    : 在组包前报告未采到且没有合法占位约定的具体遥测字段
* Input          : runtime - 上下文；event - 即将编码的真实快照
* Output         : 电压、电量或信号缺失诊断
* Return         : true - 可以组包；false - 继续保留记录等待处理
* Attention      : 不将百分比或信号伪造为固定有效值
*******************************************************************************/
static bool alarm_require_telemetry(alarm_runtime_t *runtime, const alarm_event_t *event)
{
    if (runtime->identity.unknown_telemetry_verified)
    {
        return true;
    }
    if (!(event->valid & ALARM_TELEMETRY_VOLTAGE))
    {
        alarm_cloud_failure(runtime, "cloud-telemetry-voltage", ALARM_ERROR_NOT_READY);
    }
    if (!(event->valid & ALARM_TELEMETRY_CSQ))
    {
        alarm_cloud_failure(runtime, "cloud-telemetry-signal", ALARM_ERROR_NOT_READY);
    }
    if (!(event->valid & ALARM_TELEMETRY_PERCENT))
    {
        alarm_cloud_failure(runtime, "cloud-telemetry-percent", ALARM_ERROR_NOT_READY);
    }
    return (event->valid & 7U) == 7U;
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
* Description    : 复用帧与 CRC 编码，按宏选择明文 HEX 或 AES JSON 并记录实际上行
* Input          : runtime - 运行上下文；command/sequence - 协议头；body/length - 数据体；cookie - 关联
* Output         : 编码后深拷贝进传输队列
* Return         : true - 已入传输队列；false - 编码或入队失败
* Attention      : 工作区仅由后台任务使用；加密模式每次使用新 IV，明文不依赖密钥或随机源
*******************************************************************************/
static bool alarm_publish_frame(alarm_runtime_t *runtime, uint8_t command, uint16_t sequence,
                          const uint8_t *body, uint16_t length, uint32_t cookie)
{
    size_t frame_length;
    size_t payload_length;
    uint8_t iv[16];
    const char *kind;
    int result;
    result = kaiwan_protocol_build_frame(&runtime->protocol, sequence, command, body, length, runtime->frame,
        sizeof(runtime->frame), &frame_length);
    if (result != KAIWAN_OK)
    {
        alarm_cloud_failure(runtime, "cloud-frame-encode", result);
        return false;
    }
    if (ALARM_BUTTON_ENCRYPTION_ENABLED)
    {
        if (!runtime->services->system.random(iv, sizeof(iv)))
        {
            alarm_cloud_failure(runtime, "cloud-random-source", ALARM_ERROR_NOT_READY);
            return false;
        }
        result = kaiwan_protocol_wrap_json(&runtime->protocol, &runtime->codec, runtime->frame, frame_length, iv,
            runtime->payload_text, sizeof(runtime->payload_text), &payload_length);
    }
    else
    {
        result = kaiwan_protocol_hex_encode(runtime->frame, frame_length, runtime->payload_text,
            sizeof(runtime->payload_text), &payload_length);
    }
    if (result != KAIWAN_OK)
    {
        alarm_cloud_failure(runtime, "cloud-payload-encode", result);
        return false;
    }
    result = runtime->transport->publish(runtime->transport->user, runtime->cloud.platform_up_topic,
        (const uint8_t *)runtime->payload_text, payload_length, runtime->cloud.qos, false, cookie);
    if (ALARM_BUTTON_PACKET_LOG_ENABLED && runtime->services->system.packet_log)
    {
        kind = command == KAIWAN_COMMAND_REGISTER ? "registration" :
            command == KAIWAN_COMMAND_HISTORY ? "history" :
            length > 16U && body[16] == KAIWAN_EVENT_HEARTBEAT ? "heartbeat" : "alarm";
        runtime->services->system.packet_log(kind, sequence, runtime->cloud.platform_up_topic,
            (const uint8_t *)runtime->payload_text, payload_length, result);
    }
    if (result != KAIWAN_CLOUD_OK)
    {
        alarm_cloud_failure(runtime, "cloud-publish-queue", result);
    }
    return result == KAIWAN_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : alarm_event_is_ready
* Description    : 判断持久事件的补报和遥测条件是否满足
* Input          : user - 运行上下文；event - 持久事件
* Output         : 未满足时报告具体阶段，不分配序号或修改记录
* Return         : true - 可以尝试发送；false - 暂时保留并允许检查后续事件
* Attention      : 不伪造旧记录的时间或电量，也不删除待补报事件
*******************************************************************************/
static bool alarm_event_is_ready(void *user, const alarm_event_t *event)
{
    alarm_runtime_t *runtime = user;
    bool recovered = event->id <= runtime->restored_maximum_id;
    bool history = recovered && ALARM_BUTTON_REPLAY_MODE == 1;
    if (recovered && ALARM_BUTTON_REPLAY_MODE == 0)
    {
        alarm_cloud_failure(runtime, "cloud-history-policy", ALARM_ERROR_CONFIG);
        return false;
    }
    if (history && (!(event->valid & ALARM_TIME_UTC) || !event->utc_seconds))
    {
        alarm_cloud_failure(runtime, "cloud-history-time", ALARM_ERROR_NOT_READY);
        return false;
    }
    return alarm_require_telemetry(runtime, event);
}

/*******************************************************************************
* Function Name  : alarm_publish_event
* Description    : 编码已满足发送条件的实时或历史手报事件
* Input          : user - 运行上下文；event - 持久事件；sequence - 已持久分配的序号
* Output         : 构造并提交上行报文
* Return         : true - 已入传输队列；false - 编码或发送失败
* Attention      : 入队成功后仍保留事件，直到对应业务成功回执完成持久删除
*******************************************************************************/
static bool alarm_publish_event(void *user, const alarm_event_t *event, uint16_t sequence)
{
    alarm_runtime_t *runtime = user;
    bool history = event->id <= runtime->restored_maximum_id && ALARM_BUTTON_REPLAY_MODE == 1;
    bool queued;
    int length;
    if (!alarm_event_is_ready(user, event))
    {
        return false;
    }
    /* 恢复事件只在已确定历史补报规则时标成历史帧。 */
    length = kaiwan_handset_event_payload(&runtime->identity, event, history, runtime->body, sizeof(runtime->body));
    if (length < 0)
    {
        alarm_report_error(runtime, length);
        return false;
    }
    queued = alarm_publish_frame(runtime, history ? KAIWAN_COMMAND_HISTORY : KAIWAN_COMMAND_EVENT, sequence,
        runtime->body, (uint16_t)length, sequence);
    if (queued)
    {
        alarm_cloud_diagnostic(runtime, history ? "history-queued" : "alarm-queued", event->id, 0);
    }
    return queued;
}

/*******************************************************************************
* Function Name  : alarm_send_registration_or_heartbeat
* Description    : 本次上电注册并按确认期限发送心跳，独立序列不误删报警队列
* Input          : runtime - 运行上下文；now - 单调毫秒
* Output         : 推进注册或按配置周期发送业务心跳
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
        if ((uint32_t)(now - runtime->control_sent_at_ms) < ALARM_BUTTON_CONFIRMATION_TIMEOUT_MS)
        {
            return;
        }
        runtime->waiting_control_reply = false;
        alarm_cloud_failure(runtime, runtime->control_command == KAIWAN_COMMAND_REGISTER
            ? "cloud-registration-timeout" : "cloud-heartbeat-timeout", ALARM_ERROR_NOT_READY);
        runtime->next_control_retry_ms = now + ALARM_BUTTON_RETRY_MINIMUM_MS;
    }
    if ((int32_t)(now - runtime->next_control_retry_ms) < 0)
    {
        return;
    }
    if (runtime->platform_registered && !runtime->heartbeat_needed)
    {
        return;
    }
    if (runtime->offline_enabled && runtime->platform_registered && runtime->heartbeat_completed &&
        (alarm_store_pending(&runtime->store) || runtime->reporter.inflight))
    {
        return; /* 周期心跳让位于报警，但上方仍处理已经发出的控制业务超时。 */
    }
    telemetry = runtime->telemetry;
    telemetry.event_type = 1;
    if (!alarm_require_telemetry(runtime, &telemetry))
    {
        runtime->next_control_retry_ms = now + ALARM_BUTTON_RETRY_MINIMUM_MS;
        alarm_report_error(runtime, ALARM_ERROR_NOT_READY);
        return;
    }
    command = runtime->platform_registered ? KAIWAN_COMMAND_EVENT : KAIWAN_COMMAND_REGISTER;
    length = runtime->platform_registered
                 ? kaiwan_handset_event_payload(&runtime->identity, &telemetry, false, runtime->body, sizeof(runtime->body))
                 : kaiwan_handset_registration_payload(&runtime->identity, &telemetry, runtime->body, sizeof(runtime->body));
    if (length < 0)
    {
        runtime->next_control_retry_ms = now + ALARM_BUTTON_RETRY_MINIMUM_MS;
        alarm_report_error(runtime, length);
        return;
    }
    if (alarm_store_sequence(&runtime->store, &runtime->control_sequence) != ALARM_OK)
    {
        runtime->control_storage_failed = true;
        runtime->next_control_retry_ms = now + ALARM_BUTTON_RETRY_MINIMUM_MS;
        alarm_report_error(runtime, ALARM_ERROR_STORAGE);
        return;
    }
    runtime->control_storage_failed = false;
    runtime->control_command = command;
    runtime->control_sent_at_ms = now;
    runtime->waiting_control_reply = alarm_publish_frame(runtime, command,
        runtime->control_sequence, runtime->body, (uint16_t)length,
        ALARM_CONTROL_COOKIE | runtime->control_sequence);
    if (!runtime->waiting_control_reply)
    {
        runtime->next_control_retry_ms = now + ALARM_BUTTON_RETRY_MINIMUM_MS;
    }
    else
    {
        alarm_cloud_diagnostic(runtime, command == KAIWAN_COMMAND_REGISTER
            ? "registration-queued" : "heartbeat-queued", runtime->control_sequence, 0);
    }
}

/*******************************************************************************
* Function Name  : alarm_load_cloud_config
* Description    : 加载明确产品配置，未配置或零密钥禁止连接
* Input          : runtime - 运行上下文
* Output         : 填充协议、传输与未知遥测策略
* Return         : true - 可以尝试接入；false - 配置缺失或未验证
* Attention      : 厂商码须非空且不超本地容量；不得记录或硬编码真实密钥
*******************************************************************************/
static bool alarm_load_cloud_config(alarm_runtime_t *runtime)
{
    unsigned index;
    uint8_t nonzero = 0;
    bool valid = true;
    kaiwan_cloud_config_init(&runtime->cloud);
    kaiwan_protocol_config_init(&runtime->protocol);
    runtime->identity.firmware = ALARM_BUTTON_FIRMWARE_VERSION;
    /* 联调许可只允许使用未知占位；不修改采样有效位或正式验证宏。 */
    runtime->identity.unknown_telemetry_verified =
        ALARM_BUTTON_UNKNOWN_TELEMETRY_VERIFIED != 0 || ALARM_BUTTON_UNKNOWN_TELEMETRY_TRIAL != 0;
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
    valid = alarm_require_cloud_setting(runtime, ALARM_BUTTON_CLOUD_ENABLED != 0,
        "cloud-config-disabled") && valid;
    valid = alarm_require_cloud_setting(runtime, ALARM_BUTTON_PROTOCOL_VERIFIED != 0 ||
        (!ALARM_BUTTON_ENCRYPTION_ENABLED && ALARM_BUTTON_PLAINTEXT_TRIAL),
        "cloud-config-protocol") && valid;
    valid = alarm_require_cloud_setting(runtime, ALARM_BUTTON_MANUFACTURER_ID != 0 &&
        ALARM_BUTTON_MANUFACTURER_ID <= UINT16_MAX, "cloud-config-manufacturer") && valid;
    if (ALARM_BUTTON_ENCRYPTION_ENABLED)
    {
        valid = alarm_require_cloud_setting(runtime,
            sizeof(ALARM_BUTTON_FACTORY_CODE) > 1 && ALARM_BUTTON_FACTORY_CODE[0] != '\0' &&
            sizeof(ALARM_BUTTON_FACTORY_CODE) <= sizeof(runtime->protocol.factory_code),
            "cloud-config-factory-code") && valid;
        valid = alarm_require_cloud_setting(runtime, nonzero != 0, "cloud-config-aes-key") && valid;
    }
    valid = alarm_require_cloud_setting(runtime, sizeof(ALARM_BUTTON_BROKER_HOST) > 1 &&
        sizeof(ALARM_BUTTON_BROKER_HOST) <= sizeof(runtime->cloud.broker_host), "cloud-config-broker") && valid;
    valid = alarm_require_cloud_setting(runtime, sizeof(ALARM_BUTTON_MQTT_USERNAME) > 1 &&
        sizeof(ALARM_BUTTON_MQTT_USERNAME) <= sizeof(runtime->cloud.username), "cloud-config-username") && valid;
    valid = alarm_require_cloud_setting(runtime, sizeof(ALARM_BUTTON_MQTT_PASSWORD) > 1 &&
        sizeof(ALARM_BUTTON_MQTT_PASSWORD) <= sizeof(runtime->cloud.password), "cloud-config-password") && valid;
    valid = alarm_require_cloud_setting(runtime, !ALARM_BUTTON_USE_TLS || ALARM_BUTTON_TLS_CONFIG != NULL,
        "cloud-config-tls") && valid;
    if (!ALARM_BUTTON_UNKNOWN_TELEMETRY_VERIFIED)
    {
        alarm_cloud_diagnostic(runtime, ALARM_BUTTON_UNKNOWN_TELEMETRY_TRIAL
            ? "unknown-telemetry-trial" : "unknown-telemetry-unconfirmed",
            ALARM_BUTTON_UNKNOWN_TELEMETRY_BYTE, ALARM_ERROR_NOT_READY);
    }
    if (!valid)
    {
        return false;
    }
    if (ALARM_BUTTON_ENCRYPTION_ENABLED)
    {
        memcpy(runtime->protocol.factory_code, ALARM_BUTTON_FACTORY_CODE, sizeof(ALARM_BUTTON_FACTORY_CODE));
    }
    alarm_cloud_diagnostic(runtime, ALARM_BUTTON_ENCRYPTION_ENABLED ? "mode-aes-json" : "mode-plaintext-hex", 0, 0);
    memcpy(runtime->cloud.broker_host, ALARM_BUTTON_BROKER_HOST, sizeof(ALARM_BUTTON_BROKER_HOST));
    memcpy(runtime->cloud.username, ALARM_BUTTON_MQTT_USERNAME, sizeof(ALARM_BUTTON_MQTT_USERNAME));
    memcpy(runtime->cloud.password, ALARM_BUTTON_MQTT_PASSWORD, sizeof(ALARM_BUTTON_MQTT_PASSWORD));
    runtime->cloud.broker_port = ALARM_BUTTON_BROKER_PORT;
    runtime->cloud.use_tls = ALARM_BUTTON_USE_TLS != 0;
    runtime->cloud.tls_config = ALARM_BUTTON_TLS_CONFIG;
    runtime->cloud.reconnect_minimum_ms = ALARM_BUTTON_RETRY_MINIMUM_MS;
    runtime->cloud.reconnect_maximum_ms = ALARM_BUTTON_RETRY_MAXIMUM_MS;
    return true;
}

/*******************************************************************************
* Function Name  : alarm_on_cloud_state
* Description    : 连接变化时重置传输等待，保留已确认注册和心跳期限
* Input          : online - 状态；user - 上下文
* Output         : 控制回执等待及重试时间
* Return         : 无
* Attention      : 后台串行调用；普通重连不触发重新注册或额外心跳
*******************************************************************************/
static void alarm_on_cloud_state(bool online, void *user)
{
    alarm_runtime_t *runtime = user;
    alarm_cloud_diagnostic(runtime, online ? "mqtt-subscribed" : "mqtt-offline", 0, 0);
    runtime->last_cloud_failure = NULL;
    /* 注册成功属于本次运行的业务状态，普通断线和重新订阅不能清除。 */
    runtime->waiting_control_reply = false;
    /* 保留未确认心跳及原来的计时起点；启动时由初始化请求首次心跳。 */
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
        alarm_cloud_diagnostic(runtime, "mqtt-puback", (uint16_t)cookie, 0);
        return;
    }
    alarm_cloud_failure(runtime, "cloud-publish-transport", result);
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
    size_t frame_length;
    if (topic_length != strlen(runtime->cloud.platform_down_topic) ||
        memcmp(topic, runtime->cloud.platform_down_topic, topic_length) != 0)
    {
        alarm_cloud_failure(runtime, "cloud-downlink-topic", KAIWAN_CLOUD_ERROR_ARGUMENT);
        return KAIWAN_CLOUD_ERROR_ARGUMENT;
    }
    if (ALARM_BUTTON_ENCRYPTION_ENABLED)
    {
        result = kaiwan_session_decode(&runtime->protocol, &runtime->codec, (const char *)payload, length,
            runtime->frame, sizeof(runtime->frame), &frame);
    }
    else
    {
        result = kaiwan_protocol_hex_decode((const char *)payload, length,
            runtime->frame, sizeof(runtime->frame), &frame_length);
        if (result == KAIWAN_OK)
        {
            result = kaiwan_protocol_parse_frame(&runtime->protocol, runtime->frame, frame_length, &frame);
        }
    }
    if (result != KAIWAN_OK)
    {
        alarm_cloud_failure(runtime, "cloud-downlink-decode", result);
        return KAIWAN_CLOUD_ERROR_ARGUMENT;
    }
    if (kaiwan_protocol_parse_server_response(&frame, &response) != KAIWAN_OK)
    {
        alarm_cloud_failure(runtime, "cloud-downlink-response", KAIWAN_CLOUD_ERROR_ARGUMENT);
        return KAIWAN_CLOUD_ERROR_ARGUMENT;
    }
    /* 注册/心跳回执单独消费，不能进入报警队列确认路径。 */
    if (runtime->waiting_control_reply && frame.sequence == runtime->control_sequence)
    {
        runtime->waiting_control_reply = false;
        if (response == 0)
        {
            runtime->last_cloud_failure = NULL;
            alarm_cloud_diagnostic(runtime, runtime->control_command == KAIWAN_COMMAND_REGISTER
                ? "registration-confirmed" : "heartbeat-confirmed", frame.sequence, 0);
            if (runtime->control_command == KAIWAN_COMMAND_REGISTER)
            {
                runtime->platform_registered = true;
            }
            else
            {
                runtime->heartbeat_needed = false;
                runtime->last_heartbeat = now;
                runtime->heartbeat_completed = true;
                message.kind = ALARM_MSG_HEARTBEAT_CONFIRMED;
                alarm_send_result_to_front(runtime, &message);
            }
        }
        else
        {
            alarm_cloud_failure(runtime, runtime->control_command == KAIWAN_COMMAND_REGISTER
                ? "cloud-registration-rejected" : "cloud-heartbeat-rejected", response);
            runtime->next_control_retry_ms = now + ALARM_BUTTON_RETRY_MINIMUM_MS;
        }
        return KAIWAN_CLOUD_OK;
    }
    result = alarm_reporter_on_platform_confirmation(&runtime->reporter, frame.sequence, response, now, &event_id);
    if (result == ALARM_OK)
    {
        runtime->last_cloud_failure = NULL;
        alarm_cloud_diagnostic(runtime, "alarm-confirmed-and-saved", event_id, 0);
        message.kind = ALARM_MSG_EVENT_CONFIRMED;
        message.event_id = event_id;
        alarm_send_result_to_front(runtime, &message);
    }
    else if (result != ALARM_ERROR_STALE && result != ALARM_ERROR_REJECTED)
    {
        alarm_report_error(runtime, result);
    }
    else
    {
        alarm_cloud_diagnostic(runtime, result == ALARM_ERROR_STALE ? "reply-stale" : "alarm-rejected",
            frame.sequence, response);
    }
    return KAIWAN_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : alarm_collect_battery
* Description    : 单独读取本次业务电压，离线按键保存不等待模组身份查询
* Input          : runtime - 后台上下文
* Output         : 电池遥测及有效位
* Return         : 无
* Attention      : 调用前清理旧快照；不启动蜂窝网络
*******************************************************************************/
static void alarm_collect_battery(alarm_runtime_t *runtime)
{
    uint16_t millivolts;
    uint8_t percent;
    if (runtime->services->battery.read_voltage &&
        runtime->services->battery.read_voltage(runtime->services->battery.user, &millivolts))
    {
        runtime->telemetry.battery_mv = millivolts;
        runtime->telemetry.valid |= ALARM_TELEMETRY_VOLTAGE;
        if (runtime->services->battery.estimate_percent &&
            runtime->services->battery.estimate_percent(runtime->services->battery.user, millivolts, &percent) &&
            percent <= 100U)
        {
            runtime->telemetry.battery_percent = percent;
            runtime->telemetry.valid |= ALARM_TELEMETRY_PERCENT;
        }
    }
}

/*******************************************************************************
* Function Name  : alarm_collect_device_info
* Description    : 在后台更新身份和遥测，不阻塞声光
* Input          : runtime - 上下文；now - 调度时刻
* Output         : 最新有效快照
* Return         : 无
* Attention      : 百分比由器件按本次电压估算；采样失败清除有效位，UTC 仍须核验
*******************************************************************************/
static void alarm_collect_device_info(alarm_runtime_t *runtime, uint32_t now)
{
    device_info_t info;
    if (runtime->identity_ready && !runtime->telemetry_requested)
    {
        return;
    }
    if (!runtime->identity_ready && (uint32_t)(now - runtime->identity_since) < 5000U)
    {
        return;
    }
    runtime->identity_since = now;
    runtime->telemetry_requested = false;
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
    else
    {
        alarm_cloud_failure(runtime, "cloud-device-identity", ALARM_ERROR_NOT_READY);
    }
    alarm_collect_battery(runtime);
    runtime->telemetry.uptime_ms = alarm_now_ms(runtime);
}

/*******************************************************************************
* Function Name  : alarm_make_platform_topics
* Description    : 用本机 IMEI 及可配置后缀构造上下行主题
* Input          : runtime - 产品上下文
* Output         : 已校验身份及完整上下行主题
* Return         : true - 成功；false - 身份非法或主题过长
* Attention      : 格式串固定，宏只提供后缀；绝不使用样例设备的 IMEI
*******************************************************************************/
static bool alarm_make_platform_topics(alarm_runtime_t *runtime)
{
    int up_length;
    int down_length;
    if (kaiwan_cloud_make_platform_topics(&runtime->cloud, runtime->identity.imei) != KAIWAN_CLOUD_OK)
    {
        return false;
    }
    up_length = snprintf(runtime->cloud.platform_up_topic, sizeof(runtime->cloud.platform_up_topic),
        "iot/devices/%s%s", runtime->identity.imei, ALARM_BUTTON_UP_TOPIC_SUFFIX);
    down_length = snprintf(runtime->cloud.platform_down_topic, sizeof(runtime->cloud.platform_down_topic),
        "iot/devices/%s%s", runtime->identity.imei, ALARM_BUTTON_DOWN_TOPIC_SUFFIX);
    if (up_length <= 0 || (size_t)up_length >= sizeof(runtime->cloud.platform_up_topic) ||
        down_length <= 0 || (size_t)down_length >= sizeof(runtime->cloud.platform_down_topic))
    {
        runtime->cloud.platform_up_topic[0] = '\0';
        runtime->cloud.platform_down_topic[0] = '\0';
        alarm_cloud_failure(runtime, "cloud-topic-capacity", ALARM_ERROR_CONFIG);
        return false;
    }
    return true;
}

/*******************************************************************************
* Function Name  : alarm_start_cloud
* Description    : 身份齐备后配置独立MQTT实例
* Input          : runtime - 产品上下文
* Output         : 连接请求
* Return         : true已请求
* Attention      : 真实账号必需；未验证协议只有明确开启的明文联调模式允许尝试
*******************************************************************************/
static bool alarm_start_cloud(alarm_runtime_t *runtime)
{
    kaiwan_cloud_callbacks_t callbacks = {alarm_on_cloud_state, alarm_on_cloud_message,
                                      alarm_on_cloud_error, alarm_on_publish_result,
                                      runtime};
    int result;
    if (!runtime->cloud_config_valid || !runtime->identity_ready ||
        !alarm_make_platform_topics(runtime))
    {
        return false;
    }
    snprintf(runtime->cloud.client_id, sizeof(runtime->cloud.client_id), "KW-%s", runtime->identity.imei);
    result = runtime->transport->start(runtime->transport->user, &runtime->cloud, &callbacks);
    if (result != KAIWAN_CLOUD_OK)
    {
        alarm_cloud_failure(runtime, "cloud-mqtt-start", result);
        return false;
    }
    alarm_cloud_diagnostic(runtime, "mqtt-started", 0, 0);
    return true;
}

/*******************************************************************************
* Function Name  : alarm_retry_pending_save
* Description    : 按期限重试当前报警保存，成功后才向前台报告完成
* Input          : runtime - 后台上下文；now - 单调毫秒
* Output         : 持久队列、待保存状态与完成消息
* Return         : 无
* Attention      : 失败保留发生时快照；后续请求不得越过当前请求
*******************************************************************************/
static void alarm_retry_pending_save(alarm_runtime_t *runtime, uint32_t now)
{
    alarm_message_t done = {0};
    int result = ALARM_OK;
    if (!runtime->save_pending || (int32_t)(now - runtime->next_save_retry_ms) < 0)
    {
        return;
    }
    if (!runtime->storage_ready)
    {
        result = alarm_store_open(&runtime->store, ALARM_BUTTON_PRODUCT_ID, &runtime->services->storage);
        runtime->storage_ready = result == ALARM_OK;
        if (runtime->storage_ready)
        {
            /* 首次恢复成功前未保存新事件，因此此处仍是启动历史的边界。 */
            runtime->restored_maximum_id = runtime->store.image.next_id - 1U;
        }
    }
    if (runtime->storage_ready)
    {
        result = alarm_store_enqueue(&runtime->store, &runtime->pending_save.event, &done.event_id);
    }
    if (result != ALARM_OK)
    {
        runtime->pending_save.result = result;
        runtime->save_failed = true;
        runtime->background_waiting = false;
        runtime->next_save_retry_ms = alarm_now_ms(runtime) + ALARM_SAVE_RETRY_MS;
        alarm_report_error(runtime, result);
        return;
    }
    done.kind = ALARM_MSG_SAVE_RESULT;
    done.request = runtime->pending_save.request;
    done.result = ALARM_OK;
    runtime->save_pending = false;
    runtime->save_failed = false;
    if (runtime->last_fault == runtime->pending_save.result)
    {
        runtime->last_fault = ALARM_OK;
    }
    alarm_send_result_to_front(runtime, &done);
}

/*******************************************************************************
* Function Name  : alarm_handle_front_request
* Description    : 处理有效按下并先持久化，再允许发送
* Input          : runtime - 后台上下文；message - 前台请求
* Output         : 持久队列及保存结果
* Return         : 无
* Attention      : 调用方只在无待保存请求时取新消息；每次重按独立记录
*******************************************************************************/
static void alarm_handle_front_request(alarm_runtime_t *runtime, const alarm_message_t *message)
{
    alarm_event_t event;
    int64_t event_utc;
    if (message->kind == ALARM_MSG_WAKE)
    {
        runtime->background_waiting = false;
        /* 前台通知可能早于刚收到的业务确认，后台按最新计时起点复核。 */
        if (runtime->platform_registered && !runtime->heartbeat_needed &&
            (uint32_t)(alarm_now_ms(runtime) - runtime->last_heartbeat) >= ALARM_BUTTON_HEARTBEAT_MS)
        {
            runtime->heartbeat_needed = true;
            runtime->telemetry_requested = true;
        }
        return;
    }
    if (message->kind != ALARM_MSG_SAVE_EVENT)
    {
        return;
    }
    runtime->background_waiting = false;
    __atomic_store_n(&runtime->button_wake_pending, 1U, __ATOMIC_RELEASE);
    runtime->last_handled_request_id = message->request;
    runtime->telemetry_requested = true;
    if (runtime->services->system.identity &&
        (!runtime->offline_enabled || runtime->network_phase == ALARM_NETWORK_ONLINE))
    {
        alarm_collect_device_info(runtime, alarm_now_ms(runtime));
    }
    if (runtime->offline_enabled && runtime->network_phase != ALARM_NETWORK_ONLINE)
    {
        memset(&runtime->telemetry, 0, sizeof(runtime->telemetry));
        alarm_collect_battery(runtime);
        runtime->telemetry.uptime_ms = alarm_now_ms(runtime);
    }
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
    runtime->pending_save = *message;
    runtime->pending_save.event = event;
    runtime->pending_save.result = ALARM_OK;
    runtime->save_pending = true;
    runtime->next_save_retry_ms = alarm_now_ms(runtime);
    alarm_retry_pending_save(runtime, runtime->next_save_retry_ms);
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
    alarm_cloud_diagnostic(runtime, "offline-standby-enabled", runtime->offline_enabled, 0);
    alarm_cloud_diagnostic(runtime, "heartbeat-interval-ms", ALARM_BUTTON_HEARTBEAT_MS, 0);
    if (runtime->offline_enabled)
    {
        alarm_cloud_diagnostic(runtime, "network-attempt-ms", ALARM_BUTTON_NETWORK_ATTEMPT_MS, 0);
        alarm_cloud_diagnostic(runtime, "network-retry-sleep-ms", ALARM_BUTTON_NETWORK_RETRY_SLEEP_MS, 0);
    }
    runtime->identity_since = alarm_now_ms(runtime) - 5000U;
    alarm_reporter_init(&runtime->reporter, &runtime->store, alarm_publish_event, runtime,
                     ALARM_BUTTON_CONFIRMATION_TIMEOUT_MS, ALARM_BUTTON_RETRY_MINIMUM_MS,
                     ALARM_BUTTON_RETRY_MAXIMUM_MS);
    runtime->reporter.event_ready = alarm_event_is_ready;
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
* Function Name  : alarm_network_has_alarm
* Description    : 检查必须持续唤醒的报警或存储失败
* Input          : runtime - 后台上下文
* Output         : 无
* Return         : true - 不允许限时退出
* Attention      : 保存失败即使队列为空也阻止休眠
*******************************************************************************/
static bool alarm_network_has_alarm(alarm_runtime_t *runtime)
{
    return !runtime->storage_ready || runtime->save_pending || runtime->save_failed || runtime->control_storage_failed ||
           alarm_store_pending(&runtime->store) != 0 || runtime->reporter.inflight ||
           runtime->reporter.last_error != ALARM_OK;
}

/*******************************************************************************
* Function Name  : alarm_network_begin_attempt
* Description    : 启动一次恢复网络与控制业务尝试
* Input          : runtime - 后台；now - 毫秒
* Output         : 阶段及本轮起点
* Return         : 无
* Attention      : 保留注册成功、心跳起点及未完成报警
*******************************************************************************/
static void alarm_network_begin_attempt(alarm_runtime_t *runtime, uint32_t now)
{
    runtime->attempt_started = now;
    runtime->restart_requested = false;
    runtime->retry_sleep = false;
    runtime->background_waiting = false;
    runtime->cloud_stopping = false;
    runtime->network_phase = ALARM_NETWORK_RESTORING;
    runtime->services->system.radio_request(runtime->services->system.user, true, now);
    alarm_cloud_diagnostic(runtime, "network-restoring", now, 0);
}

/*******************************************************************************
* Function Name  : alarm_network_allow_processing
* Description    : 推进离线等待、射频恢复及安全关网
* Input          : runtime - 后台；now - 毫秒
* Output         : 网络状态和前台静止通知
* Return         : true - 本轮可推进 MQTT 业务
* Attention      : 关网失败持锁；所有阶段都处理按键竞争，不修改协议身份
*******************************************************************************/
static bool alarm_network_allow_processing(alarm_runtime_t *runtime, uint32_t now)
{
    system_interface_t *system = &runtime->services->system;
    bool wake;
    bool pending;
    bool complete;
    bool quiet;
    int error = 0;
    system_radio_state_t radio;
    alarm_message_t message = {0};
    if (!runtime->offline_enabled)
    {
        return true;
    }
    if (!system->radio_request || !system->radio_poll || !system->radio_state ||
        !system->radio_next_wait || !runtime->transport->stop)
    {
        runtime->background_waiting = false;
        alarm_cloud_failure(runtime, "offline-interface-missing", ALARM_ERROR_CONFIG);
        return false;
    }
    wake = __atomic_exchange_n(&runtime->button_wake_pending, 0U, __ATOMIC_ACQ_REL) != 0;
    if (wake && (runtime->network_phase == ALARM_NETWORK_STOPPING ||
                 runtime->network_phase == ALARM_NETWORK_RADIO_OFF))
    {
        runtime->restart_requested = true;
    }
    pending = alarm_network_has_alarm(runtime);
    quiet = __atomic_load_n(&runtime->front_quiet, __ATOMIC_ACQUIRE) &&
            !__atomic_load_n(&runtime->key_pending, __ATOMIC_ACQUIRE);
    if (runtime->network_phase == ALARM_NETWORK_SLEEP)
    {
        if (!wake && !pending && (int32_t)(now - runtime->offline_deadline) < 0)
        {
            runtime->background_waiting = true;
            return false;
        }
        runtime->network_phase = ALARM_NETWORK_START;
    }
    if (runtime->network_phase == ALARM_NETWORK_START)
    {
        alarm_network_begin_attempt(runtime, now);
    }
    if ((runtime->network_phase == ALARM_NETWORK_STOPPING ||
         runtime->network_phase == ALARM_NETWORK_RADIO_OFF) && (runtime->restart_requested || pending || !quiet))
    {
        /* 先完成 MQTT 停止，不能让旧连接回调混入新一轮。 */
        runtime->transport->poll(runtime->transport->user, now);
        if (!runtime->transport->stop(runtime->transport->user))
        {
            return false;
        }
        runtime->cloud_started = false;
        alarm_network_begin_attempt(runtime, alarm_now_ms(runtime));
    }
    system->radio_poll(system->user, now);
    now = alarm_now_ms(runtime);
    radio = system->radio_state(system->user, &error);
    if (radio == SYSTEM_RADIO_ERROR)
    {
        alarm_cloud_failure(runtime, "radio-transition", error);
    }
    if (runtime->network_phase == ALARM_NETWORK_RESTORING && radio == SYSTEM_RADIO_READY)
    {
        runtime->network_phase = ALARM_NETWORK_ONLINE;
        runtime->telemetry_requested = true;
        runtime->next_control_retry_ms = now;
        alarm_cloud_diagnostic(runtime, "network-ready", now, 0);
    }
    complete = runtime->platform_registered && !runtime->heartbeat_needed && !runtime->waiting_control_reply;
    if (runtime->network_phase == ALARM_NETWORK_ONLINE || runtime->network_phase == ALARM_NETWORK_RESTORING)
    {
        /* 到期业务先标记，再判断空闲，避免刚恢复网络便再次关闭。 */
        if (runtime->platform_registered && runtime->heartbeat_completed && !runtime->heartbeat_needed &&
            (uint32_t)(now - runtime->last_heartbeat) >= ALARM_BUTTON_HEARTBEAT_MS)
        {
            runtime->heartbeat_needed = true;
            runtime->telemetry_requested = true;
            complete = false;
        }
        runtime->retry_sleep = !complete && !pending &&
            (uint32_t)(now - runtime->attempt_started) >= ALARM_BUTTON_NETWORK_ATTEMPT_MS;
        if (!pending && quiet && (complete || runtime->retry_sleep))
        {
            runtime->network_phase = ALARM_NETWORK_STOPPING;
            runtime->cloud_stopping = true;
            runtime->background_waiting = false;
            runtime->stop_started = now;
            alarm_cloud_diagnostic(runtime, "network-stopping", now, runtime->retry_sleep);
        }
        else
        {
            /* 预算用尽但前台尚未静止时，只等待前台，不新发控制业务。 */
            return runtime->network_phase == ALARM_NETWORK_ONLINE && !runtime->retry_sleep;
        }
    }
    if (runtime->network_phase == ALARM_NETWORK_STOPPING)
    {
        bool stopped = !runtime->cloud_started || runtime->transport->stop(runtime->transport->user);
        if (runtime->cloud_started)
        {
            runtime->transport->poll(runtime->transport->user, now);
        }
        if (!stopped)
        {
            if ((uint32_t)(now - runtime->stop_started) >= ALARM_BUTTON_NETWORK_STOP_TIMEOUT_MS)
            {
                alarm_cloud_failure(runtime, "mqtt-stop-timeout", ALARM_ERROR_NOT_READY);
            }
            return false;
        }
        runtime->cloud_started = false;
        runtime->waiting_control_reply = false;
        runtime->network_phase = ALARM_NETWORK_RADIO_OFF;
        system->radio_request(system->user, false, now);
        return false;
    }
    if (runtime->network_phase == ALARM_NETWORK_RADIO_OFF && radio == SYSTEM_RADIO_OFF)
    {
        if (__atomic_load_n(&runtime->button_wake_pending, __ATOMIC_ACQUIRE) ||
            !__atomic_load_n(&runtime->front_quiet, __ATOMIC_ACQUIRE))
        {
            return false;
        }
        runtime->offline_deadline = runtime->retry_sleep ? now + ALARM_BUTTON_NETWORK_RETRY_SLEEP_MS :
                                                        runtime->last_heartbeat + ALARM_BUTTON_HEARTBEAT_MS;
        message.kind = ALARM_MSG_BACKGROUND_IDLE;
        message.request = runtime->last_handled_request_id;
        message.deadline_ms = runtime->offline_deadline;
        /* 投递失败不宣布静止，下轮重投，避免前后台期限失联。 */
        if (!system->queue_put(runtime->front_queue, &message, 0))
        {
            return false;
        }
        runtime->network_phase = ALARM_NETWORK_SLEEP;
        runtime->background_waiting = true;
        runtime->cloud_stopping = false;
        alarm_cloud_diagnostic(runtime, "offline-wait", runtime->offline_deadline, 0);
        alarm_cloud_diagnostic(runtime, "network-stop-duration", now - runtime->stop_started, 0);
    }
    return false;
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
    alarm_retry_pending_save(runtime, now);
    if (!runtime->storage_ready)
    {
        return;
    }
    if (!alarm_network_allow_processing(runtime, now))
    {
        return;
    }
    alarm_collect_device_info(runtime, alarm_now_ms(runtime));
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
    /* 下行回调可能用较新的时间更新确认起点，不能用 poll 前的旧时间计算差值。 */
    now = alarm_now_ms(runtime);
    if (runtime->transport->online(runtime->transport->user))
    {
        if (runtime->platform_registered && !runtime->heartbeat_needed &&
            (uint32_t)(now - runtime->last_heartbeat) >= ALARM_BUTTON_HEARTBEAT_MS)
        {
            runtime->heartbeat_needed = true;
            runtime->telemetry_requested = true;
            alarm_collect_device_info(runtime, now);
        }
        alarm_send_registration_or_heartbeat(runtime, now);
        alarm_reporter_poll(&runtime->reporter,
            runtime->platform_registered && runtime->heartbeat_completed && !runtime->waiting_control_reply, now);
    }
    else
    {
        alarm_reporter_poll(&runtime->reporter, false, now);
    }
    if (runtime->reporter.last_error != ALARM_OK && runtime->reporter.last_error != ALARM_ERROR_REJECTED)
    {
        alarm_report_error(runtime, runtime->reporter.last_error);
    }
    if (runtime->offline_enabled)
    {
        (void)alarm_network_allow_processing(runtime, alarm_now_ms(runtime));
        return;
    }
    /* 在线兼容配置只暂停应用轮询，SDK 继续持有连接和保活。 */
    if (runtime->platform_registered && runtime->transport->online(runtime->transport->user) &&
        !runtime->heartbeat_needed && !runtime->waiting_control_reply && !runtime->save_failed &&
        alarm_store_pending(&runtime->store) == 0 && !runtime->reporter.inflight &&
        runtime->reporter.last_error == ALARM_OK &&
        (!runtime->transport->next_wait ||
         runtime->transport->next_wait(runtime->transport->user, now) == SYSTEM_WAIT_FOREVER))
    {
        if (!runtime->background_waiting)
        {
            message.kind = ALARM_MSG_BACKGROUND_IDLE;
            message.request = runtime->last_handled_request_id;
            alarm_send_result_to_front(runtime, &message);
        }
        runtime->background_waiting = true;
    }
    else
    {
        runtime->background_waiting = false;
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
    uint32_t now = alarm_now_ms(runtime);
    uint32_t wait_ms = 20U;
    bool received;
    system_interface_t *system = &runtime->services->system;
    if (runtime->save_pending)
    {
        /* 当前请求失败时不取走后续请求；网络轮询仍由本轮后台处理继续推进。 */
        if (system->power_background_hold)
        {
            system->power_background_hold(system->user, true);
        }
        system->delay(wait_ms);
        return;
    }
    if (runtime->background_waiting && runtime->transport->set_notify && runtime->transport->next_wait)
    {
        uint32_t heartbeat_deadline = runtime->last_heartbeat + ALARM_BUTTON_HEARTBEAT_MS;
        uint32_t heartbeat_wait = (int32_t)(heartbeat_deadline - now) <= 0 ? 0U : heartbeat_deadline - now;
        wait_ms = runtime->transport->next_wait(runtime->transport->user, now);
        if (heartbeat_wait < wait_ms)
        {
            wait_ms = heartbeat_wait;
        }
    }
    if (runtime->offline_enabled)
    {
        if (runtime->network_phase == ALARM_NETWORK_SLEEP)
        {
            wait_ms = (int32_t)(runtime->offline_deadline - now) <= 0 ? 0U : runtime->offline_deadline - now;
        }
        else if (runtime->network_phase == ALARM_NETWORK_RESTORING ||
                 runtime->network_phase == ALARM_NETWORK_RADIO_OFF)
        {
            wait_ms = runtime->services->system.radio_next_wait ?
                runtime->services->system.radio_next_wait(runtime->services->system.user, now) : 1000U;
            if (wait_ms > 1000U)
            {
                wait_ms = 1000U;
            }
        }
        else if (runtime->network_phase == ALARM_NETWORK_STOPPING)
        {
            wait_ms = 100U;
        }
        if (__atomic_load_n(&runtime->button_wake_pending, __ATOMIC_ACQUIRE))
        {
            wait_ms = 0;
        }
    }
    if (__atomic_load_n(&runtime->network_pending, __ATOMIC_ACQUIRE))
    {
        wait_ms = 0;
    }
    if (system->power_background_hold)
    {
        system->power_background_hold(system->user, !runtime->background_waiting);
    }
    received = system->queue_get(runtime->background_queue, &message, wait_ms);
    if (system->power_background_hold)
    {
        system->power_background_hold(system->user, true);
    }
    (void)__atomic_exchange_n(&runtime->network_pending, 0U, __ATOMIC_ACQ_REL);
    if (received)
    {
        alarm_handle_front_request(runtime, &message);
    }
    for (drained = 0;
         drained < 7 && !runtime->save_pending &&
         runtime->services->system.queue_get(runtime->background_queue, &message, 0); ++drained)
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
        if (runtime->services->system.power_background_hold)
        {
            runtime->services->system.power_background_hold(runtime->services->system.user, true);
        }
        if (!initialized)
        {
            alarm_init_background(runtime);
            initialized = true;
        }
        alarm_read_front_requests(runtime);
        alarm_process_background(runtime, alarm_now_ms(runtime));
        if (ALARM_BUTTON_SLEEP_DIAGNOSTICS && runtime->services->system.power_diagnostic)
        {
            runtime->services->system.power_diagnostic(runtime->services->system.user);
        }
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
* Function Name  : alarm_set_led
* Description    : 把前台 LED 状态交给 LED 器件
* Input          : user - 上下文；on - 目标状态
* Output         : LED 输出
* Return         : true成功
* Attention      : 只有前台调用
*******************************************************************************/
static bool alarm_set_led(void *user, bool on)
{
    alarm_runtime_t *runtime = user;
    return runtime->services->led.set(runtime->services->led.user, on);
}

/*******************************************************************************
* Function Name  : alarm_set_buzzer
* Description    : 把前台蜂鸣器状态交给蜂鸣器器件
* Input          : user - 上下文；on - 目标状态
* Output         : 蜂鸣器输出
* Return         : true成功
* Attention      : 只有前台调用
*******************************************************************************/
static bool alarm_set_buzzer(void *user, bool on)
{
    alarm_runtime_t *runtime = user;
    return runtime->services->buzzer.set(runtime->services->buzzer.user, on);
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
    __atomic_store_n(&runtime->key_pending, 1U, __ATOMIC_RELEASE);
    __atomic_store_n(&runtime->front_quiet, 0U, __ATOMIC_RELEASE);
    __atomic_store_n(&runtime->button_wake_pending, 1U, __ATOMIC_RELEASE);
    if (runtime->offline_enabled)
    {
        message.kind = ALARM_MSG_NETWORK_EVENT;
        (void)runtime->services->system.queue_put(runtime->background_queue, &message, 0);
    }
    message.kind = ALARM_MSG_WAKE;
    (void)runtime->services->system.queue_put(runtime->front_queue, &message, 0);
}

/*******************************************************************************
* Function Name  : alarm_notify_network_event
* Description    : SDK 回调只标记事件并唤醒后台任务
* Input          : user - 运行上下文
* Output         : 原子通知和零等待消息
* Return         : 无
* Attention      : 队列满时已有消息会唤醒后台；原子标志防止丢失待处理事件
*******************************************************************************/
static void alarm_notify_network_event(void *user)
{
    alarm_runtime_t *runtime = user;
    alarm_message_t message = {0};
    __atomic_store_n(&runtime->network_pending, 1U, __ATOMIC_RELEASE);
    message.kind = ALARM_MSG_NETWORK_EVENT;
    (void)runtime->services->system.queue_put(runtime->background_queue, &message, 0);
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
            if (runtime->offline_enabled)
            {
                runtime->next_heartbeat_ms = message->deadline_ms;
            }
            if (runtime->front_background_error != ALARM_OK &&
                runtime->button_state.last_error == runtime->front_background_error)
            {
                runtime->button_state.last_error = ALARM_OK;
            }
            runtime->front_background_error = ALARM_OK;
        }
        break;
    case ALARM_MSG_HEARTBEAT_CONFIRMED:
        runtime->next_heartbeat_ms = now + ALARM_BUTTON_HEARTBEAT_MS;
        break;
    case ALARM_MSG_BACKGROUND_ERROR:
        runtime->front_background_error = message->result;
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
    if (runtime->services->key.read(runtime->services->key.user, &pressed))
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
* Attention      : 唤醒未配置或业务未静止时保持工作锁
*******************************************************************************/
static void alarm_wait_for_next_event(alarm_runtime_t *runtime, uint32_t now)
{
    system_interface_t *system = &runtime->services->system;
    alarm_message_t message;
    bool sleep_allowed = ALARM_BUTTON_SLEEP_ENABLED && runtime->services->key.wake_configured &&
                         runtime->services->key.set_wakeup && system->power_background_hold &&
                         runtime->transport->set_notify && runtime->transport->next_wait &&
                         alarm_button_can_sleep(&runtime->button_state);
    uint32_t wait_ms = 5U;
    if (sleep_allowed)
    {
        wait_ms = (int32_t)(runtime->next_heartbeat_ms - now) <= 0 ? 0U : runtime->next_heartbeat_ms - now;
    }
    if (__atomic_load_n(&runtime->key_pending, __ATOMIC_ACQUIRE))
    {
        sleep_allowed = false;
        wait_ms = 0;
    }
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
* Attention      : 前台静止且唤醒已配置时释放前台需求，后台独立持锁
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
        (void)__atomic_exchange_n(&runtime->key_pending, 0U, __ATOMIC_ACQ_REL);
        alarm_process_button(runtime, now);
        if (runtime->offline_enabled)
        {
            bool quiet = !runtime->button_state.key.stable && !runtime->button_state.key.candidate &&
                         !runtime->button_state.indicator.active && !runtime->button_state.pending_save_count &&
                         (runtime->button_state.last_error == ALARM_OK ||
                          runtime->button_state.last_error == runtime->front_background_error);
            uint32_t previous = __atomic_exchange_n(&runtime->front_quiet, quiet ? 1U : 0U, __ATOMIC_ACQ_REL);
            if (quiet && !previous)
            {
                alarm_notify_network_event(runtime);
            }
        }
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
    if (!services || !services->key.ready || !services->led.ready || !services->buzzer.ready ||
        !services->battery.ready || !services->transport)
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
    runtime->offline_enabled = ALARM_BUTTON_OFFLINE_STANDBY != 0;
    runtime->background_queue = services->system.queue_create(ALARM_BACKGROUND_QUEUE_CAPACITY, sizeof(alarm_message_t));
    runtime->front_queue = services->system.queue_create(ALARM_FRONT_QUEUE_CAPACITY, sizeof(alarm_message_t));
    callbacks.user = runtime;
    callbacks.set_led = alarm_set_led;
    callbacks.set_buzzer = alarm_set_buzzer;
    callbacks.fault = alarm_report_front_error;
    callbacks.submit_event = alarm_queue_save_request;
    if (!runtime->background_queue || !runtime->front_queue ||
        alarm_button_init(&runtime->button_state, &config, &callbacks) != ALARM_OK)
    {
        services->system.fault("alarm-initialize", ALARM_ERROR_NOT_READY);
        return false;
    }
    if (ALARM_BUTTON_SLEEP_ENABLED && services->key.set_wakeup)
    {
        services->key.set_wakeup(services->key.user, alarm_notify_key_wakeup, runtime);
    }
    if (runtime->offline_enabled && services->system.radio_set_notify)
    {
        services->system.radio_set_notify(services->system.user, alarm_notify_network_event, runtime);
    }
    if (runtime->transport->set_notify)
    {
        runtime->transport->set_notify(runtime->transport->user, alarm_notify_network_event, runtime);
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
