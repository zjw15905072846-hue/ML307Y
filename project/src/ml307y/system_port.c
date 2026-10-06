/*------------------------------------------includes--------------------------------------------*/
#include "ml307y/ml307y_port.h"
#include "ml307y/diag_uart.h"
#include "product_build_config.h"
#include "cm_os.h"
#include "cm_sys.h"
#include "cm_sim.h"
#include "cm_modem.h"
#include "cm_rtc.h"
#include "cm_pm.h"
#include "cm_mem.h"
#include <string.h>

/*-------------------------------------------define---------------------------------------------*/
#define ML307Y_PACKET_LOG_CHUNK 160U /* 注册 HEX 帧可单行打印，连同前缀仍小于 UART0 的 256 字节。 */
#define ML307Y_NETWORK_EVENT_COUNT 4U /* 联网回调零等待入队，后台负责串口输出。 */
#define ML307Y_PDP_DISCONNECT_TIMEOUT_MS 30000U /* 旧 PDP 未断开时保持工作锁并报告超时。 */
/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    osMutexId_t clock_lock; /* 保护跨任务共享的扩展计时状态。 */
    uint32_t tick_last;     /* 上次读取的 32 位内核节拍。 */
    uint64_t elapsed_ticks; /* 通过差值累计的节拍数。 */
    uint32_t frequency;     /* 每秒内核节拍数。 */
    osMutexId_t power_lock; /* 合并前后台工作需求，普通任务调用。 */
    bool front_held;
    bool background_held;
    bool radio_target; /* 最近一次射频请求。 */
    bool radio_switch_pending; /* 必须完成设置及读回后才能报告 OFF。 */
    bool radio_release_pending; /* 关网期间被唤醒仍先释放旧 PDP，再恢复射频。 */
    system_radio_state_t radio_status;
    int radio_error;
    uint32_t radio_deadline;
    uint32_t radio_stop_started;
    void (*radio_notify)(void *argument);
    void *radio_notify_argument;
    osMessageQueueId_t network_events;
    uint32_t network_events_dropped;
    uint32_t dial_count; /* 本次运行内收到的 CID 1 建连通知数，IP 相同也计入。 */
    bool held;              /* 当前是否持有电源工作锁。 */
} ml307y_system_state_t;

/*-------------------------------------------variables-------------------------------------------*/
static uint32_t power_status;
static uint32_t power_source;
static uint32_t power_transitions;
static uint32_t power_reported;
#if PRODUCT_HAS_MQTT
static ml307y_system_state_t *network_system_state; /* SDK 回调无 user 参数，唯一产品独占监视器。 */
#endif
/*-------------------------------------------function---------------------------------------------*/
/* 当前底包提供的随机字节入口，失败时上层拒绝生成加密报文。 */
extern int project_random_bytes(uint8_t *data, size_t size);

/*******************************************************************************
* Function Name  : ml307y_ticks
* Description    : 将统一毫秒超时换算为CMSIS时钟节拍
* Input          : ms - 毫秒或永久等待
* Output         : 无
* Return         : SDK节拍数
* Attention      : 非零超时向上取整，保留永久等待值
*******************************************************************************/
static uint32_t ml307y_ticks(uint32_t ms)
{
    uint64_t value;
    if (ms == SYSTEM_WAIT_FOREVER)
    {
        return osWaitForever;
    }
    value = ((uint64_t)ms * osKernelGetTickFreq() + 999U) / 1000U;
    return value >= UINT32_MAX ? UINT32_MAX - 1U : (uint32_t)value;
}

/*******************************************************************************
* Function Name  : ml307y_millis
* Description    : 用独占时钟上下文扩展节拍回卷并输出毫秒
* Input          : user - 系统上下文
* Output         : 更新累计节拍
* Return         : 单调毫秒低32位
* Attention      : 任务上下文调用，短临界区不执行I/O
*******************************************************************************/
static uint32_t ml307y_millis(void *user)
{
    ml307y_system_state_t *system_state = user;
    uint32_t tick;
    uint32_t result;
    osMutexAcquire(system_state->clock_lock, osWaitForever);
    tick = osKernelGetTickCount();
    /* 无符号差值覆盖一次 32 位节拍回卷，再累计成 64 位运行时间。 */
    system_state->elapsed_ticks += (uint32_t)(tick - system_state->tick_last);
    system_state->tick_last = tick;
    result = (uint32_t)(system_state->elapsed_ticks * 1000U / system_state->frequency);
    osMutexRelease(system_state->clock_lock);
    return result;
}

