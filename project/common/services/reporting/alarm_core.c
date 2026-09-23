/*------------------------------------------includes--------------------------------------------*/
#include "alarm_core.h"
#include <string.h>
/*-------------------------------------------define---------------------------------------------*/
#define AL_MAGIC 0x414C5232U
#define AL_VERSION 2U
/*-------------------------------------------typedef---------------------------------------------*/
typedef char al_image_must_fit_ftl[(sizeof(al_image_t) <= 4080) ? 1 : -1];

/*-------------------------------------------variables-------------------------------------------*/
/* 全部可变状态归调用方上下文，无全局单例。 */
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : al_crc
* Description    : 计算镜像CRC32，跳过校验字段自身
* Input          : image - 完整持久镜像
* Output         : 无
* Return         : CRC32校验值
* Attention      : CRC字段自身按零参与；镜像布局变动必须升级格式
*******************************************************************************/
static uint32_t al_crc(const al_image_t *image)
{
    const uint8_t *bytes = (const uint8_t *)image;
    uint32_t crc = 0xffffffffU;
    size_t i;
    unsigned b;
    for (i = 0; i < sizeof(*image); i++)
    {
        uint8_t v =
            (i >= offsetof(al_image_t, crc) && i < offsetof(al_image_t, crc) + 4) ? 0 : bytes[i];
        crc ^= v;
        for (b = 0; b < 8; b++)
        {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0);
        }
    }
    return ~crc;
}

/*******************************************************************************
* Function Name  : al_commit
* Description    : 写入候选镜像，成功后才替换可见状态
* Input          : store - 含候选镜像的存储上下文
* Output         : 成功后更新image；失败保留旧image
* Return         : AL_OK或AL_ERR_STORAGE
* Attention      : 端口write须提供掉电安全提交；本函数不覆盖失败前可见状态
*******************************************************************************/
static int al_commit(al_store_t *store)
{
    store->staging.crc = al_crc(&store->staging);
    if (!store->port.write(store->port.user, &store->staging, sizeof(store->staging)))
    {
        return AL_ERR_STORAGE;
    }
    store->image = store->staging;
    return AL_OK;
}

/*******************************************************************************
* Function Name  : al_store_open
* Description    : 恢复并检查产品身份、结构版本和记录完整性
* Input          : store - 输出上下文；product_id - 产品身份；port - 存储接口
* Output         : store - 经身份、格式与CRC校验的队列
* Return         : AL_OK或明确的存储/身份/格式错误
* Attention      : 仅AL_STORAGE_EMPTY允许初始化；未知镜像不得擦除
*******************************************************************************/
int al_store_open(al_store_t *store, uint32_t product_id, const al_storage_port_t *port)
{
    int r;
    unsigned i;
    if (!store || !port || !port->read || !port->write || !product_id)
    {
        return AL_ERR_ARGUMENT;
    }
    memset(store, 0, sizeof(*store));
    store->port = *port;
    r = port->read(port->user, &store->image, sizeof(store->image));
    if (r == AL_STORAGE_EMPTY)
    {
        memset(&store->staging, 0, sizeof(store->staging));
        store->staging.magic = AL_MAGIC;
        store->staging.version = AL_VERSION;
        store->staging.product_id = product_id;
        store->staging.bytes = (uint16_t)sizeof(al_image_t);
        store->staging.next_id = 1;
        store->staging.next_sequence = 1;
        r = al_commit(store);
        if (r)
        {
            return r;
        }
    }
    else if (r != AL_OK)
    {
        if (r == STORAGE_FOREIGN)
        {
            return AL_ERR_FOREIGN;
        }
        if (r == STORAGE_CORRUPT)
        {
            return AL_ERR_CORRUPT;
        }
        return AL_ERR_STORAGE;
    }
    else
    {
        if (store->image.magic != AL_MAGIC || store->image.product_id != product_id ||
            store->image.version != AL_VERSION)
        {
            return AL_ERR_FOREIGN;
        }
        if (store->image.bytes != sizeof(al_image_t) || store->image.count > AL_CAPACITY ||
            store->image.crc != al_crc(&store->image) || !store->image.next_id ||
            !store->image.next_sequence || store->image.next_sequence > 65536U)
        {
            return AL_ERR_CORRUPT;
        }
        for (i = 0; i < store->image.count; i++)
        {
            if (!store->image.events[i].id || store->image.events[i].id >= store->image.next_id ||
                (i && store->image.events[i].id <= store->image.events[i - 1].id))
            {
                return AL_ERR_CORRUPT;
            }
        }
    }
    store->ready = true;
    return AL_OK;
}

