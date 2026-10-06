/*------------------------------------------includes--------------------------------------------*/
#include "test_support.h"
#include <stdint.h>
#ifndef __atomic_exchange_n
static uint32_t exchange_runtime_flag(uint32_t *address, uint32_t value);
#define __atomic_exchange_n(address, value, order) exchange_runtime_flag(address, value)
#define TEST_RUNTIME_EXCHANGE_FLAG 1
#endif
#ifdef TEST_PLAINTEXT_MODE
#define TEST_CLOUD_CONFIGURED 1
#define ALARM_BUTTON_CLOUD_ENABLED 1
#define ALARM_BUTTON_ENCRYPTION_ENABLED 0
#define ALARM_BUTTON_PLAINTEXT_TRIAL 1
#define ALARM_BUTTON_UNKNOWN_TELEMETRY_TRIAL 1
#define ALARM_BUTTON_PACKET_LOG_ENABLED 1
#define ALARM_BUTTON_MANUFACTURER_ID 0x4872U
#define ALARM_BUTTON_BROKER_HOST "broker.invalid"
#define ALARM_BUTTON_MQTT_USERNAME "test-account"
#define ALARM_BUTTON_MQTT_PASSWORD "test-password"
#define ALARM_BUTTON_UP_TOPIC_SUFFIX "/test/plain/up"
#define ALARM_BUTTON_DOWN_TOPIC_SUFFIX "/test/plain/down"
#endif
#include "../../src/alarm_button/alarm_runtime.c"
#include "ml307y/alarm_battery.h"
/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
static alarm_runtime_t runtime;
static product_services_t services;
static kaiwan_transport_t transport;
static alarm_storage_image_t disk_image;
static bool disk_exists;
static bool fail_write;
static bool fail_read;
static uint32_t now_ms;
static alarm_message_t submitted[8];
static unsigned submitted_count;
static unsigned submitted_head;
static unsigned recovery_delays;
static unsigned published;
static uint16_t wire_sequence;
static uint8_t wire_command;
static uint8_t wire_type;
static uint8_t wire_event;
static uint8_t wire_voltage;
static uint8_t wire_percent;
extern uint32_t alarm_mock_battery_millivolts;
extern int alarm_mock_battery_error;
static bool invalid_estimate_succeeds;
static unsigned faults;
static bool missing_manufacturer_reported;
static bool missing_factory_code_reported;
static bool missing_percent_reported;
static bool registration_rejected_reported;
static bool registration_timeout_reported;
static unsigned registration_confirmations;
static unsigned heartbeat_confirmations;
static unsigned alarm_confirmations;
static bool led_on;
static unsigned error_posts;
static unsigned queue_count;
static unsigned thread_count;
static unsigned queue_limit = 2;
static unsigned thread_limit = 2;
static alarm_runtime_t *startup_context;
static bool fail_publish;
static unsigned packet_logs;
static unsigned registration_logs;
static unsigned heartbeat_logs;
static unsigned alarm_logs;
static unsigned failed_packet_logs;
static char transmitted[KAIWAN_PROTOCOL_MAXIMUM_JSON_SIZE];
static size_t transmitted_length;

/*-------------------------------------------function---------------------------------------------*/
#ifdef TEST_RUNTIME_EXCHANGE_FLAG
/*******************************************************************************
* Function Name  : exchange_runtime_flag
* Description    : 在单线程模拟器中交换后台通知标志
* Input          : address - 标志地址；value - 新值
* Output         : 标志内容
* Return         : 旧值
* Attention      : 只替代模拟器缺失指令，不验证实机原子性
*******************************************************************************/
static uint32_t exchange_runtime_flag(uint32_t *address, uint32_t value)
{
    uint32_t previous = *address;
    *address = value;
    return previous;
}
#endif
/*******************************************************************************
* Function Name  : read_store
* Description    : 恢复内存介质
* Input          : user/data/size - 参数
* Output         : data
* Return         : 空白或正常
* Attention      : 模拟完整产品镜像
*******************************************************************************/
static int read_store(void *user, void *data, size_t size)
{
    (void)user;
    assert(size == sizeof(disk_image));
    if (fail_read)
    {
        return STORAGE_IO_ERROR;
    }
    if (!disk_exists)
    {
        return STORAGE_EMPTY;
    }
    memcpy(data, &disk_image, size);
    return STORAGE_OK;
}

/*******************************************************************************
* Function Name  : write_store
* Description    : 注入业务回执删除失败
* Input          : user/data/size - 参数
* Output         : 介质
* Return         : 写入结果
* Attention      : 失败不修改原镜像
*******************************************************************************/
static bool write_store(void *user, const void *data, size_t size)
{
    (void)user;
    assert(size == sizeof(disk_image));
    if (fail_write)
    {
        return false;
    }
    memcpy(&disk_image, data, size);
    disk_exists = true;
    return true;
}

/*******************************************************************************
* Function Name  : clock_now
* Description    : 返回可控毫秒
* Input          : user - 保留
* Output         : 无
* Return         : 毫秒
* Attention      : 单线程测试
*******************************************************************************/
static uint32_t clock_now(void *user)
{
    (void)user;
    return now_ms;
}

/*******************************************************************************
* Function Name  : fault
* Description    : 记录明确错误
* Input          : module/error - 错误
* Output         : 计数
* Return         : 无
* Attention      : 不联系硬件
*******************************************************************************/
static void fault(const char *module, int error)
{
    missing_manufacturer_reported |= strcmp(module, "cloud-config-manufacturer") == 0 && error == ALARM_ERROR_CONFIG;
    missing_factory_code_reported |= strcmp(module, "cloud-config-factory-code") == 0 && error == ALARM_ERROR_CONFIG;
    missing_percent_reported |= strcmp(module, "cloud-telemetry-percent") == 0 && error == ALARM_ERROR_NOT_READY;
    registration_rejected_reported |= strcmp(module, "cloud-registration-rejected") == 0 && error == 3;
    registration_timeout_reported |= strcmp(module, "cloud-registration-timeout") == 0 && error == ALARM_ERROR_NOT_READY;
    ++faults;
}

/*******************************************************************************
* Function Name  : diagnostic
* Description    : 分别计数注册、心跳和持久删除完成的业务确认
* Input          : stage - 固定阶段；value - 关联编号；result - 结果
* Output         : 各类确认计数
* Return         : 无
* Attention      : 入队和 PUBACK 日志不会增加任何业务确认计数
*******************************************************************************/
static void diagnostic(const char *stage, uint32_t value, int result)
{
    (void)value;
    if (result == 0)
    {
        registration_confirmations += strcmp(stage, "registration-confirmed") == 0;
        heartbeat_confirmations += strcmp(stage, "heartbeat-confirmed") == 0;
        alarm_confirmations += strcmp(stage, "alarm-confirmed-and-saved") == 0;
    }
}

/*******************************************************************************
* Function Name  : random_bytes
* Description    : 为测试报文提供确定性IV
* Input          : data/size - 输出
* Output         : data
* Return         : true
* Attention      : 仅测试，不用于固件
*******************************************************************************/
static bool random_bytes(uint8_t *data, size_t size)
{
#ifdef TEST_PLAINTEXT_MODE
    assert(false); /* 明文收发不应依赖 AES 随机 IV。 */
#endif
    memset(data, 0x5a, size);
    return true;
}

/*******************************************************************************
* Function Name  : queue_put
* Description    : 保持提交顺序并分派前台完成消息
* Input          : queue/message/wait - 队列参数
* Output         : 测试请求列表或前台状态
* Return         : 成功
* Attention      : 无真实线程并发
*******************************************************************************/
static bool queue_put(void *queue, const void *message, uint32_t wait)
{
    const alarm_message_t *m = message;
    (void)wait;
    if (queue == (void *)1)
    {
        assert(submitted_count < 8);
        submitted[submitted_count++] = *m;
    }
    else
    {
        if (m->kind == ALARM_MSG_BACKGROUND_ERROR)
        {
            ++error_posts;
        }
        alarm_handle_background_result(&runtime, m, now_ms);
    }
    return true;
}