/*******************************************************************************
* Function Name  : ml307y_delay
* Description    : 等待指定毫秒
* Input          : ms - 等待时间
* Output         : 任务休眠
* Return         : 无
* Attention      : 不用于声光忙等
*******************************************************************************/
static void ml307y_delay(uint32_t ms)
{
    osDelay(ml307y_ticks(ms));
}

/*******************************************************************************
* Function Name  : ml307y_queue_create
* Description    : 创建固定大小消息队列
* Input          : count - 深度；bytes - 消息字节数
* Output         : 新队列
* Return         : 队列句柄或NULL
* Attention      : 所有消息所有权由接口调用者管理
*******************************************************************************/
static void *ml307y_queue_create(unsigned count, unsigned bytes)
{
    return osMessageQueueNew(count, bytes, NULL);
}

/*******************************************************************************
* Function Name  : ml307y_queue_put
* Description    : 按统一毫秒超时投递消息
* Input          : queue - 队列；message - 内容；timeout_ms - 超时
* Output         : 消息副本
* Return         : true成功
* Attention      : 中断只能使用零超时
*******************************************************************************/
static bool ml307y_queue_put(void *queue, const void *message, uint32_t timeout_ms)
{
    return osMessageQueuePut(queue, message, 0, ml307y_ticks(timeout_ms)) == osOK;
}

/*******************************************************************************
* Function Name  : ml307y_queue_get
* Description    : 按统一毫秒超时取消息
* Input          : queue - 队列；message - 输出；timeout_ms - 超时
* Output         : message
* Return         : true收到消息
* Attention      : 队列空不伪造业务事件
*******************************************************************************/
static bool ml307y_queue_get(void *queue, void *message, uint32_t timeout_ms)
{
    return osMessageQueueGet(queue, message, NULL, ml307y_ticks(timeout_ms)) == osOK;
}

/*******************************************************************************
* Function Name  : ml307y_thread_start
* Description    : 创建前台或后台任务
* Input          : name - 名称；entry/argument - 入口；stack_bytes - 栈；foreground - 优先级
* Output         : SDK任务
* Return         : true成功
* Attention      : 入口创建失败由产品报告
*******************************************************************************/
static bool ml307y_thread_start(const char *name, void (*entry)(void *), void *argument,
                            unsigned stack_bytes, bool foreground)
{
    osThreadAttr_t attributes = {0};
    osThreadId_t thread;
    attributes.name = name;
    attributes.stack_size = stack_bytes;
    attributes.priority = foreground ? osPriorityAboveNormal : osPriorityNormal;
    thread = osThreadNew(entry, argument, &attributes);
    if (!thread)
    {
        ml307y_uart_diag_printf("[project] task %s create failed", name);
        return false;
    }
    ml307y_uart_diag_printf("[project] task %s created", name);
    return true;
}

/*******************************************************************************
* Function Name  : ml307y_digits
* Description    : 验证身份字符串的固定数字长度
* Input          : text - 字符串；digits - 数字数量
* Output         : 无
* Return         : true合法
* Attention      : 不允许截断身份
*******************************************************************************/
static bool ml307y_digits(const char *text, unsigned digits)
{
    unsigned index;
    for (index = 0; index < digits; ++index)
    {
        if (text[index] < '0' || text[index] > '9')
        {
            return false;
        }
    }
    return text[digits] == 0;
}

/*******************************************************************************
* Function Name  : ml307y_identity
* Description    : 在后台读取设备身份及有效信号快照
* Input          : info - 输出容器
* Output         : IMEI、IMSI、ICCID、CSQ及RTC候选值
* Return         : true身份完整
* Attention      : RTC是否可信由产品配置决定；不记录身份或凭据
*******************************************************************************/
static bool ml307y_identity(device_info_t *info)
{
    char csq = 99;
    char ber = 99;
    uint64_t utc;
    memset(info, 0, sizeof(*info));
    if (cm_sys_get_imei(info->imei) != 0 || cm_sim_get_imsi(info->imsi) != 0 ||
        cm_sim_get_iccid(info->iccid) != 0 || !ml307y_digits(info->imei, 15) ||
        !ml307y_digits(info->imsi, 15) || !ml307y_digits(info->iccid, 20))
    {
        return false;
    }
    /* CSQ=99 表示未知；只接受模组返回的有效区间。 */
    if (cm_modem_get_csq(&csq, &ber) == 0 && (unsigned char)csq <= 31)
    {
        info->csq = (uint8_t)csq;
        info->csq_valid = true;
    }
    /* RTC 只提供候选值，是否信任仍由产品配置决定。 */
    utc = cm_rtc_get_current_time();
    if (utc >= 1577836800ULL && utc <= UINT32_MAX)
    {
        info->utc_seconds = (uint32_t)utc;
    }
    return true;
}

