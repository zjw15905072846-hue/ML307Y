/*------------------------------------------includes--------------------------------------------*/
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
/* RV64 模拟器不执行原子扩展；单线程测试只替代交换与加法的顺序语义。 */
static uint32_t power_test_exchange(uint32_t *address, uint32_t value);
#define __atomic_exchange_n(address, value, order) power_test_exchange(address, value)
#define __atomic_add_fetch(address, value, order) (*(address) += (value))
#define PRODUCT_HAS_MQTT 1 /* 本套件验证报警产品；量产宏由构建选择器生成。 */
#include "../../src/ml307y/system_port.c"
/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
static unsigned lock_calls;
static unsigned unlock_calls;
static unsigned print_calls;
static bool mutex_locked;
static int modem_mode = 1;
static bool mode_failure;
static bool wrong_readback;
static int pdp_ready;
static unsigned mode_calls;
static unsigned radio_notifications;
static int sleep_set_result;
static int sleep_get_result;
static int sleep_callback_result;
static int sleep_readback;
static int sleep_requested;
static unsigned sleep_set_calls;
static unsigned sleep_get_calls;
static cm_call_info_t network_queue[ML307Y_NETWORK_EVENT_COUNT];
static unsigned network_queue_head;
static unsigned network_queue_size;
static bool network_queue_failure;
static cm_modem_call_cb network_callback;
static unsigned network_callback_registrations;
static char printed_lines[32][256];
static unsigned printed_line_count;
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : power_test_exchange
* Description    : 模拟原子交换的单线程顺序语义
* Input          : address - 地址；value - 新值
* Output         : 更新地址内容
* Return         : 旧值
* Attention      : 不替代固件原子实现或证明并发安全
*******************************************************************************/
static uint32_t power_test_exchange(uint32_t *address, uint32_t value)
{
    uint32_t previous = *address;
    *address = value;
    return previous;
}

/*******************************************************************************
* Function Name  : osMutexAcquire
* Description    : 验证工作锁切换被互斥保护
* Input          : 模拟接口参数
* Output         : 断言与计数
* Return         : osOK
* Attention      : 仅验证工作锁所有权，不模拟芯片电流
*******************************************************************************/
osStatus_t osMutexAcquire(osMutexId_t mutex, uint32_t timeout)
{
    assert(mutex && timeout == osWaitForever && !mutex_locked);
    mutex_locked = true;
    return osOK;
}

/*******************************************************************************
* Function Name  : osMutexRelease
* Description    : 验证互斥锁释放配对
* Input          : 模拟接口参数
* Output         : 断言与计数
* Return         : osOK
* Attention      : 仅验证工作锁所有权，不模拟芯片电流
*******************************************************************************/
osStatus_t osMutexRelease(osMutexId_t mutex)
{
    assert(mutex && mutex_locked);
    mutex_locked = false;
    return osOK;
}

/*******************************************************************************
* Function Name  : cm_pm_work_lock
* Description    : 记录真实平台工作锁申请
* Input          : 模拟接口参数
* Output         : 断言与计数
* Return         : 无
* Attention      : 仅验证工作锁所有权，不模拟芯片电流
*******************************************************************************/
void cm_pm_work_lock(void)
{
    assert(mutex_locked);
    ++lock_calls;
}

/*******************************************************************************
* Function Name  : cm_pm_work_unlock
* Description    : 记录真实平台工作锁释放
* Input          : 模拟接口参数
* Output         : 断言与计数
* Return         : 无
* Attention      : 仅验证工作锁所有权，不模拟芯片电流
*******************************************************************************/
void cm_pm_work_unlock(void)
{
    assert(mutex_locked && unlock_calls < lock_calls);
    ++unlock_calls;
}

