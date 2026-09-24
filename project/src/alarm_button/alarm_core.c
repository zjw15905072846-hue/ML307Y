/*------------------------------------------includes--------------------------------------------*/
#include "alarm_button/alarm_core.h"
#include <string.h>
/*-------------------------------------------define---------------------------------------------*/
#define ALARM_MAGIC 0x414C5232U /* 一键报警持久镜像标识。 */
#define ALARM_VERSION 2U        /* 镜像结构版本，布局变化须升级。 */
/*-------------------------------------------typedef---------------------------------------------*/
/* 编译期限制镜像大小，避免超出单个 FTL 记录容量。 */
typedef char alarm_image_must_fit_ftl[(sizeof(alarm_storage_image_t) <= 4080) ? 1 : -1];

/*-------------------------------------------variables-------------------------------------------*/
/* 全部可变状态归调用方上下文，无全局单例。 */
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : alarm_crc
* Description    : 计算镜像CRC32，跳过校验字段自身
* Input          : image - 完整持久镜像
* Output         : 无
* Return         : CRC32校验值
* Attention      : CRC字段自身按零参与；镜像布局变动必须升级格式
*******************************************************************************/
static uint32_t alarm_crc(const alarm_storage_image_t *image)
{
    const uint8_t *bytes = (const uint8_t *)image;
    uint32_t crc = 0xffffffffU;
    size_t index;
    unsigned bit_index;
    for (index = 0; index < sizeof(*image); index++)
    {
        uint8_t byte_value =
            (index >= offsetof(alarm_storage_image_t, crc) && index < offsetof(alarm_storage_image_t, crc) + 4) ? 0 : bytes[index];
        crc ^= byte_value;
        for (bit_index = 0; bit_index < 8; bit_index++)
        {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0);
        }
    }
    return ~crc;
}

/*******************************************************************************
* Function Name  : alarm_commit
* Description    : 写入候选镜像，成功后才替换可见状态
* Input          : store - 含候选镜像的存储上下文
* Output         : 成功后更新image；失败保留旧image
* Return         : ALARM_OK或ALARM_ERROR_STORAGE
* Attention      : 端口write须提供掉电安全提交；本函数不覆盖失败前可见状态
*******************************************************************************/
static int alarm_commit(alarm_event_store_t *store)
{
    store->staging.crc = alarm_crc(&store->staging);
    if (!store->port.write(store->port.user, &store->staging, sizeof(store->staging)))
    {
        return ALARM_ERROR_STORAGE;
    }
    store->image = store->staging;
    return ALARM_OK;
}