/*******************************************************************************
* Function Name  : ml307y_random
* Description    : 调用配套底包生成随机字节
* Input          : output/size - 输出缓冲
* Output         : 随机字节
* Return         : true成功
* Attention      : 不以固定字节补偿随机源失败
*******************************************************************************/
static bool ml307y_random(uint8_t *output, size_t size)
{
    return project_random_bytes(output, size) == 0;
}

/*******************************************************************************
* Function Name  : ml307y_fault
* Description    : 通过UART0输出不含凭据的模块故障码
* Input          : module - 固定模块名；error - 故障号
* Output         : SSCOM可查看的串口诊断文本
* Return         : 无
* Attention      : 禁止传入密钥、账号或报文内容
*******************************************************************************/
static void ml307y_fault(const char *module, int error)
{
    ml307y_uart_diag_printf("[project][%s] error=%d", module, error);
}

/*******************************************************************************
* Function Name  : ml307y_cloud_diagnostic
* Description    : 复用 UART0 入口打印云流程阶段和关联编号
* Input          : stage - 固定阶段名；value - 序号或事件 ID；result - 阶段结果
* Output         : 串口诊断文本
* Return         : 无
* Attention      : 不输出身份、账户、密钥或报文内容
*******************************************************************************/
static void ml307y_cloud_diagnostic(const char *stage, uint32_t value, int result)
{
    ml307y_uart_diag_printf("[project][cloud] stage=%s value=%u result=%d", stage, (unsigned)value, result);
}

/*******************************************************************************
* Function Name  : ml307y_packet_log
* Description    : 分行打印完整 MQTT 上行文本及实际传输入队结果
* Input          : kind - 注册/心跳/报警类别；sequence - 协议序号；topic - 主题
*                  payload/size - 实际发送文本及长度；result - 入队结果
* Output         : UART0 日志；报文分段以序号及字节偏移定位
* Return         : 无
* Attention      : 后台任务同步调用；不截断长报文，不把入队结果描述成平台确认
*******************************************************************************/
static void ml307y_packet_log(const char *kind, uint16_t sequence, const char *topic,
                              const uint8_t *payload, size_t size, int result)
{
    size_t offset;
    size_t chunk;
    if (!kind || !topic || !payload)
    {
        return;
    }
    ml307y_uart_diag_printf("[project][mqtt-tx] type=%s seq=%u bytes=%u queue_result=%d",
        kind, (unsigned)sequence, (unsigned)size, result);
    ml307y_uart_diag_printf("[project][mqtt-tx] seq=%u topic=%s", (unsigned)sequence, topic);
    for (offset = 0; offset < size; offset += chunk)
    {
        chunk = size - offset;
        if (chunk > ML307Y_PACKET_LOG_CHUNK)
        {
            chunk = ML307Y_PACKET_LOG_CHUNK;
        }
        if (ml307y_uart_diag_printf("[project][mqtt-tx] seq=%u offset=%u data=%.*s",
            (unsigned)sequence, (unsigned)offset, (int)chunk, (const char *)payload + offset) < 0)
        {
            ml307y_fault("mqtt-packet-log", -1);
            break;
        }
    }
}

/*******************************************************************************
* Function Name  : ml307y_sleep_changed
* Description    : 仅记录 SDK 休眠状态变化
* Input          : status/source - SDK 状态及唤醒源
* Output         : 原子状态与变化计数
* Return         : 无
* Attention      : 低功耗回调不打印、不申请锁、不执行业务
*******************************************************************************/
static void ml307y_sleep_changed(char status, int32_t source)
{
    uint32_t previous = __atomic_exchange_n(&power_status, (uint32_t)(unsigned char)status, __ATOMIC_ACQ_REL);
    __atomic_store_n(&power_source, (uint32_t)source, __ATOMIC_RELEASE);
    if (previous != (uint32_t)(unsigned char)status)
    {
        (void)__atomic_add_fetch(&power_transitions, 1U, __ATOMIC_RELEASE);
    }
}