/*******************************************************************************
* Function Name  : al_store_enqueue
* Description    : 持久化独立事件，失败时不消耗ID也不改变队列
* Input          : store - 队列；event - 事件快照；id - 输出ID指针
* Output         : 持久成功才返回非零id
* Return         : AL_OK或满队列、写入失败、ID耗尽错误
* Attention      : 不覆盖未确认报警；持久化完成后方可发送
*******************************************************************************/
int al_store_enqueue(al_store_t *store, const al_event_t *event, uint32_t *id)
{
    int r;
    if (id)
    {
        *id = 0;
    }
    if (!store || !event || !id)
    {
        return AL_ERR_ARGUMENT;
    }
    if (!store->ready)
    {
        return AL_ERR_NOT_READY;
    }
    if (store->image.count == AL_CAPACITY)
    {
        return AL_ERR_FULL;
    }
    if (store->image.next_id == UINT32_MAX)
    {
        return AL_ERR_EXHAUSTED;
    }
    store->staging = store->image;
    store->staging.events[store->staging.count] = *event;
    store->staging.events[store->staging.count++].id = store->staging.next_id++;
    r = al_commit(store);
    if (!r)
    {
        *id = store->image.next_id - 1;
    }
    return r;
}

/*******************************************************************************
* Function Name  : al_store_remove
* Description    : 仅在平台确认后原子删除指定队首
* Input          : store - 队列；id - 已业务确认的队首事件
* Output         : 原子删除匹配队首，保留其他事件
* Return         : AL_OK或过期/存储错误
* Attention      : 调用方必须先验证平台业务回执
*******************************************************************************/
int al_store_remove(al_store_t *store, uint32_t id)
{
    if (!store || !store->ready)
    {
        return AL_ERR_NOT_READY;
    }
    if (!store->image.count || store->image.events[0].id != id)
    {
        return AL_ERR_STALE;
    }
    store->staging = store->image;
    store->staging.count--;
    memmove(store->staging.events, store->staging.events + 1,
            store->staging.count * sizeof(al_event_t));
    memset(&store->staging.events[store->staging.count], 0, sizeof(al_event_t));
    return al_commit(store);
}

/*******************************************************************************
* Function Name  : al_store_sequence
* Description    : 持久分配序列，跨重启不复用；耗尽时显式失败
* Input          : store - 持久队列；sequence - 输出协议序号
* Output         : 成功后提交递增计数并输出序号
* Return         : AL_OK或存储/序号耗尽错误
* Attention      : 跨重启不复用序号；回卷规则未确认时显式停止分配
*******************************************************************************/
int al_store_sequence(al_store_t *store, uint16_t *sequence)
{
    int r;
    if (!store || !sequence || !store->ready)
    {
        return AL_ERR_NOT_READY;
    }
    if (store->image.next_sequence > 65535U)
    {
        return AL_ERR_EXHAUSTED;
    }
    store->staging = store->image;
    store->staging.next_sequence++;
    r = al_commit(store);
    if (!r)
    {
        *sequence = (uint16_t)(store->image.next_sequence - 1);
    }
    return r;
}

