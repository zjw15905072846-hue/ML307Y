/*------------------------------------------includes--------------------------------------------*/
#include <stdint.h>
/* 模拟器不执行原子交换扩展；只在测试替代单线程顺序语义。 */
static uint32_t sleep_exchange(uint32_t *address, uint32_t value);
#define __atomic_exchange_n(address, value, order) sleep_exchange(address, value)
#define main existing_runtime_main
#include "test_runtime.c"
#undef main
/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
static uint32_t observed_wait;
static unsigned telemetry_reads;
static uint32_t transport_wait = UINT32_MAX;
static unsigned front_releases;
static unsigned background_releases;
static bool front_held;
static bool background_held;
static bool notification_queue_full;
static alarm_runtime_t *wake_during_wait;
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : sleep_exchange
* Description    : 模拟原子交换顺序语义
* Input          : 测试参数或无
* Output         : 模拟状态及断言
* Return         : 模拟接口结果
* Attention      : 单线程交错注入不代表实板或真实 RTOS 并发验证
*******************************************************************************/
static uint32_t sleep_exchange(uint32_t *address, uint32_t value)
{
    uint32_t previous = *address;
    *address = value;
    return previous;
}

/*******************************************************************************
* Function Name  : sleep_front_power
* Description    : 记录前台锁需求
* Input          : 测试参数或无
* Output         : 模拟状态及断言
* Return         : 模拟接口结果
* Attention      : 单线程交错注入不代表实板或真实 RTOS 并发验证
*******************************************************************************/
static void sleep_front_power(void *user, bool hold)
{
    (void)user;
    front_held = hold;
    front_releases += !hold;
}

/*******************************************************************************
* Function Name  : sleep_background_power
* Description    : 记录后台锁需求
* Input          : 测试参数或无
* Output         : 模拟状态及断言
* Return         : 模拟接口结果
* Attention      : 单线程交错注入不代表实板或真实 RTOS 并发验证
*******************************************************************************/
static void sleep_background_power(void *user, bool hold)
{
    (void)user;
    background_held = hold;
    background_releases += !hold;
}

/*******************************************************************************
* Function Name  : sleep_put
* Description    : 注入队列满，通知仍保留原子标志
* Input          : 测试参数或无
* Output         : 模拟状态及断言
* Return         : 模拟接口结果
* Attention      : 单线程交错注入不代表实板或真实 RTOS 并发验证
*******************************************************************************/
static bool sleep_put(void *queue, const void *message, uint32_t timeout)
{
    (void)queue;
    (void)message;
    assert(timeout == 0);
    return !notification_queue_full;
}

/*******************************************************************************
* Function Name  : sleep_get
* Description    : 捕获阻塞期限并在入睡边界注入按键
* Input          : 测试参数或无
* Output         : 模拟状态及断言
* Return         : 模拟接口结果
* Attention      : 单线程交错注入不代表实板或真实 RTOS 并发验证
*******************************************************************************/
static bool sleep_get(void *queue, void *message, uint32_t timeout)
{
    (void)queue;
    (void)message;
    observed_wait = timeout;
    if (wake_during_wait)
    {
        alarm_notify_key_wakeup(wake_during_wait);
        wake_during_wait = NULL;
    }
    return false;
}

/*******************************************************************************
* Function Name  : sleep_next_wait
* Description    : 注入网络最近期限
* Input          : 测试参数或无
* Output         : 模拟状态及断言
* Return         : 模拟接口结果
* Attention      : 单线程交错注入不代表实板或真实 RTOS 并发验证
*******************************************************************************/
static uint32_t sleep_next_wait(void *user, uint32_t now)
{
    (void)user;
    (void)now;
    return transport_wait;
}

/*******************************************************************************
* Function Name  : sleep_set_notify
* Description    : 提供测试通知绑定接口
* Input          : 测试参数或无
* Output         : 模拟状态及断言
* Return         : 模拟接口结果
* Attention      : 单线程交错注入不代表实板或真实 RTOS 并发验证
*******************************************************************************/
static void sleep_set_notify(void *user, void (*notify)(void *), void *argument)
{
    (void)user;
    (void)notify;
    (void)argument;
}

/*******************************************************************************
* Function Name  : sleep_identity
* Description    : 统计真实采样接口的调用次数
* Input          : info - 输出地址
* Output         : 固定的虚构身份
* Return         : true
* Attention      : 等待同一心跳回执期间不应连续采样
*******************************************************************************/
static bool sleep_identity(device_info_t *info)
{
    ++telemetry_reads;
    memset(info, 0, sizeof(*info));
    return true;
}