/*******************************************************************************
* Function Name  : ml307y_power_diagnostic
* Description    : 在后台活动时打印上次空闲期间的休眠统计
* Input          : user - 保留
* Output         : UART0 诊断
* Return         : 无
* Attention      : 只由后台普通任务调用，不因打印增加周期唤醒
*******************************************************************************/
static void ml307y_power_diagnostic(void *user)
{
    uint32_t count = __atomic_load_n(&power_transitions, __ATOMIC_ACQUIRE);
    (void)user;
    if (count != power_reported)
    {
        power_reported = count;
        ml307y_uart_diag_printf("[project][power] transitions=%lu status=%lu source=%lu",
            (unsigned long)count,
            (unsigned long)__atomic_load_n(&power_status, __ATOMIC_ACQUIRE),
            (unsigned long)__atomic_load_n(&power_source, __ATOMIC_ACQUIRE));
    }
}

/*******************************************************************************
* Function Name  : ml307y_update_power_hold
* Description    : 合并前后台需求并串行切换唯一 SDK 工作锁
* Input          : system_state - 上下文；background - 所有者；hold - 保持唤醒
* Output         : 工作锁状态
* Return         : 无
* Attention      : 只允许普通任务；任一所有者忙碌都不能解锁
*******************************************************************************/
static void ml307y_update_power_hold(ml307y_system_state_t *system_state, bool background, bool hold)
{
    bool needed;
    osMutexAcquire(system_state->power_lock, osWaitForever);
    if (background)
    {
        system_state->background_held = hold;
    }
    else
    {
        system_state->front_held = hold;
    }
    needed = system_state->front_held || system_state->background_held;
    if (needed && !system_state->held)
    {
        cm_pm_work_lock();
        system_state->held = true;
    }
    else if (!needed && system_state->held)
    {
        cm_pm_work_unlock();
        system_state->held = false;
    }
    osMutexRelease(system_state->power_lock);
}

/*******************************************************************************
* Function Name  : ml307y_power_hold
* Description    : 更新前台工作锁需求
* Input          : user - 上下文；hold - 保持唤醒
* Output         : 合并后的工作锁
* Return         : 无
* Attention      : 不得解除另一个任务的工作需求
*******************************************************************************/
static void ml307y_power_hold(void *user, bool hold)
{
    ml307y_update_power_hold(user, false, hold);
}

#if PRODUCT_HAS_MQTT
/*******************************************************************************
* Function Name  : ml307y_network_connection_changed
* Description    : 保存运营商联网回调提供的本次 IPv4 和 IPv6 地址
* Input          : info - SDK 当前联网通知，仅在回调期间有效
* Output         : 有界队列及后台唤醒通知
* Return         : 无
* Attention      : 不拨号、不打印、不申请堆内存；不保证新 IP 与上一次不同
*******************************************************************************/
static void ml307y_network_connection_changed(cm_call_info_t *info)
{
    ml307y_system_state_t *system_state = __atomic_load_n(&network_system_state, __ATOMIC_ACQUIRE);
    cm_call_info_t event;
    void (*notify)(void *argument);
    if (!system_state || !info || info->cid != 1U || info->status > 1U)
    {
        return;
    }
    event = *info;
    event.ip[sizeof(event.ip) - 1U] = '\0';
    event.ipv6[sizeof(event.ipv6) - 1U] = '\0';
    if (osMessageQueuePut(system_state->network_events, &event, 0U, 0U) != osOK)
    {
        (void)__atomic_add_fetch(&system_state->network_events_dropped, 1U, __ATOMIC_RELEASE);
    }
    notify = __atomic_load_n(&system_state->radio_notify, __ATOMIC_ACQUIRE);
    if (notify)
    {
        notify(system_state->radio_notify_argument);
    }
}

