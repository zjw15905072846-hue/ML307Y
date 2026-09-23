/*------------------------------------------includes--------------------------------------------*/
#include "test_support.h"
#include "../../products/alarm_button/alarm_runtime.c"
/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
static alarm_runtime_t runtime;
static product_services_t services;
static kw_transport_t transport;
static al_image_t disk_image;
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
        if (m->kind == AR_ERROR)
        {
            ++error_posts;
        }
        ar_ui_message(&runtime, m, now_ms);
    }
    return true;
}

/*******************************************************************************
* Function Name  : output
* Description    : 捕获指示灯
* Input          : user/led/buzzer - 输出
* Output         : led_on
* Return         : true
* Attention      : 不操作GPIO
*******************************************************************************/
static bool output(void *user, bool led, bool buzzer)
{
    (void)user;
    (void)buzzer;
    led_on = led;
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
static kw_cloud_result_t publish(void *user, const char *topic, const uint8_t *payload, size_t size,
                                 uint8_t qos, bool retain, uint32_t cookie)
{
    uint8_t decoded[KW_PROTOCOL_MAX_FRAME_SIZE];
    kw_protocol_workspace_t workspace;
    kw_frame_view_t frame;
    (void)user;
    (void)topic;
    (void)qos;
    (void)retain;
    (void)cookie;
    assert(kw_session_decode(&runtime.protocol, &workspace, (const char *)payload, size, decoded,
                             sizeof(decoded), &frame) == KW_OK);
    assert(disk_exists && disk_image.next_sequence > frame.sequence);
    wire_sequence = frame.sequence;
    wire_command = frame.command;
    wire_type = frame.data[0];
    wire_event = frame.data[16];
    ++published;
    return KW_CLOUD_OK;
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
    uint8_t frame[KW_PROTOCOL_MAX_FRAME_SIZE];
    uint8_t iv[16] = {0};
    kw_protocol_workspace_t workspace;
    char json[KW_PROTOCOL_MAX_JSON_SIZE];
    size_t length;
    size_t encoded;
    assert(kw_protocol_build_frame(&runtime.protocol, sequence, KW_CMD_SERVER_RESPONSE, &code, 1,
                                   frame, sizeof(frame), &length) == KW_OK);
    assert(kw_protocol_wrap_json(&runtime.protocol, &workspace, frame, length, iv, json,
                                 sizeof(json), &encoded) == KW_OK);
    assert(ar_cloud_message("down", 4, (const uint8_t *)json, encoded, &runtime) == KW_CLOUD_OK);
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
    ab_poll(&runtime.app, false, time - 40, NULL);
    ab_poll(&runtime.app, false, time - 10, NULL);
    ab_poll(&runtime.app, true, time, NULL);
    now_ms = time + 30;
    ab_poll(&runtime.app, true, now_ms, NULL);
    assert(led_on && submitted_count == index + 1);
    assert(runtime.app.pending_saves == 1);
    ar_worker_message(&runtime, &submitted[index]);
    assert(runtime.app.pending_saves == 0);
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
    return ++queue_count == 1 ? (void *)1 : (void *)2;
}

/*******************************************************************************
* Function Name  : start_thread
* Description    : 模拟前台成功而后台创建失败
* Input          : name/entry/argument/stack/foreground - 任务参数
* Output         : 计数
* Return         : 第一次成功
* Attention      : 仅验证错误通知路径
*******************************************************************************/
static bool start_thread(const char *name, void (*entry)(void *), void *argument, unsigned stack,
                         bool foreground)
{
    (void)name;
    (void)entry;
    (void)argument;
    (void)stack;
    (void)foreground;
    return ++thread_count == 1;
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
    ab_config_t config = ab_default_config();
    ab_io_t io = {&runtime, output, NULL, ar_submit_event};
    storage_if_t store = {NULL, read_store, write_store, NULL};
    uint16_t first_sequence;
    uint16_t second_sequence;
    alarm_message_t idle = {0};
    services.system.millis = clock_now;
    services.system.random = random_bytes;
    services.system.queue_put = queue_put;
    services.system.fault = fault;
    runtime.services = &services;
    runtime.transport = &transport;
    runtime.ui_queue = (void *)2;
    runtime.worker_queue = (void *)1;
    transport.publish = publish;
    assert(ab_init(&runtime.app, &config, &io, NULL, NULL) == AL_OK);
    assert(al_store_open(&runtime.store, AB_PRODUCT_ID, &store) == AL_OK);
    runtime.worker_ready = true;
    assert(!ar_configure(&runtime)); /* No invented credentials in shipped defaults. */
    runtime.protocol.manufacturer_id = 0x1234;
    memset(runtime.protocol.aes_key, 0x42, 16);
    memset(runtime.protocol.factory_code, 'T', 32);
    strcpy(runtime.cloud.platform_down_topic, "down");
    strcpy(runtime.identity.imei, "123456789012345");
    strcpy(runtime.identity.imsi, "123456789012345");
    strcpy(runtime.identity.iccid, "12345678901234567890");
    runtime.identity.unknown_telemetry_verified = true;
    runtime.identity.unknown_telemetry = 0xff;
    al_reporter_init(&runtime.reporter, &runtime.store, ar_send_alarm, &runtime, 10000, 5000,
                     30000);
    press(100);
    press(1000);
    assert(runtime.store.image.count == 2);
    assert(runtime.app.latest_event_id == 2);
    runtime.heartbeat_needed = true;
    ar_control(&runtime, now_ms);
    assert(wire_command == KW_CMD_REGISTER && wire_type == 4);
    response(wire_sequence, 0);
    assert(runtime.registered && runtime.store.image.count == 2);
    ar_control(&runtime, now_ms);
    assert(wire_command == KW_CMD_EVENT && wire_type == 4 && wire_event == 1);
    response(wire_sequence, 0);
    assert(!runtime.heartbeat_needed && runtime.store.image.count == 2);
    al_reporter_poll(&runtime.reporter, true, now_ms);
    first_sequence = wire_sequence;
    assert(wire_type == 4 && wire_event == 0x0c);
    ar_cloud_tx(first_sequence, KW_CLOUD_OK, &runtime);
    assert(runtime.store.image.count == 2 && !runtime.app.indicator.acked);
    assert(ar_cloud_message("down", 4, (const uint8_t *)"bad", 3, &runtime) != KW_CLOUD_OK);
    now_ms = 1200;
    response(first_sequence, 0);
    assert(runtime.store.image.count == 1 && !runtime.app.indicator.acked);
    response(first_sequence, 0);
    assert(runtime.store.image.count == 1);
    al_reporter_poll(&runtime.reporter, true, now_ms);
    second_sequence = wire_sequence;
    fail_write = true;
    response(second_sequence, 0);
    assert(runtime.store.image.count == 1 && faults > 0 && !runtime.app.indicator.acked);
    fail_write = false;
    response(second_sequence, 1);
    assert(runtime.store.image.count == 1);
    now_ms = 36000;
    ab_poll(&runtime.app, false, now_ms, NULL);
    assert(!led_on && runtime.store.image.count == 1);
    al_reporter_poll(&runtime.reporter, true, now_ms);
    now_ms += 30000;
    al_reporter_poll(&runtime.reporter, true, now_ms);
    assert(published >= 5);
    response(second_sequence, 0); /* A previous real attempt may still acknowledge its event. */
    assert(runtime.store.image.count == 0 && runtime.app.indicator.acked);
    idle.kind = AR_IDLE;
    idle.request = 1;
    ar_ui_message(&runtime, &idle, now_ms);
    assert(!runtime.app.background_idle);
    idle.request = 2;
    ar_ui_message(&runtime, &idle, now_ms);
    assert(runtime.app.background_idle);
    services.board.ready = true;
    services.transport = &transport;
    services.system.allocate = malloc;
    services.system.queue_create = create_queue;
    services.system.thread_start = start_thread;
    error_posts = 0;
    alarm_product_start(&services);
    assert(thread_count == 2 && error_posts == 1);
    puts("runtime: encrypted business ACK, registration, heartbeat, re-press, failed delete and "
         "late retry OK");
    return 0;
}
