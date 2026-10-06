/*------------------------------------------includes--------------------------------------------*/
#include <stdint.h>
static uint32_t offline_exchange(uint32_t *address, uint32_t value);
#define __atomic_exchange_n(address, value, order) offline_exchange(address, value)
#define TEST_PLAINTEXT_MODE 1
#define main existing_runtime_main
#include "test_runtime.c"
#undef main
/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
static unsigned stop_count;
static bool stop_ready = true;
static bool radio_fail;
static bool radio_target = true;
static system_radio_state_t radio_status = SYSTEM_RADIO_READY;
static unsigned radio_requests;
static unsigned poll_count;
static bool full_queue;
static uint32_t wait_seen;
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : offline_exchange
* Description    : 模拟单线程原子交换
* Input          : address - 地址；value - 新值
* Output         : 地址内容
* Return         : 旧值
* Attention      : 只替代模拟器缺失的指令，不验证真实并发
*******************************************************************************/
static uint32_t offline_exchange(uint32_t *address, uint32_t value)
{
    uint32_t previous = *address;
    *address = value;
    return previous;
}
/*******************************************************************************
* Function Name  : offline_stop
* Description    : 记录业务完成后的主动断连
* Input          : user - 测试实例
* Output         : stop_count
* Return         : true - 已断连
* Attention      : 不访问实网
*******************************************************************************/
static bool offline_stop(void *user)
{
    (void)user;
    ++stop_count;
    return stop_ready;
}

/*******************************************************************************
* Function Name  : radio_request
* Description    : 模拟异步射频请求
* Input          : 模拟接口参数
* Output         : 测试状态及断言
* Return         : 模拟结果
* Attention      : 不连接实网，不代替实板验证
*******************************************************************************/
static void radio_request(void *user, bool enabled, uint32_t now)
{
    (void)user;
    (void)now;
    radio_target = enabled;
    radio_status = enabled ? SYSTEM_RADIO_RESTORING : SYSTEM_RADIO_STOPPING;
    ++radio_requests;
}

/*******************************************************************************
* Function Name  : radio_poll
* Description    : 模拟完成或失败
* Input          : 模拟接口参数
* Output         : 测试状态及断言
* Return         : 模拟结果
* Attention      : 不连接实网，不代替实板验证
*******************************************************************************/
static void radio_poll(void *user, uint32_t now)
{
    (void)user;
    (void)now;
    ++poll_count;
    radio_status = radio_fail ? SYSTEM_RADIO_ERROR : (radio_target ? SYSTEM_RADIO_READY : SYSTEM_RADIO_OFF);
}

/*******************************************************************************
* Function Name  : radio_state
* Description    : 查询模拟状态
* Input          : 模拟接口参数
* Output         : 测试状态及断言
* Return         : 模拟结果
* Attention      : 不连接实网，不代替实板验证
*******************************************************************************/
static system_radio_state_t radio_state(void *user, int *error)
{
    (void)user;
    *error = radio_fail ? -1 : 0;
    return radio_status;
}

/*******************************************************************************
* Function Name  : radio_wait
* Description    : 模拟恢复检查期限
* Input          : 模拟接口参数
* Output         : 测试状态及断言
* Return         : 模拟结果
* Attention      : 不连接实网，不代替实板验证
*******************************************************************************/
static uint32_t radio_wait(void *user, uint32_t now)
{
    (void)user;
    (void)now;
    return 1000U;
}

/*******************************************************************************
* Function Name  : offline_put
* Description    : 模拟队列满及正常前台回传
* Input          : 模拟接口参数
* Output         : 测试状态及断言
* Return         : 模拟结果
* Attention      : 不连接实网，不代替实板验证
*******************************************************************************/
static bool offline_put(void *queue, const void *message, uint32_t wait)
{
    return full_queue ? false : queue_put(queue, message, wait);
}

/*******************************************************************************
* Function Name  : offline_get
* Description    : 记录后台阻塞期限
* Input          : 模拟接口参数
* Output         : 测试状态及断言
* Return         : 模拟结果
* Attention      : 不连接实网，不代替实板验证
*******************************************************************************/
static bool offline_get(void *queue, void *message, uint32_t wait)
{
    (void)queue;
    (void)message;
    if (wait)
    {
        wait_seen = wait;
    }
    return false;
}

