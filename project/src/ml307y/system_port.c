/*------------------------------------------includes--------------------------------------------*/
#include "ml307y/ml307y_port.h"
#include "ml307y/diag_uart.h"
#include "cm_os.h"
#include "cm_sys.h"
#include "cm_sim.h"
#include "cm_modem.h"
#include "cm_rtc.h"
#include "cm_pm.h"
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    osMutexId_t clock_lock; /* 保护跨任务共享的扩展计时状态。 */
    uint32_t tick_last;     /* 上次读取的 32 位内核节拍。 */
    uint64_t elapsed_ticks; /* 通过差值累计的节拍数。 */
    uint32_t frequency;     /* 每秒内核节拍数。 */
    bool held;              /* 当前是否持有电源工作锁。 */
} ml_system_t;

/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/* 当前底包提供的随机字节入口，失败时上层拒绝生成加密报文。 */
extern int project_random_bytes(uint8_t *data, size_t size);

/*******************************************************************************
* Function Name  : ml_ticks
* Description    : 将统一毫秒超时换算为CMSIS时钟节拍
* Input          : ms - 毫秒或永久等待
* Output         : 无
* Return         : SDK节拍数
* Attention      : 非零超时向上取整，保留永久等待值
*******************************************************************************/
static uint32_t ml_ticks(uint32_t ms)
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
* Function Name  : ml_millis
* Description    : 用独占时钟上下文扩展节拍回卷并输出毫秒
* Input          : user - 系统上下文
* Output         : 更新累计节拍
* Return         : 单调毫秒低32位
* Attention      : 任务上下文调用，短临界区不执行I/O
*******************************************************************************/
static uint32_t ml_millis(void *user)
{
    ml_system_t *s = user;
    uint32_t tick;
    uint32_t result;
    osMutexAcquire(s->clock_lock, osWaitForever);
    tick = osKernelGetTickCount();
    /* 无符号差值覆盖一次 32 位节拍回卷，再累计成 64 位运行时间。 */
    s->elapsed_ticks += (uint32_t)(tick - s->tick_last);
    s->tick_last = tick;
    result = (uint32_t)(s->elapsed_ticks * 1000U / s->frequency);
    osMutexRelease(s->clock_lock);
    return result;
}

/*******************************************************************************
* Function Name  : ml_delay
* Description    : 等待指定毫秒
* Input          : ms - 等待时间
* Output         : 任务休眠
* Return         : 无
* Attention      : 不用于声光忙等
*******************************************************************************/
static void ml_delay(uint32_t ms)
{
    osDelay(ml_ticks(ms));
}

/*******************************************************************************
* Function Name  : ml_queue_create
* Description    : 创建固定大小消息队列
* Input          : count - 深度；bytes - 消息字节数
* Output         : 新队列
* Return         : 队列句柄或NULL
* Attention      : 所有消息所有权由接口调用者管理
*******************************************************************************/
static void *ml_queue_create(unsigned count, unsigned bytes)
{
    return osMessageQueueNew(count, bytes, NULL);
}

/*******************************************************************************
* Function Name  : ml_queue_put
* Description    : 按统一毫秒超时投递消息
* Input          : queue - 队列；message - 内容；timeout_ms - 超时
* Output         : 消息副本
* Return         : true成功
* Attention      : 中断只能使用零超时
*******************************************************************************/
static bool ml_queue_put(void *queue, const void *message, uint32_t timeout_ms)
{
    return osMessageQueuePut(queue, message, 0, ml_ticks(timeout_ms)) == osOK;
}

/*******************************************************************************
* Function Name  : ml_queue_get
* Description    : 按统一毫秒超时取消息
* Input          : queue - 队列；message - 输出；timeout_ms - 超时
* Output         : message
* Return         : true收到消息
* Attention      : 队列空不伪造业务事件
*******************************************************************************/
static bool ml_queue_get(void *queue, void *message, uint32_t timeout_ms)
{
    return osMessageQueueGet(queue, message, NULL, ml_ticks(timeout_ms)) == osOK;
}

/*******************************************************************************
* Function Name  : ml_thread_start
* Description    : 创建前台或后台任务
* Input          : name - 名称；entry/argument - 入口；stack_bytes - 栈；foreground - 优先级
* Output         : SDK任务
* Return         : true成功
* Attention      : 入口创建失败由产品报告
*******************************************************************************/
static bool ml_thread_start(const char *name, void (*entry)(void *), void *argument,
                            unsigned stack_bytes, bool foreground)
{
    osThreadAttr_t attributes = {0};
    attributes.name = name;
    attributes.stack_size = stack_bytes;
    attributes.priority = foreground ? osPriorityAboveNormal : osPriorityNormal;
    return osThreadNew(entry, argument, &attributes) != NULL;
}