/*******************************************************************************
* Function Name  : ml307y_network_monitor_start
* Description    : 为唯一产品绑定 SDK 自动与手动联网通知
* Input          : system_state - 生命周期覆盖产品任务的系统上下文
* Output         : 联网事件队列及 SDK 回调
* Return         : true - 已绑定；false - 已有所有者或队列创建失败
* Attention      : 仅初始化任务调用，不改变 APN 或 SDK 自动联网策略
*******************************************************************************/
static bool ml307y_network_monitor_start(ml307y_system_state_t *system_state)
{
    if (__atomic_load_n(&network_system_state, __ATOMIC_ACQUIRE))
    {
        return false;
    }
    system_state->network_events = osMessageQueueNew(ML307Y_NETWORK_EVENT_COUNT, sizeof(cm_call_info_t), NULL);
    if (!system_state->network_events)
    {
        return false;
    }
    __atomic_store_n(&network_system_state, system_state, __ATOMIC_RELEASE);
    cm_modem_set_call_callback(ml307y_network_connection_changed);
    return true;
}
#endif

/*******************************************************************************
* Function Name  : ml307y_network_report_connections
* Description    : 在后台打印每次数据连接的实际拨号 IP 或断开状态
* Input          : system_state - 系统上下文
* Output         : UART0 联网诊断与溢出错误
* Return         : 无
* Attention      : 每轮最多处理固定队列容量；空地址标为 unavailable，不伪造 IP
*******************************************************************************/
static void ml307y_network_report_connections(ml307y_system_state_t *system_state)
{
    cm_call_info_t event;
    uint32_t dropped;
    if (!system_state->network_events)
    {
        return;
    }
    dropped = __atomic_exchange_n(&system_state->network_events_dropped, 0U, __ATOMIC_ACQ_REL);
    if (dropped)
    {
        ml307y_uart_diag_printf("[project][dial-events] dropped=%lu", (unsigned long)dropped);
    }
    for (uint32_t index = 0U; index < ML307Y_NETWORK_EVENT_COUNT; ++index)
    {
        if (osMessageQueueGet(system_state->network_events, &event, NULL, 0U) != osOK)
        {
            break;
        }
        if (event.status == 0U)
        {
            ml307y_uart_diag_printf("[project][dial-network] cid=1 disconnected");
            continue;
        }
        ++system_state->dial_count;
        ml307y_uart_diag_printf("[project][dial-ip] cid=1 connection=%lu ipv4=%s ipv6=%s",
            (unsigned long)system_state->dial_count,
            event.ip[0] ? (const char *)event.ip : "unavailable",
            event.ipv6[0] ? (const char *)event.ipv6 : "unavailable");
    }
}

/*******************************************************************************
* Function Name  : ml307y_power_background_hold
* Description    : 更新后台工作锁需求并在持锁后输出实际联网通知
* Input          : user - 上下文；hold - 保持唤醒
* Output         : 合并后的工作锁及 UART0 IP 日志
* Return         : 无
* Attention      : 普通后台任务调用，IP 打印不依赖休眠统计开关
*******************************************************************************/
static void ml307y_power_background_hold(void *user, bool hold)
{
    ml307y_update_power_hold(user, true, hold);
    if (hold)
    {
        ml307y_network_report_connections(user);
    }
}


/*******************************************************************************
* Function Name  : ml307y_radio_request
* Description    : 记录射频目标，实际 AT 调用留给后台轮询
* Input          : user - 平台；enabled - 目标；now - 毫秒
* Output         : 状态及最近期限
* Return         : 无
* Attention      : 仅后台调用，不操作电源工作锁
*******************************************************************************/
static void ml307y_radio_request(void *user, bool enabled, uint32_t now)
{
    ml307y_system_state_t *state = user;
    state->radio_target = enabled;
    if (!enabled)
    {
        state->radio_release_pending = true;
    }
    state->radio_switch_pending = true;
    state->radio_status = enabled ? SYSTEM_RADIO_RESTORING : SYSTEM_RADIO_STOPPING;
    state->radio_error = 0;
    state->radio_deadline = now;
    state->radio_stop_started = now;
}