/*******************************************************************************
* Function Name  : ml307y_uart_diag_printf
* Description    : 记录诊断调用
* Input          : 模拟接口参数
* Output         : 断言与计数
* Return         : 0
* Attention      : 仅验证工作锁所有权，不模拟芯片电流
*******************************************************************************/
int ml307y_uart_diag_printf(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    assert(printed_line_count < 32U);
    assert(vsnprintf(printed_lines[printed_line_count], sizeof(printed_lines[0]), format, arguments) < 256);
    va_end(arguments);
    ++printed_line_count;
    ++print_calls;
    return 0;
}

/*******************************************************************************
* Function Name  : osMessageQueueNew
* Description    : 模拟固定容量联网通知队列及创建失败
* Input          : count - 容量；bytes - 元素大小；attr - 属性
* Output         : 清空模拟队列
* Return         : 模拟队列句柄或 NULL
* Attention      : 单线程模拟不证明实板并发安全
*******************************************************************************/
osMessageQueueId_t osMessageQueueNew(uint32_t count, uint32_t bytes, const osMessageQueueAttr_t *attr)
{
    assert(count == ML307Y_NETWORK_EVENT_COUNT && bytes == sizeof(cm_call_info_t) && attr == NULL);
    network_queue_head = 0U;
    network_queue_size = 0U;
    return network_queue_failure ? NULL : (void *)network_queue;
}

/*******************************************************************************
* Function Name  : osMessageQueuePut
* Description    : 模拟零等待入队，复制 SDK 回调缓冲并覆盖队列满
* Input          : queue - 队列；message - 通知；priority - 优先级；timeout - 超时
* Output         : 通知副本
* Return         : osOK 或 osErrorResource
* Attention      : 回调不得等待或打印
*******************************************************************************/
osStatus_t osMessageQueuePut(osMessageQueueId_t queue, const void *message, uint8_t priority, uint32_t timeout)
{
    assert(queue == (void *)network_queue && priority == 0U && timeout == 0U);
    if (network_queue_size == ML307Y_NETWORK_EVENT_COUNT)
    {
        return osErrorResource;
    }
    network_queue[(network_queue_head + network_queue_size) % ML307Y_NETWORK_EVENT_COUNT] = *(const cm_call_info_t *)message;
    ++network_queue_size;
    return osOK;
}

/*******************************************************************************
* Function Name  : osMessageQueueGet
* Description    : 模拟后台零等待消费联网通知
* Input          : queue - 队列；message - 输出；priority - 保留；timeout - 超时
* Output         : 通知副本
* Return         : osOK 或 osErrorResource
* Attention      : 只模拟有界队列语义
*******************************************************************************/
osStatus_t osMessageQueueGet(osMessageQueueId_t queue, void *message, uint8_t *priority, uint32_t timeout)
{
    assert(queue == (void *)network_queue && priority == NULL && timeout == 0U);
    if (!network_queue_size)
    {
        return osErrorResource;
    }
    *(cm_call_info_t *)message = network_queue[network_queue_head];
    network_queue_head = (network_queue_head + 1U) % ML307Y_NETWORK_EVENT_COUNT;
    --network_queue_size;
    return osOK;
}

/*******************************************************************************
* Function Name  : cm_modem_set_call_callback
* Description    : 保存 SDK 联网通知入口供测试注入
* Input          : callback - 联网回调
* Output         : 入口及绑定次数
* Return         : 无
* Attention      : 不联系运营商或修改实际自动联网设置
*******************************************************************************/
void cm_modem_set_call_callback(cm_modem_call_cb callback)
{
    assert(callback != NULL);
    network_callback = callback;
    ++network_callback_registrations;
}


/*******************************************************************************
* Function Name  : cm_modem_set_cfun
* Description    : 注入模式设置失败及错误读回
* Input          : 模拟接口参数
* Output         : 模拟状态
* Return         : 模拟返回值
* Attention      : 不访问射频硬件
*******************************************************************************/
int32_t cm_modem_set_cfun(uint16_t mode)
{
    ++mode_calls;
    if (mode_failure)
    {
        return -7;
    }
    if (!wrong_readback)
    {
        modem_mode = mode;
    }
    return 0;
}