/*******************************************************************************
* Function Name  : set_led
* Description    : 捕获指示灯
* Input          : user - 保留；on - 输出
* Output         : led_on
* Return         : true
* Attention      : 不操作GPIO
*******************************************************************************/
static bool set_led(void *user, bool on)
{
    (void)user;
    led_on = on;
    return true;
}

/*******************************************************************************
* Function Name  : get_recovery_request
* Description    : 按实际后台循环从模拟队列取出请求
* Input          : queue/message/wait - 队列参数
* Output         : message及读取位置
* Return         : 是否取到消息
* Attention      : 等待仅推进模拟时间，不运行真实线程
*******************************************************************************/
static bool get_recovery_request(void *queue, void *message, uint32_t wait)
{
    assert(queue == (void *)1);
    if (submitted_head < submitted_count)
    {
        *(alarm_message_t *)message = submitted[submitted_head++];
        return true;
    }
    now_ms += wait;
    return false;
}

/*******************************************************************************
* Function Name  : delay_recovery_task
* Description    : 核验保存失败时后台任务持续阻塞而非忙循环
* Input          : milliseconds - 延时
* Output         : 模拟时钟与延时计数
* Return         : 无
* Attention      : 不等待墙上时间
*******************************************************************************/
static void delay_recovery_task(uint32_t milliseconds)
{
    assert(milliseconds > 0);
    ++recovery_delays;
    now_ms += milliseconds;
}

/*******************************************************************************
* Function Name  : set_buzzer
* Description    : 模拟蜂鸣器输出
* Input          : user - 保留；on - 输出
* Output         : 无
* Return         : true
* Attention      : 不操作GPIO
*******************************************************************************/
static bool set_buzzer(void *user, bool on)
{
    (void)user;
    (void)on;
    return true;
}