/*******************************************************************************
* Function Name  : prepare_offline
* Description    : 准备已完成业务的离线场景
* Input          : 模拟接口参数
* Output         : 测试状态及断言
* Return         : 模拟结果
* Attention      : 不连接实网，不代替实板验证
*******************************************************************************/
static void prepare_offline(void)
{
    memset(&runtime, 0, sizeof(runtime));
    memset(&services, 0, sizeof(services));
    memset(&transport, 0, sizeof(transport));
    runtime.services = &services;
    runtime.transport = &transport;
    services.system.millis = clock_now;
    services.system.queue_put = offline_put;
    services.system.queue_get = offline_get;
    services.system.fault = fault;
    services.system.radio_request = radio_request;
    services.system.radio_poll = radio_poll;
    services.system.radio_state = radio_state;
    services.system.radio_next_wait = radio_wait;
    runtime.offline_enabled = true;
    runtime.front_quiet = 1;
    runtime.network_phase = ALARM_NETWORK_ONLINE;
    runtime.storage_ready = true;
    runtime.store.ready = true;
    runtime.cloud_started = true;
    runtime.identity_ready = true;
    runtime.platform_registered = true;
    runtime.heartbeat_completed = true;
    runtime.last_heartbeat = now_ms;
    runtime.attempt_started = now_ms;
    transport.poll = poll_trial;
    transport.online = online_trial;
    transport.stop = offline_stop;
    radio_target = true;
    radio_status = SYSTEM_RADIO_READY;
    radio_fail = false;
    stop_ready = true;
    full_queue = false;
    stop_count = 0;
    submitted_count = 0;
}

/*******************************************************************************
* Function Name  : offline_start
* Description    : 模拟重建已订阅会话，保留业务注册
* Input          : 模拟参数或无
* Output         : 模拟状态与断言
* Return         : 模拟结果
* Attention      : 网络订阅门槛另由 MQTT 端口测试覆盖，不证明实网
*******************************************************************************/
static kaiwan_cloud_result_t offline_start(void *user, const kaiwan_cloud_config_t *config,
                                                 const kaiwan_cloud_callbacks_t *callbacks)
{
    (void)user;
    (void)config;
    callbacks->on_state_changed(true, callbacks->user);
    strcpy(runtime.cloud.platform_down_topic, "down");
    return KAIWAN_CLOUD_OK;
}