/*******************************************************************************
* Function Name  : main
* Description    : 验证空闲长阻塞、故障和待确认阻止休眠、通知丢包防护及心跳期限
* Input          : 测试参数或无
* Output         : 模拟状态及断言
* Return         : 模拟接口结果
* Attention      : 单线程交错注入不代表实板或真实 RTOS 并发验证
*******************************************************************************/
int main(void)
{
    alarm_runtime_t sleeping = {0};
    product_services_t sleep_services = {0};
    kaiwan_transport_t sleep_transport = {0};
    unsigned releases;
    assert(existing_runtime_main() == 0);
    sleep_services.system.millis = clock_now;
    sleep_services.system.queue_get = sleep_get;
    sleep_services.system.queue_put = sleep_put;
    sleep_services.system.power_hold = sleep_front_power;
    sleep_services.system.power_background_hold = sleep_background_power;
    sleep_services.key.wake_configured = true;
    sleep_services.key.wake_verified = false;
    sleep_services.key.set_wakeup = sleep_set_notify;
    sleep_transport.set_notify = sleep_set_notify;
    sleep_transport.next_wait = sleep_next_wait;
    sleeping.services = &sleep_services;
    sleeping.transport = &sleep_transport;
    sleeping.button_state.background_idle = true;
    now_ms = UINT32_MAX - 1000U;
    sleeping.next_heartbeat_ms = now_ms + ALARM_BUTTON_HEARTBEAT_MS;
    alarm_wait_for_next_event(&sleeping, now_ms);
    assert(observed_wait == ALARM_BUTTON_HEARTBEAT_MS && front_held && front_releases == 1);
    releases = front_releases;
    sleeping.button_state.pending_save_count = 1;
    alarm_wait_for_next_event(&sleeping, now_ms);
    assert(observed_wait == 5U && front_releases == releases);
    sleeping.button_state.pending_save_count = 0;
    sleeping.button_state.background_idle = false;
    alarm_wait_for_next_event(&sleeping, now_ms);
    assert(observed_wait == 5U && front_releases == releases);
    sleeping.button_state.background_idle = true;
    sleeping.button_state.indicator.active = true;
    alarm_wait_for_next_event(&sleeping, now_ms);
    assert(observed_wait == 5U && front_releases == releases);
    sleeping.button_state.indicator.active = false;
    sleeping.button_state.key.candidate = true;
    alarm_wait_for_next_event(&sleeping, now_ms);
    assert(observed_wait == 5U && front_releases == releases);
    sleeping.button_state.key.candidate = false;
    sleeping.button_state.last_error = ALARM_ERROR_STORAGE;
    alarm_wait_for_next_event(&sleeping, now_ms);
    assert(observed_wait == 5U && front_releases == releases);
    sleeping.button_state.last_error = ALARM_OK;
    sleep_services.key.wake_configured = false;
    alarm_wait_for_next_event(&sleeping, now_ms);
    assert(observed_wait == 5U && front_releases == releases);
    sleep_services.key.wake_configured = true;
    notification_queue_full = true;
    alarm_notify_key_wakeup(&sleeping);
    assert(sleeping.key_pending == 1U);
    alarm_wait_for_next_event(&sleeping, now_ms);
    assert(observed_wait == 0 && front_releases == releases);
    sleeping.key_pending = 0;
    wake_during_wait = &sleeping;
    alarm_wait_for_next_event(&sleeping, now_ms);
    assert(sleeping.key_pending == 1U && front_held);
    sleeping.key_pending = 0;
    sleeping.next_heartbeat_ms = now_ms;
    alarm_wait_for_next_event(&sleeping, now_ms);
    assert(observed_wait == 0);
    sleeping.background_waiting = true;
    sleeping.last_heartbeat = now_ms;
    alarm_read_front_requests(&sleeping);
    /* 最后的零等待 drain 会覆盖 observed_wait，锁计数仍证明空闲释放。 */
    assert(background_releases == 1U && background_held);
    alarm_notify_network_event(&sleeping);
    assert(sleeping.network_pending == 1U);
    alarm_read_front_requests(&sleeping);
    assert(sleeping.network_pending == 0U && background_held);
    sleeping.background_waiting = false;
    releases = background_releases;
    alarm_read_front_requests(&sleeping);
    assert(background_releases == releases && background_held);
    /* 心跳已发出但没有回执，经过到期时刻也不能每轮重新采样。 */
    sleeping.storage_ready = true;
    sleeping.cloud_started = true;
    sleeping.platform_registered = true;
    sleeping.identity_ready = true;
    sleeping.heartbeat_needed = true;
    sleeping.waiting_control_reply = true;
    sleeping.control_sent_at_ms = now_ms;
    sleeping.last_heartbeat = now_ms - ALARM_BUTTON_HEARTBEAT_MS;
    sleep_services.system.identity = sleep_identity;
    sleep_services.system.fault = fault;
    sleep_transport.poll = poll_trial;
    sleep_transport.online = online_trial;
    alarm_process_background(&sleeping, now_ms);
    alarm_process_background(&sleeping, now_ms + 20U);
    assert(telemetry_reads == 0 && !sleeping.background_waiting);
    sleeping.telemetry_requested = true;
    alarm_collect_device_info(&sleeping, now_ms);
    alarm_collect_device_info(&sleeping, now_ms + 20U);
    assert(telemetry_reads == 1U);
    puts("sleep runtime: idle, wrap, unsaved/unconfirmed, outputs, key race and full notification queue passed");
    return 0;
}