/*******************************************************************************
* Function Name  : publish
* Description    : 解码实际上行并检查发送前已落盘
* Input          : user/topic/payload/size/qos/retain/cookie - MQTT参数
* Output         : 上行字段
* Return         : 成功
* Attention      : PUBACK不隐式生成业务确认
*******************************************************************************/
static kaiwan_cloud_result_t publish(void *user, const char *topic, const uint8_t *payload, size_t size,
                                 uint8_t qos, bool retain, uint32_t cookie)
{
    uint8_t decoded[KAIWAN_PROTOCOL_MAXIMUM_FRAME_SIZE];
    kaiwan_frame_view_t frame;
#ifdef TEST_PLAINTEXT_MODE
    size_t decoded_length;
#else
    kaiwan_protocol_workspace_t workspace;
#endif
    (void)user;
    (void)topic;
    (void)qos;
    (void)retain;
    (void)cookie;
#ifdef TEST_PLAINTEXT_MODE
    assert(size >= 12 && memcmp(payload, "57544B364872", 12) == 0);
    assert(kaiwan_protocol_hex_decode((const char *)payload, size, decoded,
        sizeof(decoded), &decoded_length) == KAIWAN_OK);
    assert(kaiwan_protocol_parse_frame(&runtime.protocol, decoded, decoded_length, &frame) == KAIWAN_OK);
    assert(size == frame.raw_length * 2U);
    if (frame.command == KAIWAN_COMMAND_REGISTER && !(runtime.telemetry.valid & ALARM_TELEMETRY_PERCENT))
    {
        assert(frame.data_length == 55 && frame.data[53] == 0xff);
    }
#else
    assert(kaiwan_session_decode(&runtime.protocol, &workspace, (const char *)payload, size, decoded,
                             sizeof(decoded), &frame) == KAIWAN_OK);
#endif
    assert(disk_exists && disk_image.next_sequence > frame.sequence);
    wire_sequence = frame.sequence;
    wire_command = frame.command;
    wire_type = frame.data[0];
    wire_event = frame.data[16];
    wire_voltage = frame.data[frame.command == KAIWAN_COMMAND_REGISTER ? 51U : 17U];
    wire_percent = frame.data[frame.command == KAIWAN_COMMAND_REGISTER ? 53U : 19U];
    assert(size < sizeof(transmitted));
    memcpy(transmitted, payload, size);
    transmitted_length = size;
    ++published;
    return fail_publish ? KAIWAN_CLOUD_ERROR_STATE : KAIWAN_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : packet_log
* Description    : 验证打印内容与真正提交 MQTT 的字节逐一相同
* Input          : kind/sequence/topic - 报文信息；payload/size - 文本；result - 入队结果
* Output         : 报文类型及失败计数
* Return         : 无
* Attention      : 不用重新构造的示例报文代替真实发送缓冲
*******************************************************************************/
static void packet_log(const char *kind, uint16_t sequence, const char *topic,
                       const uint8_t *payload, size_t size, int result)
{
    assert(sequence == wire_sequence && size == transmitted_length);
    assert(memcmp(payload, transmitted, size) == 0);
    assert(strcmp(topic, runtime.cloud.platform_up_topic) == 0);
    assert(result == (fail_publish ? KAIWAN_CLOUD_ERROR_STATE : KAIWAN_CLOUD_OK));
    ++packet_logs;
    registration_logs += strcmp(kind, "registration") == 0;
    heartbeat_logs += strcmp(kind, "heartbeat") == 0;
    alarm_logs += strcmp(kind, "alarm") == 0;
    failed_packet_logs += result != KAIWAN_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : response
* Description    : 生成加密业务回执并走实际下行解析
* Input          : sequence/code - 确认对象与状态
* Output         : 产品持久队列和UI
* Return         : 无
* Attention      : 使用虚构测试密钥
*******************************************************************************/
static void response(uint16_t sequence, uint8_t code)
{
    uint8_t frame[KAIWAN_PROTOCOL_MAXIMUM_FRAME_SIZE];
#ifndef TEST_PLAINTEXT_MODE
    uint8_t iv[16] = {0};
    kaiwan_protocol_workspace_t workspace;
#endif
    char json[KAIWAN_PROTOCOL_MAXIMUM_JSON_SIZE];
    size_t length;
    size_t encoded;
    assert(kaiwan_protocol_build_frame(&runtime.protocol, sequence, KAIWAN_COMMAND_SERVER_RESPONSE, &code, 1,
                                   frame, sizeof(frame), &length) == KAIWAN_OK);
#ifdef TEST_PLAINTEXT_MODE
    assert(kaiwan_protocol_hex_encode(frame, length, json, sizeof(json), &encoded) == KAIWAN_OK);
#else
    assert(kaiwan_protocol_wrap_json(&runtime.protocol, &workspace, frame, length, iv, json,
                                 sizeof(json), &encoded) == KAIWAN_OK);
#endif
    assert(alarm_on_cloud_message("down", 4, (const uint8_t *)json, encoded, &runtime) == KAIWAN_CLOUD_OK);
}

#ifdef TEST_PLAINTEXT_MODE
/*******************************************************************************
* Function Name  : test_invalid_plaintext_replies
* Description    : 注入异厂商、异版本、错误 CRC、奇数 HEX 和错误主题
* Input          : sequence - 当前等待注册的序号
* Output         : 保持注册未确认且持久报警条数不变
* Return         : 无
* Attention      : 使用独立缓冲，坏回执不能绕过原有协议校验
*******************************************************************************/
static void test_invalid_plaintext_replies(uint16_t sequence)
{
    kaiwan_protocol_config_t protocol = runtime.protocol;
    uint8_t frame[32];
    uint8_t response_code = 0;
    char text[65];
    size_t frame_length;
    size_t text_length;
    unsigned variant;
    uint16_t count = runtime.store.image.count;
    for (variant = 0; variant < 5; ++variant)
    {
        protocol = runtime.protocol;
        protocol.manufacturer_id += variant == 0;
        protocol.protocol_version += variant == 1;
        assert(kaiwan_protocol_build_frame(&protocol, sequence, KAIWAN_COMMAND_SERVER_RESPONSE,
            &response_code, 1, frame, sizeof(frame), &frame_length) == KAIWAN_OK);
        if (variant == 2)
        {
            frame[frame_length - 4] ^= 1; /* CRC 最后一个字节。 */
        }
        assert(kaiwan_protocol_hex_encode(frame, frame_length, text, sizeof(text), &text_length) == KAIWAN_OK);
        assert(alarm_on_cloud_message(variant == 4 ? "oops" : "down", 4,
            (const uint8_t *)text, text_length - (variant == 3), &runtime) != KAIWAN_CLOUD_OK);
        assert(runtime.waiting_control_reply && !runtime.platform_registered);
        assert(runtime.store.image.count == count);
    }
}
#endif

/*******************************************************************************
* Function Name  : press
* Description    : 产生有效按下并执行一次后台保存
* Input          : time - 原始按下时间
* Output         : 报警及保存通知
* Return         : 无
* Attention      : 保持前台先亮灯再排队
*******************************************************************************/
static void press(uint32_t time)
{
    unsigned index = submitted_count;
    alarm_button_update(&runtime.button_state, false, time - 40);
    alarm_button_update(&runtime.button_state, false, time - 10);
    alarm_button_update(&runtime.button_state, true, time);
    now_ms = time + 30;
    alarm_button_update(&runtime.button_state, true, now_ms);
    assert(led_on && submitted_count == index + 1);
    assert(runtime.button_state.pending_save_count == 1);
    alarm_handle_front_request(&runtime, &submitted[index]);
    assert(runtime.button_state.pending_save_count == 0);
}

/*******************************************************************************
* Function Name  : create_queue
* Description    : 为启动失败测试分配逻辑队列
* Input          : count/bytes - 参数
* Output         : 计数
* Return         : 句柄
* Attention      : 不运行任务
*******************************************************************************/
static void *create_queue(unsigned count, unsigned bytes)
{
    (void)count;
    (void)bytes;
    ++queue_count;
    if (queue_count > queue_limit)
    {
        return NULL;
    }
    return queue_count == 1 ? (void *)1 : (void *)2;
}

/*******************************************************************************
* Function Name  : start_thread
* Description    : 按设定数量模拟任务创建成功或失败
* Input          : name/entry/argument/stack/foreground - 任务参数
* Output         : 计数
* Return         : 未超过成功数量时为 true
* Attention      : 仅验证错误通知路径
*******************************************************************************/
static bool start_thread(const char *name, void (*entry)(void *), void *argument, unsigned stack,
                         bool foreground)
{
    (void)name;
    (void)entry;
    startup_context = argument;
    (void)stack;
    (void)foreground;
    return ++thread_count <= thread_limit;
}

/*******************************************************************************
* Function Name  : test_configured_heartbeat
* Description    : 验证业务回执后配置周期到期唤醒及毫秒计数回卷
* Input          : 无
* Output         : 心跳唤醒请求断言
* Return         : 无
* Attention      : 使用可控时钟，不等待真实周期或连接平台
*******************************************************************************/
static void test_configured_heartbeat(void)
{
    alarm_runtime_t schedule = {0};
    alarm_message_t confirmation = {0};
    uint32_t confirmed_at = 1234U;
    uint32_t deadline;
    unsigned before = submitted_count;
    schedule.services = &services;
    schedule.background_queue = (void *)1;
    confirmation.kind = ALARM_MSG_HEARTBEAT_CONFIRMED;
    alarm_handle_background_result(&schedule, &confirmation, confirmed_at);
    deadline = confirmed_at + ALARM_BUTTON_HEARTBEAT_MS;
    assert(schedule.next_heartbeat_ms == deadline);
    alarm_wake_background_for_heartbeat(&schedule, deadline - 1U);
    assert(submitted_count == before);
    alarm_wake_background_for_heartbeat(&schedule, deadline);
    assert(submitted_count == before + 1U);
    assert(submitted[before].kind == ALARM_MSG_WAKE);
    assert(schedule.next_heartbeat_ms == deadline + ALARM_BUTTON_HEARTBEAT_MS);
    alarm_wake_background_for_heartbeat(&schedule, deadline);
    assert(submitted_count == before + 1U);
    confirmed_at = UINT32_MAX - 1234U;
    alarm_handle_background_result(&schedule, &confirmation, confirmed_at);
    deadline = confirmed_at + ALARM_BUTTON_HEARTBEAT_MS;
    assert(schedule.next_heartbeat_ms == deadline);
    alarm_wake_background_for_heartbeat(&schedule, deadline - 1U);
    assert(submitted_count == before + 1U);
    alarm_wake_background_for_heartbeat(&schedule, deadline);
    assert(submitted_count == before + 2U);
    assert(submitted[before + 1U].kind == ALARM_MSG_WAKE);
}

/*******************************************************************************
* Function Name  : test_registration_failures
* Description    : 验证未知电量、厂商拒绝和回执超时可定位且不会伪造注册成功
* Input          : 无
* Output         : 测试断言和模拟注册状态
* Return         : 无
* Attention      : 仅使用测试身份；PUBACK 不得代替注册业务回执
*******************************************************************************/
static void test_registration_failures(void)
{
    unsigned before = published;
    unsigned confirmed_before = registration_confirmations;
    uint32_t next_sequence = runtime.store.image.next_sequence;
    /* 独立模拟尚未注册成功的首次启动，不依赖断线清除业务状态。 */
    runtime.platform_registered = false;
    runtime.heartbeat_needed = true;
    alarm_on_cloud_state(true, &runtime);
    runtime.identity.unknown_telemetry_verified = false;
    runtime.telemetry.valid = ALARM_TELEMETRY_VOLTAGE | ALARM_TELEMETRY_CSQ;
    runtime.telemetry.battery_mv = 3700;
    runtime.telemetry.csq = 20;
    alarm_send_registration_or_heartbeat(&runtime, now_ms);
    assert(published == before && runtime.store.image.next_sequence == next_sequence);
    assert(missing_percent_reported && !runtime.platform_registered);
    runtime.identity.unknown_telemetry_verified = true;
    now_ms += ALARM_BUTTON_RETRY_MINIMUM_MS;
    alarm_send_registration_or_heartbeat(&runtime, now_ms);
    assert(published == before + 1 && runtime.waiting_control_reply);
    alarm_on_publish_result(ALARM_CONTROL_COOKIE | wire_sequence, KAIWAN_CLOUD_OK, &runtime);
    assert(!runtime.platform_registered && runtime.waiting_control_reply);
    assert(registration_confirmations == confirmed_before);
    response(wire_sequence, 3);
    assert(registration_rejected_reported && !runtime.platform_registered);
    assert(registration_confirmations == confirmed_before);
    now_ms += ALARM_BUTTON_RETRY_MINIMUM_MS;
    alarm_send_registration_or_heartbeat(&runtime, now_ms);
    assert(published == before + 2 && runtime.waiting_control_reply);
    now_ms += ALARM_BUTTON_CONFIRMATION_TIMEOUT_MS;
    alarm_send_registration_or_heartbeat(&runtime, now_ms);
    assert(registration_timeout_reported && !runtime.waiting_control_reply);
    assert(!runtime.platform_registered && runtime.store.image.count == 0);
    now_ms += ALARM_BUTTON_RETRY_MINIMUM_MS;
    alarm_send_registration_or_heartbeat(&runtime, now_ms);
    response(wire_sequence, 0);
    assert(runtime.platform_registered);
    assert(registration_confirmations == confirmed_before + 1);
    alarm_send_registration_or_heartbeat(&runtime, now_ms);
    assert(wire_command == KAIWAN_COMMAND_EVENT && wire_event == 1);
    response(wire_sequence, 0);
    assert(!runtime.heartbeat_needed);
    /* 已注册的本次运行重连后不重复注册，也不触发额外心跳。 */
    before = published;
    alarm_on_cloud_state(false, &runtime);
    assert(runtime.platform_registered && !runtime.heartbeat_needed);
    alarm_on_cloud_state(true, &runtime);
    alarm_send_registration_or_heartbeat(&runtime, now_ms);
    assert(published == before);
    assert(registration_confirmations == confirmed_before + 1);
    runtime.platform_registered = false;
    runtime.heartbeat_needed = true;
    alarm_on_cloud_state(true, &runtime);
    fail_publish = true;
    alarm_send_registration_or_heartbeat(&runtime, now_ms);
    assert(!runtime.platform_registered && !runtime.waiting_control_reply);
    fail_publish = false;
}

/*******************************************************************************
* Function Name  : test_pending_history_does_not_block_new_alarm
* Description    : 复现旧记录待补报时新报警被堵住，并验证匹配删除及写失败保护
* Input          : 无
* Output         : 发送计数、队列顺序、序号与持久状态断言
* Return         : 无
* Attention      : 旧记录无真实 UTC 且未启用重投策略，必须保留
*******************************************************************************/
static void test_pending_history_does_not_block_new_alarm(void)
{
    alarm_event_t event = runtime.telemetry;
    uint32_t old_id;
    uint32_t new_id;
    uint32_t next_sequence;
    unsigned before = published;
    event.event_type = 12;
    event.valid &= (uint8_t)~ALARM_TIME_UTC;
    event.utc_seconds = 0;
    assert(alarm_store_enqueue(&runtime.store, &event, &old_id) == ALARM_OK);
    runtime.restored_maximum_id = old_id;
    assert(alarm_store_enqueue(&runtime.store, &event, &new_id) == ALARM_OK);
    alarm_reporter_poll(&runtime.reporter, true, now_ms);
    assert(published == before + 1 && runtime.reporter.event_id == new_id);
    assert(runtime.store.image.count == 2 && runtime.store.image.events[0].id == old_id);
    fail_write = true;
    response(wire_sequence, 0);
    assert(runtime.store.image.count == 2 && disk_image.count == 2);
    fail_write = false;
    response(wire_sequence, 0);
    assert(runtime.store.image.count == 1 && runtime.store.image.events[0].id == old_id);
    assert(disk_image.count == 1 && disk_image.events[0].id == old_id);
    response(wire_sequence, 0);
    assert(runtime.store.image.count == 1);
    next_sequence = runtime.store.image.next_sequence;
    alarm_reporter_poll(&runtime.reporter, true, now_ms);
    assert(published == before + 1 && runtime.store.image.next_sequence == next_sequence);
}

/*******************************************************************************
* Function Name  : online_trial
* Description    : 模拟持续在线
* Input          : 测试参数
* Output         : 测试断言
* Return         : 模拟状态或无
* Attention      : 仅用于主机回归，不连接真实平台
*******************************************************************************/
static bool online_trial(void *user)
{
    (void)user;
    return true;
}

/*******************************************************************************
* Function Name  : poll_trial
* Description    : 模拟非阻塞轮询
* Input          : 测试参数
* Output         : 测试断言
* Return         : 模拟状态或无
* Attention      : 仅用于主机回归，不连接真实平台
*******************************************************************************/
static void poll_trial(void *user, uint32_t now)
{
    (void)user;
    (void)now;
}

/*******************************************************************************
* Function Name  : stop_trial
* Description    : 禁止业务空闲时主动断开
* Input          : 测试参数
* Output         : 测试断言
* Return         : 模拟状态或无
* Attention      : 仅用于主机回归，不连接真实平台
*******************************************************************************/
static bool stop_trial(void *user)
{
    (void)user;
    assert(false);
    return false;
}

/*******************************************************************************
* Function Name  : test_stay_online
* Description    : 验证注册完成且无待发事件仍持续在线
* Input          : 测试参数
* Output         : 测试断言
* Return         : 模拟状态或无
* Attention      : 仅用于主机回归，不连接真实平台
*******************************************************************************/
static void test_stay_online(void)
{
    alarm_runtime_t saved = runtime;
    kaiwan_transport_t saved_transport = transport;
    runtime.background_waiting = false;
    runtime.cloud_started = true;
    runtime.cloud_stopping = false;
    runtime.identity_ready = true;
    runtime.identity_since = now_ms;
    runtime.telemetry_requested = false;
    runtime.platform_registered = true;
    runtime.heartbeat_needed = false;
    runtime.waiting_control_reply = false;
    runtime.last_heartbeat = now_ms;
    transport.poll = poll_trial;
    transport.online = online_trial;
    transport.stop = stop_trial;
    assert(runtime.store.image.count == 0);
    alarm_process_background(&runtime, now_ms);
    assert(runtime.cloud_started && !runtime.cloud_stopping);
    assert(runtime.background_waiting); /* 保留连接时也必须能报告业务空闲。 */
    runtime = saved;
    transport = saved_transport;
}

/*******************************************************************************
* Function Name  : reconnect_identity
* Description    : 为周期心跳采样提供当前虚构设备身份
* Input          : info - 设备信息输出地址
* Output         : 与当前测试设备一致的身份
* Return         : true
* Attention      : 不读取模组或真实遥测
*******************************************************************************/
static bool reconnect_identity(device_info_t *info)
{
    memset(info, 0, sizeof(*info));
    memcpy(info->imei, runtime.identity.imei, sizeof(info->imei));
    memcpy(info->imsi, runtime.identity.imsi, sizeof(info->imsi));
    memcpy(info->iccid, runtime.identity.iccid, sizeof(info->iccid));
    return true;
}

/*******************************************************************************
* Function Name  : invalid_battery_estimate
* Description    : 注入估算失败或成功返回越界百分比的接口异常
* Input          : user/millivolts - 忽略；percent - 输出地址
* Output         : percent - 非法值 255
* Return         : 由测试选择成功或失败
* Attention      : 仅供回归，业务侧不得把非法输出标记为有效百分比
*******************************************************************************/
static bool invalid_battery_estimate(void *user, uint16_t millivolts, uint8_t *percent)
{
    (void)user;
    (void)millivolts;
    *percent = 255U;
    return invalid_estimate_succeeds;
}

/*******************************************************************************
* Function Name  : test_battery_collection
* Description    : 验证真实器件接口接入快照、失败清除旧值和协议字段
* Input          : 无
* Output         : 电压与百分比有效位、注册和事件字节断言
* Return         : 无
* Attention      : ADC 使用模拟值；采用默认估算端点，不证明实板电量精度
*******************************************************************************/
static void test_battery_collection(void)
{
    alarm_runtime_t sample = {0};
    product_services_t sample_services = {0};
    const uint16_t voltages[] = {2500U, ALARM_BATTERY_EMPTY_MILLIVOLTS,
        (ALARM_BATTERY_EMPTY_MILLIVOLTS + ALARM_BATTERY_FULL_MILLIVOLTS) / 2U, 4200U, 4500U};
    const uint8_t percentages[] = {0U, 0U, 50U, 100U, 100U};
    uint8_t body[55];
    unsigned index;
    sample.services = &sample_services;
    sample.identity_ready = true;
    sample_services.system.identity = reconnect_identity;
    sample_services.system.millis = clock_now;
    ml307y_alarm_battery_bind(&sample_services.battery);
    for (index = 0U; index < sizeof(voltages) / sizeof(voltages[0]); ++index)
    {
        alarm_mock_battery_millivolts = voltages[index];
        sample.telemetry_requested = true;
        alarm_collect_device_info(&sample, now_ms);
        assert((sample.telemetry.valid & (ALARM_TELEMETRY_VOLTAGE | ALARM_TELEMETRY_PERCENT)) ==
            (ALARM_TELEMETRY_VOLTAGE | ALARM_TELEMETRY_PERCENT));
        assert(sample.telemetry.battery_mv == voltages[index]);
        assert(sample.telemetry.battery_percent == percentages[index]);
        assert(kaiwan_handset_registration_payload(&runtime.identity, &sample.telemetry, body, sizeof(body)) == 55);
        assert(body[51] == (voltages[index] + 50U) / 100U && body[53] == percentages[index]);
        assert(kaiwan_handset_event_payload(&runtime.identity, &sample.telemetry, false, body, sizeof(body)) == 21);
        assert(body[17] == (voltages[index] + 50U) / 100U && body[19] == percentages[index]);
    }
    alarm_mock_battery_error = -7;
    sample.telemetry_requested = true;
    alarm_collect_device_info(&sample, now_ms);
    assert(!(sample.telemetry.valid & (ALARM_TELEMETRY_VOLTAGE | ALARM_TELEMETRY_PERCENT)));
    assert(sample.telemetry.battery_mv == 0U && sample.telemetry.battery_percent == 0U);
    assert(kaiwan_handset_registration_payload(&runtime.identity, &sample.telemetry, body, sizeof(body)) == 55);
    assert(body[51] == 0xff && body[53] == 0xff);
    assert(kaiwan_handset_event_payload(&runtime.identity, &sample.telemetry, false, body, sizeof(body)) == 21);
    assert(body[17] == 0xff && body[19] == 0xff);
    alarm_mock_battery_error = 0;
    alarm_mock_battery_millivolts = 3700U;
    sample.telemetry_requested = true;
    alarm_collect_device_info(&sample, now_ms);
    assert(sample.telemetry.battery_percent == (370000U - ALARM_BATTERY_EMPTY_MILLIVOLTS * 100U +
        (ALARM_BATTERY_FULL_MILLIVOLTS - ALARM_BATTERY_EMPTY_MILLIVOLTS) / 2U) /
        (ALARM_BATTERY_FULL_MILLIVOLTS - ALARM_BATTERY_EMPTY_MILLIVOLTS) && (sample.telemetry.valid & ALARM_TELEMETRY_PERCENT));
    sample_services.battery.estimate_percent = NULL;
    sample.telemetry_requested = true;
    alarm_collect_device_info(&sample, now_ms);
    assert((sample.telemetry.valid & ALARM_TELEMETRY_VOLTAGE) && !(sample.telemetry.valid & ALARM_TELEMETRY_PERCENT));
    sample_services.battery.estimate_percent = invalid_battery_estimate;
    for (index = 0U; index < 2U; ++index)
    {
        invalid_estimate_succeeds = index != 0U;
        sample.telemetry_requested = true;
        alarm_collect_device_info(&sample, now_ms);
        assert((sample.telemetry.valid & ALARM_TELEMETRY_VOLTAGE) && !(sample.telemetry.valid & ALARM_TELEMETRY_PERCENT));
    }
}

/*******************************************************************************
* Function Name  : test_reconnect_heartbeat
* Description    : 验证重连保留注册、周期期限、过期补发和心跳失败重试
* Input          : 无
* Output         : 实际发送报文及业务状态断言
* Return         : 无
* Attention      : 可控时钟覆盖回卷；不代表真实网络或实板功耗验证
*******************************************************************************/
static void test_reconnect_heartbeat(void)
{
    kaiwan_transport_t saved_transport = transport;
    alarm_message_t wake = {0};
    uint32_t confirmed_at = UINT32_MAX - 1234U;
    uint32_t deadline = confirmed_at + ALARM_BUTTON_HEARTBEAT_MS;
    unsigned before = published;
    unsigned confirmations = registration_confirmations;
    unsigned attempt;
    services.system.identity = reconnect_identity;
    transport.poll = poll_trial;
    transport.online = online_trial;
    transport.stop = stop_trial;
    runtime.cloud_started = true;
    runtime.identity_ready = true;
    runtime.telemetry_requested = false;
    runtime.last_heartbeat = confirmed_at;
    runtime.next_heartbeat_ms = deadline;
    runtime.platform_registered = true;
    runtime.heartbeat_needed = false;
    runtime.waiting_control_reply = false;
    for (attempt = 1; attempt <= 3; ++attempt)
    {
        now_ms = confirmed_at + attempt * 201000U;
        alarm_on_cloud_state(false, &runtime);
        alarm_on_cloud_state(true, &runtime);
        alarm_process_background(&runtime, now_ms);
        assert(runtime.platform_registered && !runtime.heartbeat_needed);
        assert(runtime.last_heartbeat == confirmed_at && runtime.next_heartbeat_ms == deadline);
        assert(published == before && registration_confirmations == confirmations);
    }
    now_ms = deadline - 1U;
    alarm_process_background(&runtime, now_ms);
    assert(published == before);
    alarm_on_cloud_state(false, &runtime);
    now_ms = deadline + 1000U;
    alarm_on_cloud_state(true, &runtime);
    alarm_process_background(&runtime, now_ms);
    assert(published == before + 1U && wire_command == KAIWAN_COMMAND_EVENT && wire_event == 1);
    assert(wire_voltage == 42U && wire_percent == 100U);
    assert(runtime.waiting_control_reply);
    response(wire_sequence, 0);
    assert(!runtime.heartbeat_needed && runtime.last_heartbeat == now_ms);
    /* 后台已收到心跳成功，前台先前排队的唤醒不能再创建一条心跳。 */
    wake.kind = ALARM_MSG_WAKE;
    alarm_handle_front_request(&runtime, &wake);
    alarm_process_background(&runtime, now_ms);
    assert(published == before + 1U && !runtime.heartbeat_needed);
    now_ms += ALARM_BUTTON_HEARTBEAT_MS;
    alarm_process_background(&runtime, now_ms);
    assert(published == before + 2U && runtime.waiting_control_reply);
    alarm_on_cloud_state(false, &runtime);
    alarm_on_cloud_state(true, &runtime);
    alarm_process_background(&runtime, now_ms);
    assert(published == before + 3U && runtime.platform_registered);
    assert(wire_command == KAIWAN_COMMAND_EVENT && wire_event == 1);
    now_ms += ALARM_BUTTON_CONFIRMATION_TIMEOUT_MS;
    alarm_process_background(&runtime, now_ms);
    assert(!runtime.waiting_control_reply && runtime.heartbeat_needed && !runtime.background_waiting);
    assert(published == before + 3U);
    now_ms += ALARM_BUTTON_RETRY_MINIMUM_MS;
    alarm_process_background(&runtime, now_ms);
    assert(published == before + 4U && runtime.waiting_control_reply);
    response(wire_sequence, 0);
    alarm_process_background(&runtime, now_ms);
    assert(runtime.background_waiting && !runtime.heartbeat_needed);
    assert(registration_confirmations == confirmations);
    transport = saved_transport;
}

/*******************************************************************************
* Function Name  : poll_heartbeat_confirmation
* Description    : 模拟网络事件处理期间时钟前进并收到心跳业务确认
* Input          : user - 保留；now - 进入网络处理时的旧时间
* Output         : 更新可控时钟并通过真实协议解析完成确认
* Return         : 无
* Attention      : 覆盖回调时间晚于外层循环时间的交错，不访问真实网络
*******************************************************************************/
static void poll_heartbeat_confirmation(void *user, uint32_t now)
{
    (void)user;
    assert(runtime.waiting_control_reply && runtime.heartbeat_needed);
    now_ms = now + 5U;
    response(wire_sequence, 0);
}

/*******************************************************************************
* Function Name  : test_heartbeat_callback_clock
* Description    : 验证回调推进时间后不重复心跳，且回卷和下一周期仍正常
* Input          : 无
* Output         : 发送次数、业务确认及下一期限断言
* Return         : 无
* Attention      : 普通时刻与 UINT32 回卷分别测试，不用固定时钟掩盖问题
*******************************************************************************/
static void test_heartbeat_callback_clock(void)
{
    kaiwan_transport_t saved_transport = transport;
    const uint32_t starts[] = {100000U, UINT32_MAX - 2U};
    unsigned index;
    unsigned before;
    uint32_t confirmed_at;
    for (index = 0U; index < 2U; ++index)
    {
        now_ms = starts[index];
        transport.poll = poll_trial;
        transport.online = online_trial;
        runtime.next_control_retry_ms = now_ms;
        runtime.heartbeat_needed = true;
        runtime.waiting_control_reply = false;
        runtime.last_heartbeat = now_ms - ALARM_BUTTON_HEARTBEAT_MS;
        before = published;
        alarm_process_background(&runtime, now_ms);
        assert(published == before + 1U && runtime.waiting_control_reply);
        transport.poll = poll_heartbeat_confirmation;
        alarm_process_background(&runtime, now_ms);
        confirmed_at = now_ms;
        assert(published == before + 1U);
        assert(!runtime.heartbeat_needed && !runtime.waiting_control_reply);
        assert(runtime.last_heartbeat == confirmed_at && runtime.background_waiting);
        transport.poll = poll_trial;
        now_ms = confirmed_at + ALARM_BUTTON_HEARTBEAT_MS - 1U;
        alarm_process_background(&runtime, now_ms);
        assert(published == before + 1U);
        ++now_ms;
        alarm_process_background(&runtime, now_ms);
        assert(published == before + 2U && runtime.waiting_control_reply);
        response(wire_sequence, 0);
    }
    transport = saved_transport;
}

/*******************************************************************************
* Function Name  : prepare_failure_runtime
* Description    : 为启动门槛与保存恢复测试建立独立运行场景
* Input          : 无
* Output         : 全局模拟上下文
* Return         : 无
* Attention      : 复用虚构协议配置，调用方负责恢复原测试场景
*******************************************************************************/
static void prepare_failure_runtime(void)
{
    kaiwan_protocol_config_t protocol = runtime.protocol;
    kaiwan_handset_identity_t identity = runtime.identity;
    kaiwan_cloud_config_t cloud = runtime.cloud;
    alarm_button_config_t config = alarm_button_default_config();
    alarm_button_callbacks_t callbacks = {&runtime, set_led, set_buzzer, NULL, alarm_queue_save_request};
    storage_interface_t storage = {NULL, read_store, write_store, NULL};
    memset(&runtime, 0, sizeof(runtime));
    runtime.protocol = protocol;
    runtime.identity = identity;
    runtime.cloud = cloud;
    runtime.services = &services;
    runtime.transport = &transport;
    runtime.front_queue = (void *)2;
    runtime.background_queue = (void *)1;
    runtime.cloud_started = true;
    runtime.identity_ready = true;
    services.storage = storage;
    disk_exists = false;
    fail_write = false;
    fail_read = false;
    fail_publish = false;
    submitted_count = 0;
    submitted_head = 0;
    recovery_delays = 0;
    now_ms = 100000U;
    assert(alarm_store_open(&runtime.store, ALARM_BUTTON_PRODUCT_ID, &storage) == ALARM_OK);
    runtime.storage_ready = true;
    assert(alarm_button_init(&runtime.button_state, &config, &callbacks) == ALARM_OK);
    alarm_reporter_init(&runtime.reporter, &runtime.store, alarm_publish_event, &runtime, 10000U, 5000U, 30000U);
    runtime.reporter.event_ready = alarm_event_is_ready;
    transport.poll = poll_trial;
    transport.online = online_trial;
}

/*******************************************************************************
* Function Name  : submit_recovery_press
* Description    : 产生有效重按但将保存工作留给真实后台处理函数
* Input          : time - 按下时间
* Output         : 请求队列及前台未保存计数
* Return         : 无
* Attention      : 可以连续提交多个尚未完成的请求
*******************************************************************************/
static void submit_recovery_press(uint32_t time)
{
    unsigned before = submitted_count;
    alarm_button_update(&runtime.button_state, false, time - 40U);
    alarm_button_update(&runtime.button_state, false, time - 10U);
    alarm_button_update(&runtime.button_state, true, time);
    now_ms = time + 30U;
    alarm_button_update(&runtime.button_state, true, now_ms);
    assert(submitted_count == before + 1U);
}

/*******************************************************************************
* Function Name  : test_save_failure_recovery
* Description    : 验证失败请求保留、顺序恢复、满队列腾空重试及初始读故障恢复
* Input          : 无
* Output         : 持久记录、前台状态与后台阻塞断言
* Return         : 无；断言失败终止测试
* Attention      : 保存失败不发送完成消息；成功回执仍按事件关联
*******************************************************************************/
static void test_save_failure_recovery(void)
{
    alarm_runtime_t saved_runtime = runtime;
    alarm_storage_image_t saved_disk = disk_image;
    product_services_t saved_services = services;
    kaiwan_transport_t saved_transport = transport;
    bool saved_exists = disk_exists;
    uint32_t saved_now = now_ms;
    uint32_t first_uptime;
    uint32_t second_uptime;
    uint32_t first_id;
    uint16_t sequence;
    unsigned index;
    prepare_failure_runtime();
    services.system.queue_get = get_recovery_request;
    services.system.delay = delay_recovery_task;
    runtime.cloud_started = false;
    fail_write = true;
    submit_recovery_press(now_ms + 100U);
    first_uptime = submitted[0].event.uptime_ms;
    alarm_read_front_requests(&runtime);
    assert(runtime.store.image.count == 0 && runtime.save_failed);
    assert(runtime.button_state.pending_save_count == 1);
    assert(runtime.button_state.last_completed_request_id == 0);
    submit_recovery_press(now_ms + 100U);
    second_uptime = submitted[1].event.uptime_ms;
    alarm_read_front_requests(&runtime);
    assert(submitted_head == 1 && recovery_delays == 1);
    assert(runtime.button_state.pending_save_count == 2);
    now_ms += 1000U;
    alarm_process_background(&runtime, now_ms);
    assert(runtime.save_failed && runtime.store.image.count == 0);
    assert(runtime.button_state.pending_save_count == 2);
    fail_write = false;
    now_ms += 1000U;
    alarm_process_background(&runtime, now_ms);
    assert(runtime.store.image.count == 1 && !runtime.save_failed);
    first_id = runtime.store.image.events[0].id;
    assert(runtime.store.image.events[0].uptime_ms == first_uptime);
    assert(runtime.button_state.pending_save_count == 1 && runtime.button_state.indicator.event_id == 0);
    alarm_read_front_requests(&runtime);
    assert(runtime.store.image.count == 2 && runtime.button_state.pending_save_count == 0);
    assert(runtime.store.image.events[1].uptime_ms == second_uptime);
    assert(runtime.button_state.indicator.event_id == runtime.store.image.events[1].id);
    alarm_button_on_alarm_confirmed(&runtime.button_state, first_id);
    assert(!runtime.button_state.indicator.acked);

    prepare_failure_runtime();
    services.system.queue_get = get_recovery_request;
    services.system.delay = delay_recovery_task;
    runtime.platform_registered = true;
    runtime.heartbeat_completed = true;
    runtime.last_heartbeat = now_ms;
    /* 构造有效满队列夹具，再通过生产序号提交接口写入真实CRC镜像。 */
    for (index = 0; index < ALARM_CAPACITY; ++index)
    {
        runtime.store.image.events[index].id = index + 1U;
        runtime.store.image.events[index].event_type = 0x0c;
    }
    runtime.store.image.count = ALARM_CAPACITY;
    runtime.store.image.next_id = ALARM_CAPACITY + 1U;
    assert(alarm_store_sequence(&runtime.store, &sequence) == ALARM_OK);
    submit_recovery_press(now_ms + 100U);
    alarm_read_front_requests(&runtime);
    assert(runtime.save_failed && runtime.button_state.pending_save_count == 1);
    alarm_process_background(&runtime, now_ms);
    assert(runtime.reporter.inflight);
    response(wire_sequence, 0);
    assert(runtime.store.image.count == ALARM_CAPACITY - 1U);
    now_ms += 1000U;
    alarm_process_background(&runtime, now_ms);
    assert(runtime.store.image.count == ALARM_CAPACITY && !runtime.save_failed);
    assert(runtime.button_state.pending_save_count == 0);
    assert(runtime.store.image.events[ALARM_CAPACITY - 1U].id == ALARM_CAPACITY + 1U);

    prepare_failure_runtime();
    services.system.queue_get = get_recovery_request;
    services.system.delay = delay_recovery_task;
    runtime.cloud_started = false;
    runtime.storage_ready = false;
    fail_read = true;
    submit_recovery_press(now_ms + 100U);
    alarm_read_front_requests(&runtime);
    assert(runtime.save_failed && runtime.button_state.pending_save_count == 1);
    fail_read = false;
    now_ms += 1000U;
    alarm_process_background(&runtime, now_ms);
    assert(runtime.storage_ready && !runtime.save_failed && runtime.store.image.count == 1);
    assert(runtime.button_state.pending_save_count == 0);
    runtime = saved_runtime;
    disk_image = saved_disk;
    services = saved_services;
    transport = saved_transport;
    disk_exists = saved_exists;
    now_ms = saved_now;
}

/*******************************************************************************
* Function Name  : test_first_heartbeat_alarm_gate
* Description    : 验证首次心跳各类失败与重试间隙都不能提前发送报警
* Input          : 无
* Output         : 实际报文、持久队列及确认门槛断言
* Return         : 无；断言失败终止测试
* Attention      : 必须先走注册成功；只有首次心跳业务成功才放行报警
*******************************************************************************/
static void test_first_heartbeat_alarm_gate(void)
{
    alarm_runtime_t saved_runtime = runtime;
    alarm_storage_image_t saved_disk = disk_image;
    product_services_t saved_services = services;
    kaiwan_transport_t saved_transport = transport;
    bool saved_exists = disk_exists;
    uint32_t saved_now = now_ms;
    unsigned scenario;
    unsigned before;
    for (scenario = 0; scenario < 4; ++scenario)
    {
        prepare_failure_runtime();
        press(now_ms + 100U);
        runtime.heartbeat_needed = true;
        alarm_process_background(&runtime, now_ms);
        assert(wire_command == KAIWAN_COMMAND_REGISTER && runtime.waiting_control_reply);
        response(wire_sequence, 0);
        assert(runtime.platform_registered && !runtime.heartbeat_completed);
        fail_publish = scenario == 3;
        alarm_process_background(&runtime, now_ms);
        assert(wire_command == KAIWAN_COMMAND_EVENT && wire_event == 1);
        before = published;
        fail_publish = false;
        if (scenario == 0)
        {
            now_ms += ALARM_BUTTON_CONFIRMATION_TIMEOUT_MS;
        }
        else if (scenario == 1)
        {
            response(wire_sequence, 3);
        }
        else if (scenario == 2)
        {
            alarm_on_publish_result(ALARM_CONTROL_COOKIE | wire_sequence, KAIWAN_CLOUD_ERROR_STATE, &runtime);
        }
        alarm_process_background(&runtime, now_ms);
        assert(published == before && !runtime.heartbeat_completed);
        assert(!runtime.reporter.inflight && runtime.store.image.count == 1);
        now_ms += ALARM_BUTTON_RETRY_MINIMUM_MS - 1U;
        alarm_process_background(&runtime, now_ms);
        assert(published == before && !runtime.reporter.inflight);
        ++now_ms;
        alarm_process_background(&runtime, now_ms);
        assert(published == before + 1U && wire_event == 1 && runtime.waiting_control_reply);
        response(wire_sequence, 0);
        assert(runtime.heartbeat_completed);
        alarm_process_background(&runtime, now_ms);
        assert(published == before + 2U && wire_event == 0x0c && runtime.reporter.inflight);
        alarm_on_cloud_state(false, &runtime);
        alarm_on_cloud_state(true, &runtime);
        assert(runtime.platform_registered && runtime.heartbeat_completed);
    }
    runtime = saved_runtime;
    disk_image = saved_disk;
    services = saved_services;
    transport = saved_transport;
    disk_exists = saved_exists;
    now_ms = saved_now;
}

/*******************************************************************************
* Function Name  : main
* Description    : 覆盖注册心跳、重按、业务回执与失败保留
* Input          : 无
* Output         : 断言
* Return         : 0通过
* Attention      : 原时序测试另行覆盖所有边界
*******************************************************************************/
int main(void)
{
    alarm_button_config_t config = alarm_button_default_config();
    alarm_button_callbacks_t callbacks = {&runtime, set_led, set_buzzer, NULL, alarm_queue_save_request};
    storage_interface_t store = {NULL, read_store, write_store, NULL};
    uint16_t first_sequence;
    uint16_t second_sequence;
    alarm_message_t idle = {0};
    services.system.millis = clock_now;
    services.system.random = random_bytes;
    services.system.queue_put = queue_put;
    services.system.fault = fault;
    services.system.diagnostic = diagnostic;
    services.system.packet_log = packet_log;
    runtime.services = &services;
    runtime.transport = &transport;
    runtime.front_queue = (void *)2;
    runtime.background_queue = (void *)1;
    transport.publish = publish;
    assert(alarm_button_init(&runtime.button_state, &config, &callbacks) == ALARM_OK);
    assert(alarm_store_open(&runtime.store, ALARM_BUTTON_PRODUCT_ID, &store) == ALARM_OK);
    runtime.storage_ready = true;
#ifdef TEST_CLOUD_CONFIGURED
    assert(alarm_load_cloud_config(&runtime));
    assert(strcmp(runtime.cloud.broker_host, "broker.invalid") == 0);
#ifdef TEST_PLAINTEXT_MODE
    assert(runtime.protocol.manufacturer_id == 0x4872U);
    assert(!missing_factory_code_reported && runtime.identity.unknown_telemetry_verified);
    assert(runtime.protocol.factory_code[0] == '\0');
#else
    assert(strcmp(runtime.protocol.factory_code, "TEST-VENDOR") == 0);
#endif
    assert(runtime.cloud.broker_port == 1883);
#else
    assert(!alarm_load_cloud_config(&runtime)); /* No invented credentials in shipped defaults. */
    assert(missing_manufacturer_reported && missing_factory_code_reported);
#endif
    runtime.protocol.manufacturer_id = ALARM_BUTTON_MANUFACTURER_ID ? ALARM_BUTTON_MANUFACTURER_ID : 0x1234;
    memset(runtime.protocol.aes_key, 0x42, 16);
    memset(runtime.protocol.factory_code, 'T', 32);
    strcpy(runtime.cloud.platform_down_topic, "down");
    strcpy(runtime.identity.imei, "123456789012345");
    strcpy(runtime.identity.imsi, "123456789012345");
    strcpy(runtime.identity.iccid, "12345678901234567890");
    assert(alarm_make_platform_topics(&runtime));
#ifdef TEST_PLAINTEXT_MODE
    assert(strcmp(runtime.cloud.platform_up_topic, "iot/devices/123456789012345/test/plain/up") == 0);
    assert(strcmp(runtime.cloud.platform_down_topic, "iot/devices/123456789012345/test/plain/down") == 0);
#else
    assert(strcmp(runtime.cloud.platform_up_topic, "iot/devices/123456789012345/sys/fire/aesdata/up") == 0);
#endif
    strcpy(runtime.cloud.platform_down_topic, "down");
    runtime.identity.unknown_telemetry_verified = true;
    runtime.identity.unknown_telemetry = 0xff;
    test_battery_collection();
    services.system.identity = reconnect_identity;
    ml307y_alarm_battery_bind(&services.battery);
    runtime.identity_ready = true;
    alarm_reporter_init(&runtime.reporter, &runtime.store, alarm_publish_event, &runtime, 10000, 5000,
                     30000);
    runtime.reporter.event_ready = alarm_event_is_ready;
    press(100);
    alarm_mock_battery_millivolts = 4000U;
    press(1000);
    assert(runtime.store.image.count == 2);
    assert(runtime.button_state.indicator.event_id == 2);
    runtime.heartbeat_needed = true;
    alarm_send_registration_or_heartbeat(&runtime, now_ms);
    assert(wire_command == KAIWAN_COMMAND_REGISTER && wire_type == 4);
    assert(wire_voltage == 40U && wire_percent == (400000U - ALARM_BATTERY_EMPTY_MILLIVOLTS * 100U +
        (ALARM_BATTERY_FULL_MILLIVOLTS - ALARM_BATTERY_EMPTY_MILLIVOLTS) / 2U) /
        (ALARM_BATTERY_FULL_MILLIVOLTS - ALARM_BATTERY_EMPTY_MILLIVOLTS));
#ifdef TEST_PLAINTEXT_MODE
    test_invalid_plaintext_replies(wire_sequence);
#endif
    response(wire_sequence, 0);
    assert(runtime.platform_registered && runtime.store.image.count == 2);
    alarm_send_registration_or_heartbeat(&runtime, now_ms);
    assert(wire_command == KAIWAN_COMMAND_EVENT && wire_type == 4 && wire_event == 1);
    assert(wire_voltage == 40U && wire_percent == (400000U - ALARM_BATTERY_EMPTY_MILLIVOLTS * 100U +
        (ALARM_BATTERY_FULL_MILLIVOLTS - ALARM_BATTERY_EMPTY_MILLIVOLTS) / 2U) /
        (ALARM_BATTERY_FULL_MILLIVOLTS - ALARM_BATTERY_EMPTY_MILLIVOLTS));
    response(wire_sequence, 0);
    assert(!runtime.heartbeat_needed && runtime.store.image.count == 2);
    alarm_reporter_poll(&runtime.reporter, true, now_ms);
    first_sequence = wire_sequence;
    assert(wire_type == 4 && wire_event == 0x0c);
    assert(wire_voltage == 37U && wire_percent == (370000U - ALARM_BATTERY_EMPTY_MILLIVOLTS * 100U +
        (ALARM_BATTERY_FULL_MILLIVOLTS - ALARM_BATTERY_EMPTY_MILLIVOLTS) / 2U) /
        (ALARM_BATTERY_FULL_MILLIVOLTS - ALARM_BATTERY_EMPTY_MILLIVOLTS));
    alarm_on_publish_result(first_sequence, KAIWAN_CLOUD_OK, &runtime);
    assert(runtime.store.image.count == 2 && !runtime.button_state.indicator.acked);
    assert(alarm_on_cloud_message("down", 4, (const uint8_t *)"bad", 3, &runtime) != KAIWAN_CLOUD_OK);
    now_ms = 1200;
    response(first_sequence, 0);
    assert(runtime.store.image.count == 1 && !runtime.button_state.indicator.acked);
    assert(alarm_confirmations == 1);
    response(first_sequence, 0);
    assert(runtime.store.image.count == 1);
    alarm_reporter_poll(&runtime.reporter, true, now_ms);
    second_sequence = wire_sequence;
    assert(wire_voltage == 40U && wire_percent == (400000U - ALARM_BATTERY_EMPTY_MILLIVOLTS * 100U +
        (ALARM_BATTERY_FULL_MILLIVOLTS - ALARM_BATTERY_EMPTY_MILLIVOLTS) / 2U) /
        (ALARM_BATTERY_FULL_MILLIVOLTS - ALARM_BATTERY_EMPTY_MILLIVOLTS));
    alarm_mock_battery_millivolts = 4200U;
    runtime.telemetry_requested = true;
    alarm_collect_device_info(&runtime, now_ms);
    assert(runtime.telemetry.battery_mv == 4200U && runtime.telemetry.battery_percent == 100U);
    fail_write = true;
    response(second_sequence, 0);
    assert(runtime.store.image.count == 1 && faults > 0 && !runtime.button_state.indicator.acked);
    assert(alarm_confirmations == 1);
    fail_write = false;
    response(second_sequence, 1);
    assert(runtime.store.image.count == 1);
    now_ms = 36000;
    alarm_button_update(&runtime.button_state, false, now_ms);
    assert(!led_on && runtime.store.image.count == 1);
    alarm_reporter_poll(&runtime.reporter, true, now_ms);
    now_ms += 30000;
    alarm_reporter_poll(&runtime.reporter, true, now_ms);
    assert(published >= 5);
    assert(wire_voltage == 40U && wire_percent == (400000U - ALARM_BATTERY_EMPTY_MILLIVOLTS * 100U +
        (ALARM_BATTERY_FULL_MILLIVOLTS - ALARM_BATTERY_EMPTY_MILLIVOLTS) / 2U) /
        (ALARM_BATTERY_FULL_MILLIVOLTS - ALARM_BATTERY_EMPTY_MILLIVOLTS));
    response(second_sequence, 0); /* A previous real attempt may still acknowledge its event. */
    assert(runtime.store.image.count == 0 && runtime.button_state.indicator.acked);
    assert(alarm_confirmations == 2 && registration_confirmations == 1 && heartbeat_confirmations == 1);
    idle.kind = ALARM_MSG_BACKGROUND_IDLE;
    idle.request = 1;
    alarm_handle_background_result(&runtime, &idle, now_ms);
    assert(!runtime.button_state.background_idle);
    idle.request = 2;
    alarm_handle_background_result(&runtime, &idle, now_ms);
    assert(runtime.button_state.background_idle);
    test_stay_online();
    test_reconnect_heartbeat();
    test_heartbeat_callback_clock();
    test_configured_heartbeat();
    test_registration_failures();
    test_pending_history_does_not_block_new_alarm();
    if (ALARM_BUTTON_PACKET_LOG_ENABLED)
    {
        assert(packet_logs == published && registration_logs > 0 && heartbeat_logs > 0 && alarm_logs > 0);
        assert(failed_packet_logs == 1);
    }
    else
    {
        assert(packet_logs == 0);
    }
    test_first_heartbeat_alarm_gate();
    test_save_failure_recovery();
    services.key.ready = true;
    services.led.ready = true;
    services.buzzer.ready = true;
    services.battery.ready = true;
    services.transport = &transport;
    services.system.allocate = malloc;
    services.system.queue_create = create_queue;
    services.system.thread_start = start_thread;
    services.key.ready = false;
    assert(!alarm_product_start(&services));
    assert(thread_count == 0);
    services.key.ready = true;
    queue_limit = 0;
    assert(!alarm_product_start(&services));
    assert(thread_count == 0);
    queue_limit = 2;
    queue_count = 0;
    thread_limit = 0;
    assert(!alarm_product_start(&services));
    assert(thread_count == 1);
    assert(startup_context && !startup_context->startup_ready);
    queue_count = 0;
    thread_count = 0;
    thread_limit = 1;
    error_posts = 0;
    assert(!alarm_product_start(&services));
    assert(thread_count == 2 && error_posts == 0);
    assert(!startup_context->startup_ready);
    assert(startup_context->startup_failed);
    queue_count = 0;
    thread_count = 0;
    thread_limit = 2;
    assert(alarm_product_start(&services));
    assert(thread_count == 2);
    assert(startup_context->startup_ready);
    assert(!startup_context->startup_failed);
    puts("runtime: business confirmation, registration, heartbeat, re-press, failed delete and "
         "late retry OK");
    return 0;
}