/*******************************************************************************
* Function Name  : test_offline_business_cycle
* Description    : 完整覆盖注册心跳、真实回执、离线按键保存、确认及下一周期
* Input          : 模拟参数或无
* Output         : 模拟状态与断言
* Return         : 模拟结果
* Attention      : 网络订阅门槛另由 MQTT 端口测试覆盖，不证明实网
*******************************************************************************/
static void test_offline_business_cycle(void)
{
    alarm_button_config_t config = alarm_button_default_config();
    alarm_button_callbacks_t callbacks = {&runtime, set_led, set_buzzer, NULL, alarm_queue_save_request};
    storage_interface_t store = {NULL, read_store, write_store, NULL};
    uint32_t deadline;
    unsigned sent;
    now_ms = 1000U;
    prepare_offline();
    runtime.background_queue = (void *)1;
    runtime.front_queue = (void *)2;
    disk_exists = false;
    fail_write = false;
    assert(alarm_button_init(&runtime.button_state, &config, &callbacks) == ALARM_OK);
    assert(alarm_store_open(&runtime.store, ALARM_BUTTON_PRODUCT_ID, &store) == ALARM_OK);
    runtime.cloud_config_valid = alarm_load_cloud_config(&runtime);
    assert(runtime.cloud_config_valid);
    strcpy(runtime.identity.imei, "123456789012345");
    strcpy(runtime.identity.imsi, "123456789012345");
    strcpy(runtime.identity.iccid, "12345678901234567890");
    assert(alarm_make_platform_topics(&runtime));
    strcpy(runtime.cloud.platform_down_topic, "down");
    services.system.identity = reconnect_identity;
    services.system.packet_log = packet_log;
    ml307y_alarm_battery_bind(&services.battery);
    runtime.telemetry_requested = true;
    runtime.platform_registered = false;
    runtime.heartbeat_completed = false;
    runtime.heartbeat_needed = true;
    transport.start = offline_start;
    transport.publish = publish;
    alarm_reporter_init(&runtime.reporter, &runtime.store, alarm_publish_event, &runtime, 10000U, 5000U, 30000U);
    runtime.reporter.event_ready = alarm_event_is_ready;
    alarm_process_background(&runtime, now_ms);
    assert(wire_command == KAIWAN_COMMAND_REGISTER && runtime.waiting_control_reply);
    response(wire_sequence, 0);
    alarm_process_background(&runtime, now_ms);
    assert(wire_command == KAIWAN_COMMAND_EVENT && wire_event == 1);
    response(wire_sequence, 0);
    deadline = now_ms + ALARM_BUTTON_HEARTBEAT_MS;
    alarm_process_background(&runtime, now_ms);
    alarm_process_background(&runtime, now_ms);
    assert(runtime.network_phase == ALARM_NETWORK_SLEEP);
    sent = published;
    now_ms += 60000U;
    runtime.front_quiet = 0;
    /* 真实前台消抖 -> 后台持久化；此时射频仍处于关闭状态。 */
    press(now_ms);
    assert(runtime.store.image.count == 1 && radio_status == SYSTEM_RADIO_OFF);
    alarm_process_background(&runtime, now_ms);
    assert(runtime.platform_registered && published == sent + 1 && wire_event == 12);
    alarm_on_publish_result(wire_sequence, KAIWAN_CLOUD_OK, &runtime);
    assert(runtime.store.image.count == 1);
    fail_write = true;
    response(wire_sequence, 0);
    assert(runtime.store.image.count == 1);
    alarm_process_background(&runtime, now_ms);
    assert(runtime.network_phase == ALARM_NETWORK_ONLINE);
    fail_write = false;
    now_ms += ALARM_BUTTON_RETRY_MAXIMUM_MS;
    alarm_process_background(&runtime, now_ms);
    response(wire_sequence, 0);
    assert(runtime.store.image.count == 0);
    alarm_button_update(&runtime.button_state, false, now_ms);
    now_ms += 30U;
    alarm_button_update(&runtime.button_state, false, now_ms);
    runtime.front_quiet = 1;
    alarm_process_background(&runtime, now_ms);
    alarm_process_background(&runtime, now_ms);
    assert(runtime.network_phase == ALARM_NETWORK_SLEEP && runtime.offline_deadline == deadline);
    sent = published;
    now_ms = deadline;
    alarm_process_background(&runtime, now_ms);
    assert(published == sent + 1 && wire_event == 1 && runtime.platform_registered);
    response(wire_sequence, 0);
    alarm_process_background(&runtime, now_ms);
    alarm_process_background(&runtime, now_ms);
    assert(runtime.network_phase == ALARM_NETWORK_SLEEP);
    assert(runtime.offline_deadline == now_ms + ALARM_BUTTON_HEARTBEAT_MS);
}
/*******************************************************************************
* Function Name  : main
* Description    : 复现业务完成后仍持有 MQTT 的旧行为
* Input          : 无
* Output         : 断言
* Return         : 0 - 通过
* Attention      : 主机状态测试不证明实板功耗
*******************************************************************************/
int main(void)
{
    unsigned before;
    uint32_t confirmed;
    now_ms = 100U;
    prepare_offline();
    alarm_process_background(&runtime, now_ms);
    assert(stop_count == 1 && runtime.network_phase == ALARM_NETWORK_RADIO_OFF);
    assert(!runtime.background_waiting);
    alarm_process_background(&runtime, now_ms);
    assert(runtime.network_phase == ALARM_NETWORK_SLEEP && runtime.background_waiting);
    assert(runtime.offline_deadline == 100U + ALARM_BUTTON_HEARTBEAT_MS);
    assert(runtime.next_heartbeat_ms == runtime.offline_deadline);
    before = poll_count;
    now_ms += 1000U;
    alarm_process_background(&runtime, now_ms);
    assert(poll_count == before); /* 离线没有持续查询射频或采样。 */
    alarm_read_front_requests(&runtime);
    assert(wait_seen == runtime.offline_deadline - now_ms);
    now_ms = runtime.offline_deadline;
    assert(alarm_network_allow_processing(&runtime, now_ms));
    assert(runtime.platform_registered && runtime.heartbeat_needed);

    /* 注册尚未成功与心跳未确认都允许预算结束后等待十五分钟。 */
    for (unsigned registered = 0; registered < 2; ++registered)
    {
        now_ms = UINT32_MAX - 1000U;
        prepare_offline();
        runtime.platform_registered = registered != 0;
        runtime.heartbeat_needed = true;
        confirmed = runtime.last_heartbeat;
        now_ms += ALARM_BUTTON_NETWORK_ATTEMPT_MS - 1U;
        assert(alarm_network_allow_processing(&runtime, now_ms));
        ++now_ms;
        assert(!alarm_network_allow_processing(&runtime, now_ms));
        assert(runtime.retry_sleep && stop_count == 1);
        now_ms += 500U;
        assert(!alarm_network_allow_processing(&runtime, now_ms));
        assert(runtime.offline_deadline == now_ms + ALARM_BUTTON_NETWORK_RETRY_SLEEP_MS);
        assert(runtime.last_heartbeat == confirmed);
        assert(runtime.heartbeat_needed);
        now_ms = runtime.offline_deadline - 1U;
        assert(!alarm_network_allow_processing(&runtime, now_ms));
        ++now_ms;
        assert(alarm_network_allow_processing(&runtime, now_ms));
        assert(runtime.attempt_started == now_ms);
    }

    /* 未确认、删除失败、保存失败和前台提示均阻止关网。 */
    prepare_offline();
    runtime.heartbeat_needed = true;
    runtime.store.image.count = 1;
    now_ms += ALARM_BUTTON_NETWORK_ATTEMPT_MS * 2U;
    assert(alarm_network_allow_processing(&runtime, now_ms) && stop_count == 0);
    runtime.store.image.count = 0;
    runtime.save_failed = true;
    assert(alarm_network_allow_processing(&runtime, now_ms) && stop_count == 0);
    runtime.save_failed = false;
    runtime.reporter.last_error = ALARM_ERROR_STORAGE;
    assert(alarm_network_allow_processing(&runtime, now_ms) && stop_count == 0);
    runtime.reporter.last_error = ALARM_OK;
    runtime.front_quiet = 0;
    assert(!alarm_network_allow_processing(&runtime, now_ms) && stop_count == 0);
    runtime.front_quiet = 1;
    assert(!alarm_network_allow_processing(&runtime, now_ms) && stop_count == 1);

    /* 射频失败与前台通知队列满均不得伪装入睡。 */
    radio_fail = true;
    assert(!alarm_network_allow_processing(&runtime, now_ms));
    assert(!runtime.background_waiting);
    radio_fail = false;
    full_queue = true;
    assert(!alarm_network_allow_processing(&runtime, now_ms));
    assert(runtime.network_phase == ALARM_NETWORK_RADIO_OFF);
    full_queue = false;
    assert(!alarm_network_allow_processing(&runtime, now_ms));
    assert(runtime.background_waiting);

    /* 离线等待中按键提前唤醒；故意让通知投递失败，原子标志仍存在。 */
    full_queue = true;
    alarm_notify_key_wakeup(&runtime);
    assert(runtime.button_wake_pending && runtime.key_pending && !runtime.front_quiet);
    full_queue = false;
    assert(alarm_network_allow_processing(&runtime, now_ms));
    assert(!runtime.background_waiting && runtime.network_phase == ALARM_NETWORK_ONLINE);

    /* 停止连接尚未完成时按下，必须先收尾，再恢复网络。 */
    prepare_offline();
    stop_ready = false;
    assert(!alarm_network_allow_processing(&runtime, now_ms));
    assert(runtime.network_phase == ALARM_NETWORK_STOPPING);
    runtime.button_wake_pending = 1;
    runtime.front_quiet = 0;
    assert(!alarm_network_allow_processing(&runtime, now_ms));
    stop_ready = true;
    assert(alarm_network_allow_processing(&runtime, now_ms));
    assert(runtime.network_phase == ALARM_NETWORK_ONLINE && runtime.platform_registered);
    prepare_offline();
    runtime.heartbeat_needed = true;
    runtime.control_storage_failed = true;
    now_ms += ALARM_BUTTON_NETWORK_ATTEMPT_MS;
    assert(alarm_network_allow_processing(&runtime, now_ms) && stop_count == 0);
    runtime.control_storage_failed = false;
    runtime.store.image.count = 1;
    before = published;
    alarm_send_registration_or_heartbeat(&runtime, now_ms);
    assert(published == before); /* 周期心跳不能抢在待处理报警之前。 */
    test_offline_business_cycle();
    puts("offline runtime: stop, retry budget, rollover, faults and wake races OK");
    return 0;
}