/*******************************************************************************
* Function Name  : cm_modem_get_cfun
* Description    : 返回实际读回值
* Input          : 模拟接口参数
* Output         : 模拟状态
* Return         : 模拟返回值
* Attention      : 不访问射频硬件
*******************************************************************************/
int32_t cm_modem_get_cfun(void)
{
    return modem_mode;
}

/*******************************************************************************
* Function Name  : cm_modem_get_pdp_state
* Description    : 注入恢复联网延迟
* Input          : 模拟接口参数
* Output         : 模拟状态
* Return         : 模拟返回值
* Attention      : 不访问射频硬件
*******************************************************************************/
int32_t cm_modem_get_pdp_state(uint16_t cid)
{
    assert(cid == 1);
    return pdp_ready;
}

/*******************************************************************************
* Function Name  : radio_notification
* Description    : 计数模式变化通知
* Input          : 模拟接口参数
* Output         : 模拟状态
* Return         : 模拟返回值
* Attention      : 不访问射频硬件
*******************************************************************************/
static void radio_notification(void *argument)
{
    (void)argument;
    ++radio_notifications;
}

/*******************************************************************************
* Function Name  : test_radio_transitions
* Description    : 验证 CFUN 设置与读回、失败重试、PDP 恢复及回卷
* Input          : 模拟接口参数
* Output         : 模拟状态
* Return         : 模拟返回值
* Attention      : 不访问射频硬件
*******************************************************************************/
static void test_radio_transitions(void)
{
    ml307y_system_state_t state = {0};
    int error;
    uint32_t now = UINT32_MAX - 2000U;
    ml307y_radio_set_notify(&state, radio_notification, NULL);
    ml307y_radio_request(&state, false, now);
    assert(ml307y_radio_state(&state, &error) == SYSTEM_RADIO_STOPPING);
    mode_failure = true;
    ml307y_radio_poll(&state, now);
    assert(ml307y_radio_state(&state, &error) == SYSTEM_RADIO_ERROR && error == -7);
    assert(ml307y_radio_next_wait(&state, now) == 5000U);
    ml307y_radio_poll(&state, now + 4999U);
    assert(mode_calls == 1);
    mode_failure = false;
    wrong_readback = true;
    now += 5000U;
    ml307y_radio_poll(&state, now);
    assert(ml307y_radio_state(&state, &error) == SYSTEM_RADIO_ERROR);
    wrong_readback = false;
    now += 5000U;
    ml307y_radio_poll(&state, now);
    assert(ml307y_radio_state(&state, &error) == SYSTEM_RADIO_OFF && error == 0);
    assert(ml307y_radio_next_wait(&state, now) == SYSTEM_WAIT_FOREVER);
    ml307y_radio_request(&state, true, now);
    ml307y_radio_poll(&state, now);
    assert(ml307y_radio_state(&state, &error) == SYSTEM_RADIO_RESTORING);
    pdp_ready = 1;
    ml307y_radio_poll(&state, now + 1000U);
    assert(ml307y_radio_state(&state, &error) == SYSTEM_RADIO_READY);
    assert(radio_notifications == 3);
}

/*******************************************************************************
* Function Name  : test_pdp_disconnect_before_redial
* Description    : 确认 CFUN 已关闭但旧 PDP 未断开时不能宣布关网完成
* Input          : 无
* Output         : 断言
* Return         : 无
* Attention      : 模拟状态不证明运营商重新分配了不同的 IP
*******************************************************************************/
static void test_pdp_disconnect_before_redial(void)
{
    ml307y_system_state_t state = {0};
    uint32_t now = UINT32_MAX - 500U;
    int error;
    modem_mode = 1;
    pdp_ready = 1;
    ml307y_radio_request(&state, false, now);
    ml307y_radio_poll(&state, now);
    assert(modem_mode == 0);
    assert(ml307y_radio_state(&state, &error) == SYSTEM_RADIO_STOPPING);
    assert(ml307y_radio_next_wait(&state, now) == 1000U);
    pdp_ready = 0;
    ml307y_radio_poll(&state, now + 1000U);
    assert(ml307y_radio_state(&state, &error) == SYSTEM_RADIO_OFF);
    ml307y_radio_request(&state, true, now + 1000U);
    ml307y_radio_poll(&state, now + 1000U);
    assert(modem_mode == 1);
    assert(ml307y_radio_state(&state, &error) == SYSTEM_RADIO_RESTORING);
    pdp_ready = 1;
    ml307y_radio_poll(&state, now + 2000U);
    assert(ml307y_radio_state(&state, &error) == SYSTEM_RADIO_READY);
}