/*******************************************************************************
* Function Name  : alarm_store_open
* Description    : 恢复并检查产品身份、结构版本和记录完整性
* Input          : store - 输出上下文；product_id - 产品身份；port - 存储接口
* Output         : store - 经身份、格式与CRC校验的队列
* Return         : ALARM_OK或明确的存储/身份/格式错误
* Attention      : 仅ALARM_STORAGE_EMPTY允许初始化；未知镜像不得擦除
*******************************************************************************/
int alarm_store_open(alarm_event_store_t *store, uint32_t product_id, const alarm_storage_port_t *port)
{
    int result;
    unsigned index;
    if (!store || !port || !port->read || !port->write || !product_id)
    {
        return ALARM_ERROR_ARGUMENT;
    }
    memset(store, 0, sizeof(*store));
    store->port = *port;
    result = port->read(port->user, &store->image, sizeof(store->image));
    /* 只有明确报告为空白才能建立初始镜像；损坏或外来数据留待人工处理。 */
    if (result == ALARM_STORAGE_EMPTY)
    {
        memset(&store->staging, 0, sizeof(store->staging));
        store->staging.magic = ALARM_MAGIC;
        store->staging.version = ALARM_VERSION;
        store->staging.product_id = product_id;
        store->staging.bytes = (uint16_t)sizeof(alarm_storage_image_t);
        store->staging.next_id = 1;
        store->staging.next_sequence = 1;
        result = alarm_commit(store);
        if (result)
        {
            return result;
        }
    }
    else if (result != ALARM_OK)
    {
        if (result == STORAGE_FOREIGN)
        {
            return ALARM_ERROR_FOREIGN;
        }
        if (result == STORAGE_CORRUPT)
        {
            return ALARM_ERROR_CORRUPT;
        }
        return ALARM_ERROR_STORAGE;
    }
    else
    {
        if (store->image.magic != ALARM_MAGIC || store->image.product_id != product_id ||
            store->image.version != ALARM_VERSION)
        {
            return ALARM_ERROR_FOREIGN;
        }
        if (store->image.bytes != sizeof(alarm_storage_image_t) || store->image.count > ALARM_CAPACITY ||
            store->image.crc != alarm_crc(&store->image) || !store->image.next_id ||
            !store->image.next_sequence || store->image.next_sequence > 65536U)
        {
            return ALARM_ERROR_CORRUPT;
        }
        for (index = 0; index < store->image.count; index++)
        {
            if (!store->image.events[index].id || store->image.events[index].id >= store->image.next_id ||
                (index && store->image.events[index].id <= store->image.events[index - 1].id))
            {
                return ALARM_ERROR_CORRUPT;
            }
        }
    }
    store->ready = true;
    return ALARM_OK;
}

/*******************************************************************************
* Function Name  : alarm_store_enqueue
* Description    : 持久化独立事件，失败时不消耗ID也不改变队列
* Input          : store - 队列；event - 事件快照；id - 输出ID指针
* Output         : 持久成功才返回非零id
* Return         : ALARM_OK或满队列、写入失败、ID耗尽错误
* Attention      : 不覆盖未确认报警；持久化完成后方可发送
*******************************************************************************/
int alarm_store_enqueue(alarm_event_store_t *store, const alarm_event_t *event, uint32_t *id)
{
    int result;
    if (id)
    {
        *id = 0;
    }
    if (!store || !event || !id)
    {
        return ALARM_ERROR_ARGUMENT;
    }
    if (!store->ready)
    {
        return ALARM_ERROR_NOT_READY;
    }
    if (store->image.count == ALARM_CAPACITY)
    {
        return ALARM_ERROR_FULL;
    }
    if (store->image.next_id == UINT32_MAX)
    {
        return ALARM_ERROR_EXHAUSTED;
    }
    store->staging = store->image;
    /* 先写候选镜像，写失败时旧队列和下一个事件 ID 均不变化。 */
    store->staging.events[store->staging.count] = *event;
    store->staging.events[store->staging.count++].id = store->staging.next_id++;
    result = alarm_commit(store);
    if (!result)
    {
        *id = store->image.next_id - 1;
    }
    return result;
}

/*******************************************************************************
* Function Name  : alarm_store_remove
* Description    : 仅在平台确认后原子删除指定队首
* Input          : store - 队列；id - 已业务确认的队首事件
* Output         : 原子删除匹配队首，保留其他事件
* Return         : ALARM_OK或过期/存储错误
* Attention      : 调用方必须先验证平台业务回执
*******************************************************************************/
int alarm_store_remove(alarm_event_store_t *store, uint32_t id)
{
    if (!store || !store->ready)
    {
        return ALARM_ERROR_NOT_READY;
    }
    if (!store->image.count || store->image.events[0].id != id)
    {
        return ALARM_ERROR_STALE;
    }
    store->staging = store->image;
    /* 只删除已确认的队首，其他记录仍按原顺序保留。 */
    store->staging.count--;
    memmove(store->staging.events, store->staging.events + 1,
            store->staging.count * sizeof(alarm_event_t));
    memset(&store->staging.events[store->staging.count], 0, sizeof(alarm_event_t));
    return alarm_commit(store);
}