/*******************************************************************************
* Function Name  : al_reporter_init
* Description    : 初始化串行队首发送及有限间隔重试
* Input          : ctx - 上报上下文；store/send/user - 依赖与回调；其余参数为毫秒超时
* Output         : ctx - 独立的发送、重试及序号关联状态
* Return         : 无；非法配置记入last_error
* Attention      : send只表示进入传输队列；不能等同业务成功
*******************************************************************************/
void al_reporter_init(al_reporter_t *ctx, al_store_t *store, al_send_fn send, void *user,
                      uint32_t ack_ms, uint32_t min_retry_ms, uint32_t max_retry_ms)
{
    if (!ctx)
    {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->store = store;
    ctx->send = send;
    ctx->user = user;
    ctx->ack_ms = ack_ms;
    ctx->min_ms = min_retry_ms;
    ctx->max_ms = max_retry_ms;
    if (!ack_ms || !min_retry_ms || max_retry_ms < min_retry_ms || max_retry_ms >= 0x80000000U ||
        ack_ms >= 0x80000000U)
    {
        ctx->last_error = AL_ERR_CONFIG;
    }
}

/*******************************************************************************
* Function Name  : al_retry
* Description    : 失败后安排退避，保持报警记录
* Input          : ctx - 上报上下文；now - 当前毫秒
* Output         : 重试截止、退避间隔及inflight状态
* Return         : 无
* Attention      : 保留事件与全部已发送序号关联
*******************************************************************************/
static void al_retry(al_reporter_t *ctx, uint32_t now)
{
    ctx->inflight = false;
    ctx->backoff = ctx->backoff == 0
                       ? ctx->min_ms
                       : (ctx->backoff >= ctx->max_ms / 2 ? ctx->max_ms : ctx->backoff * 2);
    ctx->retry_at = now + ctx->backoff;
    ctx->retry_wait = true;
}

/*******************************************************************************
* Function Name  : al_reporter_poll
* Description    : 在线且退避到期时发送队首，排队超时也受控重试
* Input          : ctx - 上报上下文；online - 可上报；now - 当前毫秒
* Output         : 按需分配序号、发送队首或安排退避
* Return         : 无；错误见last_error
* Attention      : 单业务任务调用；退避期间仍接受同事件的成功回执
*******************************************************************************/
void al_reporter_poll(al_reporter_t *ctx, bool online, uint32_t now)
{
    int r;
    if (!ctx || !ctx->store || !ctx->store->ready || !ctx->send || ctx->last_error == AL_ERR_CONFIG)
    {
        return;
    }
    if (ctx->inflight)
    {
        if ((uint32_t)(now - ctx->sent_at) < ctx->ack_ms)
        {
            return;
        }
        al_retry(ctx, now);
    }
    if (!online || !ctx->store->image.count)
    {
        return;
    }
    if (ctx->retry_wait && (int32_t)(now - ctx->retry_at) < 0)
    {
        return;
    }
    ctx->retry_wait = false;
    r = al_store_sequence(ctx->store, &ctx->sequence);
    if (r)
    {
        ctx->last_error = r;
        al_retry(ctx, now);
        return;
    }
    if (ctx->event_id != ctx->store->image.events[0].id)
    {
        memset(ctx->attempts, 0, sizeof(ctx->attempts));
        ctx->event_id = ctx->store->image.events[0].id;
    }
    ctx->inflight = true;
    ctx->sent_at = now;
    if (!ctx->send(ctx->user, &ctx->store->image.events[0], ctx->sequence))
    {
        ctx->last_error = AL_ERR_NOT_READY;
        al_retry(ctx, now);
    }
    else
    {
        ctx->attempts[ctx->sequence / 8U] |= (uint8_t)(1U << (ctx->sequence % 8U));
    }
}

/*******************************************************************************
* Function Name  : al_reporter_ack
* Description    : 检查当前序号和队首归属，原子删除后返回已完成事件ID
* Input          : ctx - 上报上下文；sequence/response - 已验证业务回执；now - 毫秒；completed_id - 完成ID
* Output         : 持久删除成功才输出completed_id，其他情况为0
* Return         : AL_OK或过期/拒绝/存储错误
* Attention      : 迟到成功可完成同一事件；旧拒绝不打断新尝试
*******************************************************************************/
int al_reporter_ack(al_reporter_t *ctx, uint16_t sequence, uint8_t response, uint32_t now,
                    uint32_t *completed_id)
{
    int r;
    if (completed_id)
    {
        *completed_id = 0;
    }
    if (!ctx || !completed_id || !ctx->store)
    {
        return AL_ERR_ARGUMENT;
    }
    if (!(ctx->attempts[sequence / 8U] & (uint8_t)(1U << (sequence % 8U))))
    {
        return AL_ERR_STALE;
    }
    if (response != 0)
    {
        if (ctx->inflight && sequence == ctx->sequence)
        {
            al_retry(ctx, now);
        }
        return AL_ERR_REJECTED;
    }
    r = al_store_remove(ctx->store, ctx->event_id);
    if (r)
    {
        ctx->last_error = r;
        return r;
    }
    *completed_id = ctx->event_id;
    memset(ctx->attempts, 0, sizeof(ctx->attempts));
    ctx->inflight = false;
    ctx->retry_wait = false;
    ctx->backoff = 0;
    ctx->last_error = AL_OK;
    return AL_OK;
}

/*******************************************************************************
* Function Name  : al_reporter_send_failed
* Description    : 忽略过期发送结果，当前发送失败进入退避
* Input          : ctx - 上报上下文；sequence - 发送cookie；now - 当前毫秒
* Output         : 当前发送失败时安排退避
* Return         : 无
* Attention      : 仅匹配当前尝试；不删除记录
*******************************************************************************/
void al_reporter_send_failed(al_reporter_t *ctx, uint16_t sequence, uint32_t now)
{
    if (ctx && ctx->inflight && ctx->sequence == sequence)
    {
        al_retry(ctx, now);
    }
}

/*******************************************************************************
* Function Name  : al_store_pending
* Description    : 查询持久队列中尚未业务确认的记录数
* Input          : store - 已打开的产品存储上下文
* Output         : 无
* Return         : 非负记录数，或AL_ERR_NOT_READY
* Attention      : 未打开或读取失败的存储绝不能解释为空队列
*******************************************************************************/
int al_store_pending(const al_store_t *store)
{
    return store && store->ready ? (int)store->image.count : AL_ERR_NOT_READY;
}