/*******************************************************************************
* Function Name  : test_pdp_disconnect_errors
* Description    : 覆盖旧 PDP 查询失败与持续激活超时后的重试
* Input          : 无
* Output         : 断言
* Return         : 无
* Attention      : 失败不得宣称旧 IP 会话已释放，时钟回卷仍须正常处理
*******************************************************************************/
static void test_pdp_disconnect_errors(void)
{
    ml307y_system_state_t state = {0};
    uint32_t now = UINT32_MAX - 2000U;
    int error;
    modem_mode = 1;
    pdp_ready = -8;
    ml307y_radio_request(&state, false, now);
    ml307y_radio_poll(&state, now);
    assert(ml307y_radio_state(&state, &error) == SYSTEM_RADIO_ERROR && error == -8);
    assert(ml307y_radio_next_wait(&state, now) == 5000U);
    pdp_ready = 1;
    now += 5000U;
    ml307y_radio_poll(&state, now);
    assert(ml307y_radio_state(&state, &error) == SYSTEM_RADIO_STOPPING);
    now += ML307Y_PDP_DISCONNECT_TIMEOUT_MS;
    ml307y_radio_poll(&state, now);
    assert(ml307y_radio_state(&state, &error) == SYSTEM_RADIO_ERROR && error == -2);
    pdp_ready = 0;
    ml307y_radio_poll(&state, now + 5000U);
    assert(ml307y_radio_state(&state, &error) == SYSTEM_RADIO_OFF && error == 0);
}

/*******************************************************************************
* Function Name  : test_wake_during_pdp_disconnect
* Description    : 覆盖关网未完成时新报警唤醒，恢复不得复用未释放的旧 PDP
* Input          : 无
* Output         : 断言
* Return         : 无
* Attention      : 报警请求由业务保留；此处只检查关闭再恢复的网络顺序
*******************************************************************************/
static void test_wake_during_pdp_disconnect(void)
{
    ml307y_system_state_t state = {0};
    uint32_t now = 1000U;
    modem_mode = 1;
    pdp_ready = 1;
    ml307y_radio_request(&state, false, now);
    ml307y_radio_poll(&state, now);
    ml307y_radio_request(&state, true, now + 100U);
    ml307y_radio_poll(&state, now + 100U);
    assert(modem_mode == 0 && state.radio_status == SYSTEM_RADIO_STOPPING);
    pdp_ready = 0;
    ml307y_radio_poll(&state, now + 1100U);
    assert(modem_mode == 0 && state.radio_status == SYSTEM_RADIO_RESTORING);
    ml307y_radio_poll(&state, now + 1100U);
    assert(modem_mode == 1 && state.radio_status == SYSTEM_RADIO_RESTORING);
    pdp_ready = 1;
    ml307y_radio_poll(&state, now + 2100U);
    assert(state.radio_status == SYSTEM_RADIO_READY);
}