/*******************************************************************************
* Function Name  : ml307y_radio_poll
* Description    : 设置 CFUN 并核对读回，关网等待旧 PDP 断开，恢复等待新 PDP
* Input          : user - 平台；now - 单调毫秒
* Output         : 状态、错误及合并事件通知
* Return         : 无
* Attention      : 不在 ISR 调用；SDK AT 有自身超时，不等待注册业务
*******************************************************************************/
static void ml307y_radio_poll(void *user, uint32_t now)
{
    ml307y_system_state_t *state = user;
    system_radio_state_t previous = state->radio_status;
    int mode;
    int requested_mode;
    int result = 0;
    ml307y_network_report_connections(state);
    if ((!state->radio_switch_pending && state->radio_status != SYSTEM_RADIO_RESTORING &&
         state->radio_status != SYSTEM_RADIO_STOPPING) ||
        (int32_t)(now - state->radio_deadline) < 0)
    {
        return;
    }
    if (state->radio_switch_pending)
    {
        requested_mode = state->radio_target && !state->radio_release_pending ? 1 : 0;
        mode = cm_modem_get_cfun();
        if (mode != requested_mode)
        {
            result = cm_modem_set_cfun(requested_mode);
            mode = cm_modem_get_cfun();
        }
        if (result != 0 || mode != requested_mode)
        {
            state->radio_error = result != 0 ? result : -1;
            state->radio_status = SYSTEM_RADIO_ERROR;
            state->radio_deadline = now + 5000U;
        }
        else
        {
            state->radio_switch_pending = false;
            state->radio_error = 0;
            state->radio_status = requested_mode ? SYSTEM_RADIO_RESTORING : SYSTEM_RADIO_STOPPING;
        }
    }
    if (state->radio_status == SYSTEM_RADIO_STOPPING && !state->radio_switch_pending)
    {
        result = cm_modem_get_pdp_state(1);
        if (result == 0)
        {
            state->radio_release_pending = false;
            state->radio_status = state->radio_target ? SYSTEM_RADIO_RESTORING : SYSTEM_RADIO_OFF;
            state->radio_switch_pending = state->radio_target;
            state->radio_deadline = now;
        }
        else if (result < 0 || (uint32_t)(now - state->radio_stop_started) >= ML307Y_PDP_DISCONNECT_TIMEOUT_MS)
        {
            state->radio_error = result < 0 ? result : -2;
            state->radio_status = SYSTEM_RADIO_ERROR;
            state->radio_switch_pending = true;
            state->radio_stop_started = now;
            state->radio_deadline = now + 5000U;
        }
        else
        {
            state->radio_deadline = now + 1000U;
        }
    }
    if (state->radio_status == SYSTEM_RADIO_RESTORING && !state->radio_switch_pending)
    {
        if (cm_modem_get_pdp_state(1) == 1)
        {
            state->radio_status = SYSTEM_RADIO_READY;
        }
        state->radio_deadline = now + 1000U;
    }
    if (state->radio_status != previous && state->radio_notify)
    {
        state->radio_notify(state->radio_notify_argument);
    }
}

/*******************************************************************************
* Function Name  : ml307y_radio_state
* Description    : 读取后台拥有的射频状态
* Input          : user - 平台；error - 可选错误输出
* Output         : 最近 SDK 错误
* Return         : 射频状态
* Attention      : OFF 表示 CFUN 0 且 PDP 未激活，不证明实板电流或新 IP 已变化
*******************************************************************************/
static system_radio_state_t ml307y_radio_state(void *user, int *error)
{
    ml307y_system_state_t *state = user;
    if (error)
    {
        *error = state->radio_error;
    }
    return state->radio_status;
}

/*******************************************************************************
* Function Name  : ml307y_radio_next_wait
* Description    : 给出射频处理期限
* Input          : user - 平台；now - 毫秒
* Output         : 无
* Return         : 等待毫秒或永久等待
* Attention      : 停止后不再周期查询模组
*******************************************************************************/
static uint32_t ml307y_radio_next_wait(void *user, uint32_t now)
{
    ml307y_system_state_t *state = user;
    if (!state->radio_switch_pending && state->radio_status != SYSTEM_RADIO_RESTORING &&
        state->radio_status != SYSTEM_RADIO_STOPPING)
    {
        return SYSTEM_WAIT_FOREVER;
    }
    return (int32_t)(state->radio_deadline - now) <= 0 ? 0U : state->radio_deadline - now;
}

/*******************************************************************************
* Function Name  : ml307y_radio_set_notify
* Description    : 绑定后台事件通知
* Input          : user - 平台；notify - 回调；argument - 参数
* Output         : 通知绑定
* Return         : 无
* Attention      : 初始化时调用，通知必须零等待
*******************************************************************************/
static void ml307y_radio_set_notify(void *user, void (*notify)(void *), void *argument)
{
    ml307y_system_state_t *state = user;
    state->radio_notify_argument = argument;
    __atomic_store_n(&state->radio_notify, notify, __ATOMIC_RELEASE);
}