/*******************************************************************************
* Function Name  : ml_digits
* Description    : 验证身份字符串的固定数字长度
* Input          : text - 字符串；digits - 数字数量
* Output         : 无
* Return         : true合法
* Attention      : 不允许截断身份
*******************************************************************************/
static bool ml_digits(const char *text, unsigned digits)
{
    unsigned i;
    for (i = 0; i < digits; ++i)
    {
        if (text[i] < '0' || text[i] > '9')
        {
            return false;
        }
    }
    return text[digits] == 0;
}

/*******************************************************************************
* Function Name  : ml_identity
* Description    : 在后台读取设备身份及有效信号快照
* Input          : info - 输出容器
* Output         : IMEI、IMSI、ICCID、CSQ及RTC候选值
* Return         : true身份完整
* Attention      : RTC是否可信由产品配置决定；不记录身份或凭据
*******************************************************************************/
static bool ml_identity(device_info_t *info)
{
    char csq = 99;
    char ber = 99;
    uint64_t utc;
    memset(info, 0, sizeof(*info));
    if (cm_sys_get_imei(info->imei) != 0 || cm_sim_get_imsi(info->imsi) != 0 ||
        cm_sim_get_iccid(info->iccid) != 0 || !ml_digits(info->imei, 15) ||
        !ml_digits(info->imsi, 15) || !ml_digits(info->iccid, 20))
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
* Function Name  : ml_random
* Description    : 调用配套底包生成随机字节
* Input          : output/size - 输出缓冲
* Output         : 随机字节
* Return         : true成功
* Attention      : 不以固定字节补偿随机源失败
*******************************************************************************/
static bool ml_random(uint8_t *output, size_t size)
{
    return project_random_bytes(output, size) == 0;
}

/*******************************************************************************
* Function Name  : ml_fault
* Description    : 通过UART0输出不含凭据的模块故障码
* Input          : module - 固定模块名；error - 故障号
* Output         : SSCOM可查看的串口诊断文本
* Return         : 无
* Attention      : 禁止传入密钥、账号或报文内容
*******************************************************************************/
static void ml_fault(const char *module, int error)
{
    ml_uart_diag_printf("[project][%s] error=%d", module, error);
}

/*******************************************************************************
* Function Name  : ml_power_hold
* Description    : 按前台唯一所有者管理工作锁
* Input          : user - 系统上下文；hold - 保持唤醒
* Output         : 工作锁状态
* Return         : 无
* Attention      : 只允许普通休眠；深睡与唤醒能力需另外验证
*******************************************************************************/
static void ml_power_hold(void *user, bool hold)
{
    ml_system_t *s = user;
    /* 防止重复加锁或解锁，工作锁只由前台任务切换。 */
    if (hold && !s->held)
    {
        cm_pm_work_lock();
        s->held = true;
    }
    else if (!hold && s->held)
    {
        cm_pm_work_unlock();
        s->held = false;
    }
}

/*******************************************************************************
* Function Name  : ml_system_create
* Description    : 建立当前产品的系统服务与时钟上下文
* Input          : services - 服务容器
* Output         : 完整系统接口
* Return         : true成功
* Attention      : 创建后保持工作锁，业务静止且唤醒已核验才允许释放
*******************************************************************************/
bool ml_system_create(product_services_t *services)
{
    ml_system_t *s = calloc(1, sizeof(*s));
    cm_pm_sleep_mode_t mode = {CM_PM_SLEEP_MODE_LIGHT, false};
    if (!s)
    {
        return false;
    }
    s->clock_lock = osMutexNew(NULL);
    s->frequency = osKernelGetTickFreq();
    s->tick_last = osKernelGetTickCount();
    if (!s->clock_lock || !s->frequency)
    {
        free(s);
        return false;
    }
    services->system.user = s;
    services->system.millis = ml_millis;
    services->system.delay = ml_delay;
    services->system.queue_create = ml_queue_create;
    services->system.queue_put = ml_queue_put;
    services->system.queue_get = ml_queue_get;
    services->system.thread_start = ml_thread_start;
    services->system.allocate = malloc;
    services->system.release = free;
    services->system.identity = ml_identity;
    services->system.random = ml_random;
    services->system.fault = ml_fault;
    services->system.power_hold = ml_power_hold;
    ml_power_hold(s, true);
    if (cm_pm_set_cfg(CM_PM_CFG_SLEEPMODE, &mode) != 0)
    {
        ml_fault("power-mode", -1);
        /* 休眠模式配置失败仍保留工作锁，不能进入未经验证的深睡。 */
        return false;
    }
    return true;
}