/*******************************************************************************
* Function Name  : test_network_ip_notifications
* Description    : 覆盖每次拨号的 IP 输出、相同地址、缓冲副本和队列溢出
* Input          : 无
* Output         : 断言及 UART0 模拟文本
* Return         : 无
* Attention      : 相同 IP 不触发额外拨号；回调不直接打印；不证明实板 IP 分配
*******************************************************************************/
static void test_network_ip_notifications(void)
{
    ml307y_system_state_t state = {0};
    cm_call_info_t event = {1U, 1U, "10.20.30.40", "2001:db8::1"};
    unsigned prints;
    unsigned modes = mode_calls;
    state.power_lock = (void *)1;
    network_queue_failure = true;
    assert(!ml307y_network_monitor_start(&state));
    assert(network_callback_registrations == 0U && network_system_state == NULL);
    network_queue_failure = false;
    assert(ml307y_network_monitor_start(&state));
    assert(!ml307y_network_monitor_start(&state));
    assert(network_callback_registrations == 1U);
    ml307y_radio_set_notify(&state, radio_notification, NULL);
    printed_line_count = 0U;
    prints = print_calls;
    network_callback(NULL);
    event.cid = 8U;
    network_callback(&event);
    event.cid = 1U;
    event.status = 2U;
    network_callback(&event);
    assert(network_queue_size == 0U && print_calls == prints);
    event.status = 1U;
    network_callback(&event);
    strcpy((char *)event.ip, "10.20.30.41");
    assert(print_calls == prints);
    ml307y_power_background_hold(&state, true);
    assert(strstr(printed_lines[0], "connection=1 ipv4=10.20.30.40 ipv6=2001:db8::1"));
    event.status = 0U;
    network_callback(&event);
    event.status = 1U;
    network_callback(&event);
    network_callback(&event);
    ml307y_power_background_hold(&state, true);
    assert(strstr(printed_lines[1], "disconnected"));
    assert(strstr(printed_lines[2], "connection=2 ipv4=10.20.30.41"));
    assert(strstr(printed_lines[3], "connection=3 ipv4=10.20.30.41"));
    assert(mode_calls == modes);
    prints = print_calls;
    ml307y_power_background_hold(&state, true);
    assert(print_calls == prints);
    memset(event.ip, 0, sizeof(event.ip));
    memset(event.ipv6, 0, sizeof(event.ipv6));
    network_callback(&event);
    ml307y_network_report_connections(&state);
    assert(strstr(printed_lines[4], "ipv4=unavailable ipv6=unavailable"));
    memset(event.ip, '1', sizeof(event.ip));
    memset(event.ipv6, '2', sizeof(event.ipv6));
    for (unsigned index = 0U; index <= ML307Y_NETWORK_EVENT_COUNT; ++index)
    {
        network_callback(&event);
    }
    assert(print_calls == prints + 1U && state.network_events_dropped == 1U);
    ml307y_network_report_connections(&state);
    assert(strstr(printed_lines[5], "dropped=1"));
    assert(strlen(printed_lines[6]) < 160U);
    assert(state.network_events_dropped == 0U && network_queue_size == 0U);
    ml307y_power_background_hold(&state, false);
    network_system_state = NULL;
    network_callback(NULL);
}

/*******************************************************************************
* Function Name  : cm_pm_set_cfg
* Description    : 注入回调或模式设置失败
* Input          : type/info - SDK配置
* Output         : 记录请求模式及调用次数
* Return         : 模拟SDK结果
* Attention      : 不改变任何真实电源配置
*******************************************************************************/
int cm_pm_set_cfg(int type, void *info)
{
    if (type == CM_PM_CFG_SLEEPIND)
    {
        assert(info != NULL);
        return sleep_callback_result;
    }
    assert(type == CM_PM_CFG_SLEEPMODE);
    cm_pm_sleep_mode_t *mode = info;
    assert(!mode->permanent);
    sleep_requested = mode->mode;
    ++sleep_set_calls;
    return sleep_set_result;
}

/*******************************************************************************
* Function Name  : cm_pm_get_cfg
* Description    : 模拟读回错误及模式不一致
* Input          : type/info - SDK配置
* Output         : 读回模式及计数
* Return         : 模拟SDK结果
* Attention      : 不访问模组
*******************************************************************************/
int cm_pm_get_cfg(int type, void *info)
{
    assert(type == CM_PM_CFG_SLEEPMODE);
    ++sleep_get_calls;
    ((cm_pm_sleep_mode_t *)info)->mode = sleep_readback;
    return sleep_get_result;
}