/*******************************************************************************
* Function Name  : alarm_store_sequence
* Description    : 持久分配序列，跨重启不复用；耗尽时显式失败
* Input          : store - 持久队列；sequence - 输出协议序号
* Output         : 成功后提交递增计数并输出序号
* Return         : ALARM_OK或存储/序号耗尽错误
* Attention      : 跨重启不复用序号；回卷规则未确认时显式停止分配
*******************************************************************************/
int alarm_store_sequence(alarm_event_store_t *store, uint16_t *sequence)
{
    int result;
    if (!store || !sequence || !store->ready)
    {
        return ALARM_ERROR_NOT_READY;
    }
    if (store->image.next_sequence > 65535U)
    {
        return ALARM_ERROR_EXHAUSTED;
    }
    store->staging = store->image;
    store->staging.next_sequence++;
    result = alarm_commit(store);
    if (!result)
    {
        *sequence = (uint16_t)(store->image.next_sequence - 1);
    }
    return result;
}

/*******************************************************************************
* Function Name  : alarm_reporter_init
* Description    : 初始化串行队首发送及有限间隔重试
* Input          : context - 上报上下文；store/send/user - 依赖与回调；其余参数为毫秒超时
* Output         : context - 独立的发送、重试及序号关联状态
* Return         : 无；非法配置记入last_error
* Attention      : send只表示进入传输队列；不能等同业务成功
*******************************************************************************/
void alarm_reporter_init(alarm_event_reporter_t *context, alarm_event_store_t *store, alarm_send_callback_t send, void *user,
                      uint32_t confirmation_ms, uint32_t minimum_retry_ms, uint32_t maximum_retry_ms)
{
    if (!context)
    {
        return;
    }
    memset(context, 0, sizeof(*context));
    context->store = store;
    context->send = send;
    context->user = user;
    context->confirmation_ms = confirmation_ms;
    context->minimum_ms = minimum_retry_ms;
    context->maximum_ms = maximum_retry_ms;
    if (!confirmation_ms || !minimum_retry_ms || maximum_retry_ms < minimum_retry_ms || maximum_retry_ms >= 0x80000000U ||
        confirmation_ms >= 0x80000000U)
    {
        context->last_error = ALARM_ERROR_CONFIG;
    }
}

/*******************************************************************************
* Function Name  : alarm_retry
* Description    : 失败后安排退避，保持报警记录
* Input          : context - 上报上下文；now - 当前毫秒
* Output         : 重试截止、退避间隔及inflight状态
* Return         : 无
* Attention      : 保留事件与全部已发送序号关联
*******************************************************************************/
static void alarm_retry(alarm_event_reporter_t *context, uint32_t now)
{
    context->inflight = false;
    context->backoff = context->backoff == 0
                       ? context->minimum_ms
                       : (context->backoff >= context->maximum_ms / 2 ? context->maximum_ms : context->backoff * 2);
    context->retry_at = now + context->backoff;
    context->retry_wait = true;
}

/*******************************************************************************
* Function Name  : alarm_reporter_poll
* Description    : 在线且退避到期时发送队首，排队超时也受控重试
* Input          : context - 上报上下文；online - 可上报；now - 当前毫秒
* Output         : 按需分配序号、发送队首或安排退避
* Return         : 无；错误见last_error
* Attention      : 单业务任务调用；退避期间仍接受同事件的成功回执
*******************************************************************************/
void alarm_reporter_poll(alarm_event_reporter_t *context, bool online, uint32_t now)
{
    int result;
    if (!context || !context->store || !context->store->ready || !context->send || context->last_error == ALARM_ERROR_CONFIG)
    {
        return;
    }
    if (context->inflight)
    {
        if ((uint32_t)(now - context->sent_at) < context->confirmation_ms)
        {
            return;
        }
        alarm_retry(context, now);
    }
    if (!online || !context->store->image.count)
    {
        return;
    }
    if (context->retry_wait && (int32_t)(now - context->retry_at) < 0)
    {
        return;
    }
    context->retry_wait = false;
    /* 序号先持久分配，再尝试传输；重启后不会误认旧回执。 */
    result = alarm_store_sequence(context->store, &context->sequence);
    if (result)
    {
        context->last_error = result;
        alarm_retry(context, now);
        return;
    }
    if (context->event_id != context->store->image.events[0].id)
    {
        memset(context->attempts, 0, sizeof(context->attempts));
        context->event_id = context->store->image.events[0].id;
    }
    context->inflight = true;
    context->sent_at = now;
    if (!context->send(context->user, &context->store->image.events[0], context->sequence))
    {
        context->last_error = ALARM_ERROR_NOT_READY;
        alarm_retry(context, now);
    }
    else
    {
        context->attempts[context->sequence / 8U] |= (uint8_t)(1U << (context->sequence % 8U));
    }
}