/*******************************************************************************
* Function Name  : ml307y_configure_sleep
* Description    : 设置允许的睡眠档位并核对 SDK 读回结果
* Input          : requested - SDK LIGHT 或 DEEP 模式
* Output         : 初始化日志记录请求、读回、失败阶段及返回码
* Return         : true - 设置和读回一致；false - 参数、设置或读回失败
* Attention      : 调用前必须持有工作锁；不写 Flash，不释放锁，不证明实际入睡
*******************************************************************************/
static bool ml307y_configure_sleep(int requested)
{
    cm_pm_sleep_mode_t mode = {requested, false};
    cm_pm_sleep_mode_t actual = {-1, false};
    const char *stage = "validate";
    int result = -1;
    if (requested == CM_PM_SLEEP_MODE_LIGHT || requested == CM_PM_SLEEP_MODE_DEEP)
    {
        stage = "callback";
        result = cm_pm_set_cfg(CM_PM_CFG_SLEEPIND, (void *)ml307y_sleep_changed);
        if (result == 0)
        {
            stage = "set";
            result = cm_pm_set_cfg(CM_PM_CFG_SLEEPMODE, &mode);
        }
        if (result == 0)
        {
            stage = "get";
            result = cm_pm_get_cfg(CM_PM_CFG_SLEEPMODE, &actual);
        }
        if (result == 0 && actual.mode != requested)
        {
            stage = "mismatch";
            result = -1;
        }
    }
    ml307y_uart_diag_printf("[project][sleep-mode] requested=%d readback=%d stage=%s result=%d",
                           requested, actual.mode, stage, result);
    return result == 0;
}

/*******************************************************************************
* Function Name  : ml307y_system_create
* Description    : 建立当前产品的系统服务与时钟上下文
* Input          : services - 服务容器
* Output         : 完整系统接口
* Return         : true成功
* Attention      : 创建后保持工作锁，业务静止且唤醒已核验才允许释放
*******************************************************************************/
bool ml307y_system_create(product_services_t *services)
{
    ml307y_system_state_t *system_state = cm_calloc(1, sizeof(*system_state));
    if (!system_state)
    {
        return false;
    }
    system_state->clock_lock = osMutexNew(NULL);
    system_state->power_lock = osMutexNew(NULL);
    system_state->frequency = osKernelGetTickFreq();
    system_state->tick_last = osKernelGetTickCount();
    if (!system_state->clock_lock || !system_state->power_lock || !system_state->frequency)
    {
        cm_free(system_state);
        return false;
    }
    services->system.user = system_state;
    services->system.millis = ml307y_millis;
    services->system.delay = ml307y_delay;
    services->system.queue_create = ml307y_queue_create;
    services->system.queue_put = ml307y_queue_put;
    services->system.queue_get = ml307y_queue_get;
    services->system.thread_start = ml307y_thread_start;
    services->system.allocate = cm_malloc;
    services->system.release = cm_free;
    services->system.identity = ml307y_identity;
    services->system.random = ml307y_random;
    services->system.fault = ml307y_fault;
    services->system.diagnostic = ml307y_cloud_diagnostic;
    services->system.packet_log = ml307y_packet_log;
    services->system.power_hold = ml307y_power_hold;
    services->system.power_background_hold = ml307y_power_background_hold;
    services->system.power_diagnostic = ml307y_power_diagnostic;
    services->system.radio_request = ml307y_radio_request;
    services->system.radio_poll = ml307y_radio_poll;
    services->system.radio_state = ml307y_radio_state;
    services->system.radio_next_wait = ml307y_radio_next_wait;
    services->system.radio_set_notify = ml307y_radio_set_notify;
    ml307y_power_hold(system_state, true);
    if (!ml307y_configure_sleep(ML307Y_SLEEP_MODE))
    {
        ml307y_fault("power-mode", -1);
        /* 设置或读回失败仍保留初始化工作锁，不能报告休眠已启用。 */
        return false;
    }
#if PRODUCT_HAS_MQTT
    if (!ml307y_network_monitor_start(system_state))
    {
        ml307y_fault("dial-monitor-init", -1);
        return false;
    }
#endif
    return true;
}