/*******************************************************************************
* Function Name  : test_sleep_mode_validation
* Description    : 验证编译默认值、两档设置、失败读回及持锁
* Input          : 无
* Output         : 断言
* Return         : 无
* Attention      : 模式配置成功不等同真实睡眠
*******************************************************************************/
static void test_sleep_mode_validation(void)
{
    ml307y_system_state_t state = {0};
    unsigned releases;
    unsigned calls;
    state.power_lock = (void *)1;
    ml307y_power_hold(&state, true);
    releases = unlock_calls;
    sleep_readback = ML307Y_SLEEP_MODE;
    assert(ml307y_configure_sleep(ML307Y_SLEEP_MODE));
    assert(sleep_requested == ML307Y_SLEEP_MODE);
    for (int mode = CM_PM_SLEEP_MODE_LIGHT; mode <= CM_PM_SLEEP_MODE_DEEP; ++mode)
    {
        sleep_readback = mode;
        assert(ml307y_configure_sleep(mode));
        assert(sleep_requested == mode && state.held && unlock_calls == releases);
        sleep_set_result = -7;
        calls = sleep_get_calls;
        assert(!ml307y_configure_sleep(mode));
        assert(sleep_get_calls == calls);
        sleep_set_result = 0;
        sleep_get_result = -8;
        assert(!ml307y_configure_sleep(mode));
        sleep_get_result = 0;
        sleep_readback = mode == CM_PM_SLEEP_MODE_LIGHT ? CM_PM_SLEEP_MODE_DEEP : CM_PM_SLEEP_MODE_LIGHT;
        assert(!ml307y_configure_sleep(mode));
        assert(state.held && unlock_calls == releases);
    }
    calls = sleep_set_calls;
    assert(!ml307y_configure_sleep(-1));
    assert(!ml307y_configure_sleep(CM_PM_SLEEP_MODE_ACTIVE));
    assert(sleep_set_calls == calls);
    sleep_callback_result = -9;
    assert(!ml307y_configure_sleep(CM_PM_SLEEP_MODE_DEEP));
    assert(sleep_set_calls == calls && state.held && unlock_calls == releases);
    ml307y_power_hold(&state, false);
}
/*******************************************************************************
* Function Name  : main
* Description    : 验证两种任务交错顺序、幂等持锁及回调零打印
* Input          : 模拟接口参数
* Output         : 断言与计数
* Return         : 0 表示通过
* Attention      : 仅验证工作锁所有权，不模拟芯片电流
*******************************************************************************/
int main(void)
{
    ml307y_system_state_t state = {0};
    state.power_lock = (void *)1;
    ml307y_power_hold(&state, true);
    ml307y_power_hold(&state, true);
    ml307y_power_background_hold(&state, true);
    assert(lock_calls == 1 && unlock_calls == 0);
    ml307y_power_hold(&state, false);
    assert(state.held && unlock_calls == 0);
    ml307y_power_background_hold(&state, false);
    assert(!state.held && unlock_calls == 1);
    ml307y_power_background_hold(&state, true);
    ml307y_power_hold(&state, true);
    ml307y_power_background_hold(&state, false);
    assert(state.held && unlock_calls == 1);
    ml307y_power_hold(&state, false);
    ml307y_power_hold(&state, false);
    assert(lock_calls == 2 && unlock_calls == 2);
    ml307y_sleep_changed(1, 0);
    ml307y_sleep_changed(1, 0);
    ml307y_sleep_changed(0, 26);
    assert(print_calls == 0 && power_transitions == 2);
    ml307y_power_diagnostic(&state);
    ml307y_power_diagnostic(&state);
    assert(print_calls == 1);
    test_radio_transitions();
    test_pdp_disconnect_before_redial();
    test_pdp_disconnect_errors();
    test_wake_during_pdp_disconnect();
    test_sleep_mode_validation();
    test_network_ip_notifications();
    puts("power: ownership, sleep, PDP release and per-connection IP reporting passed");
    return 0;
}

