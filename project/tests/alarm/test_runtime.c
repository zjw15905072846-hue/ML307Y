/*------------------------------------------includes--------------------------------------------*/
#include "test_support.h"
#include "../../src/alarm_button/alarm_runtime.c"
/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
static alarm_runtime_t runtime;
static product_services_t services;
static kaiwan_transport_t transport;
static alarm_storage_image_t disk_image;
static bool disk_exists;
static bool fail_write;
static uint32_t now_ms;
static alarm_message_t submitted[8];
static unsigned submitted_count;
static unsigned published;
static uint16_t wire_sequence;
static uint8_t wire_command;
static uint8_t wire_type;
static uint8_t wire_event;
static unsigned faults;
static bool led_on;
static unsigned error_posts;
static unsigned queue_count;
static unsigned thread_count;
static unsigned queue_limit = 2;
static unsigned thread_limit = 2;
static alarm_runtime_t *startup_context;

/*-------------------------------------------function---------------------------------------------*/
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
    (void)module;
    (void)error;
    ++faults;
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
    kaiwan_protocol_workspace_t workspace;
    kaiwan_frame_view_t frame;
    (void)user;
    (void)topic;
    (void)qos;
    (void)retain;
    (void)cookie;
    assert(kaiwan_session_decode(&runtime.protocol, &workspace, (const char *)payload, size, decoded,
                             sizeof(decoded), &frame) == KAIWAN_OK);
    assert(disk_exists && disk_image.next_sequence > frame.sequence);
    wire_sequence = frame.sequence;
    wire_command = frame.command;
    wire_type = frame.data[0];
    wire_event = frame.data[16];
    ++published;
    return KAIWAN_CLOUD_OK;
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
    uint8_t iv[16] = {0};
    kaiwan_protocol_workspace_t workspace;
    char json[KAIWAN_PROTOCOL_MAXIMUM_JSON_SIZE];
    size_t length;
    size_t encoded;
    assert(kaiwan_protocol_build_frame(&runtime.protocol, sequence, KAIWAN_COMMAND_SERVER_RESPONSE, &code, 1,
                                   frame, sizeof(frame), &length) == KAIWAN_OK);
    assert(kaiwan_protocol_wrap_json(&runtime.protocol, &workspace, frame, length, iv, json,
                                 sizeof(json), &encoded) == KAIWAN_OK);
    assert(alarm_on_cloud_message("down", 4, (const uint8_t *)json, encoded, &runtime) == KAIWAN_CLOUD_OK);
}

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
    runtime.services = &services;
    runtime.transport = &transport;
    runtime.front_queue = (void *)2;
    runtime.background_queue = (void *)1;
    transport.publish = publish;
    assert(alarm_button_init(&runtime.button_state, &config, &callbacks) == ALARM_OK);
    assert(alarm_store_open(&runtime.store, ALARM_BUTTON_PRODUCT_ID, &store) == ALARM_OK);
    runtime.storage_ready = true;
    assert(!alarm_load_cloud_config(&runtime)); /* No invented credentials in shipped defaults. */
    runtime.protocol.manufacturer_id = 0x1234;
    memset(runtime.protocol.aes_key, 0x42, 16);
    memset(runtime.protocol.factory_code, 'T', 32);
    strcpy(runtime.cloud.platform_down_topic, "down");
    strcpy(runtime.identity.imei, "123456789012345");
    strcpy(runtime.identity.imsi, "123456789012345");
    strcpy(runtime.identity.iccid, "12345678901234567890");
    runtime.identity.unknown_telemetry_verified = true;
    runtime.identity.unknown_telemetry = 0xff;
    alarm_reporter_init(&runtime.reporter, &runtime.store, alarm_publish_event, &runtime, 10000, 5000,
                     30000);
    press(100);
    press(1000);
    assert(runtime.store.image.count == 2);
    assert(runtime.button_state.indicator.event_id == 2);
    runtime.heartbeat_needed = true;
    alarm_send_registration_or_heartbeat(&runtime, now_ms);
    assert(wire_command == KAIWAN_COMMAND_REGISTER && wire_type == 4);
    response(wire_sequence, 0);
    assert(runtime.platform_registered && runtime.store.image.count == 2);
    alarm_send_registration_or_heartbeat(&runtime, now_ms);
    assert(wire_command == KAIWAN_COMMAND_EVENT && wire_type == 4 && wire_event == 1);
    response(wire_sequence, 0);
    assert(!runtime.heartbeat_needed && runtime.store.image.count == 2);
    alarm_reporter_poll(&runtime.reporter, true, now_ms);
    first_sequence = wire_sequence;
    assert(wire_type == 4 && wire_event == 0x0c);
    alarm_on_publish_result(first_sequence, KAIWAN_CLOUD_OK, &runtime);
    assert(runtime.store.image.count == 2 && !runtime.button_state.indicator.acked);
    assert(alarm_on_cloud_message("down", 4, (const uint8_t *)"bad", 3, &runtime) != KAIWAN_CLOUD_OK);
    now_ms = 1200;
    response(first_sequence, 0);
    assert(runtime.store.image.count == 1 && !runtime.button_state.indicator.acked);
    response(first_sequence, 0);
    assert(runtime.store.image.count == 1);
    alarm_reporter_poll(&runtime.reporter, true, now_ms);
    second_sequence = wire_sequence;
    fail_write = true;
    response(second_sequence, 0);
    assert(runtime.store.image.count == 1 && faults > 0 && !runtime.button_state.indicator.acked);
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
    response(second_sequence, 0); /* A previous real attempt may still acknowledge its event. */
    assert(runtime.store.image.count == 0 && runtime.button_state.indicator.acked);
    idle.kind = ALARM_MSG_BACKGROUND_IDLE;
    idle.request = 1;
    alarm_handle_background_result(&runtime, &idle, now_ms);
    assert(!runtime.button_state.background_idle);
    idle.request = 2;
    alarm_handle_background_result(&runtime, &idle, now_ms);
    assert(runtime.button_state.background_idle);
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
    puts("runtime: encrypted business confirmation, registration, heartbeat, re-press, failed delete and "
         "late retry OK");
    return 0;
}