/*******************************************************************************
* Function Name  : alarm_reporter_on_platform_confirmation
* Description    : 检查当前序号和队首归属，原子删除后返回已完成事件ID
* Input          : context - 上报上下文；sequence/response - 已验证业务回执；now - 毫秒；completed_id - 完成ID
* Output         : 持久删除成功才输出completed_id，其他情况为0
* Return         : ALARM_OK或过期/拒绝/存储错误
* Attention      : 迟到成功可完成同一事件；旧拒绝不打断新尝试
*******************************************************************************/
int alarm_reporter_on_platform_confirmation(alarm_event_reporter_t *context, uint16_t sequence, uint8_t response, uint32_t now,
                    uint32_t *completed_id)
{
    int result;
    if (completed_id)
    {
        *completed_id = 0;
    }
    if (!context || !completed_id || !context->store)
    {
        return ALARM_ERROR_ARGUMENT;
    }
    if (!(context->attempts[sequence / 8U] & (uint8_t)(1U << (sequence % 8U))))
    {
        return ALARM_ERROR_STALE;
    }
    /* 拒绝只影响当前尝试的退避；同事件旧拒绝不冲掉新尝试。 */
    if (response != 0)
    {
        if (context->inflight && sequence == context->sequence)
        {
            alarm_retry(context, now);
        }
        return ALARM_ERROR_REJECTED;
    }
    /* 平台业务成功后仍须先持久删除，才向前台报告已完成。 */
    result = alarm_store_remove(context->store, context->event_id);
    if (result)
    {
        context->last_error = result;
        return result;
    }
    *completed_id = context->event_id;
    memset(context->attempts, 0, sizeof(context->attempts));
    context->inflight = false;
    context->retry_wait = false;
    context->backoff = 0;
    context->last_error = ALARM_OK;
    return ALARM_OK;
}

/*******************************************************************************
* Function Name  : alarm_reporter_send_failed
* Description    : 忽略过期发送结果，当前发送失败进入退避
* Input          : context - 上报上下文；sequence - 发送cookie；now - 当前毫秒
* Output         : 当前发送失败时安排退避
* Return         : 无
* Attention      : 仅匹配当前尝试；不删除记录
*******************************************************************************/
void alarm_reporter_send_failed(alarm_event_reporter_t *context, uint16_t sequence, uint32_t now)
{
    if (context && context->inflight && context->sequence == sequence)
    {
        alarm_retry(context, now);
    }
}

/*******************************************************************************
* Function Name  : alarm_store_pending
* Description    : 查询持久队列中尚未业务确认的记录数
* Input          : store - 已打开的产品存储上下文
* Output         : 无
* Return         : 非负记录数，或ALARM_ERROR_NOT_READY
* Attention      : 未打开或读取失败的存储绝不能解释为空队列
*******************************************************************************/
int alarm_store_pending(const alarm_event_store_t *store)
{
    return store && store->ready ? (int)store->image.count : ALARM_ERROR_NOT_READY;
}
