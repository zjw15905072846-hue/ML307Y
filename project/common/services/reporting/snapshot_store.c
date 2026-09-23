/*------------------------------------------includes--------------------------------------------*/
#include "snapshot_store.h"
#include <string.h>
/*-------------------------------------------define---------------------------------------------*/
#define SNAPSHOT_MAGIC 0x53504E31U
#define SNAPSHOT_VERSION 1U

/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : ss_get32
* Description    : 读取固定小端字段，不依赖结构体填充
* Input          : p - 至少四字节输入
* Output         : 无
* Return         : 解码值
* Attention      : 只访问给定缓冲
*******************************************************************************/
static uint32_t ss_get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/*******************************************************************************
* Function Name  : ss_put32
* Description    : 写入固定小端字段
* Input          : p - 四字节输出；value - 数值
* Output         : p - 编码结果
* Return         : 无
* Attention      : 不访问存储设备
*******************************************************************************/
static void ss_put32(uint8_t *p, uint32_t value)
{
    unsigned i;
    for (i = 0; i < 4; ++i)
    {
        p[i] = (uint8_t)(value >> (8U * i));
    }
}

/*******************************************************************************
* Function Name  : ss_crc
* Description    : 校验快照头与数据，校验字段自身按零计算
* Input          : p - 完整记录；size - 字节数
* Output         : 无
* Return         : CRC32
* Attention      : 不修改输入
*******************************************************************************/
static uint32_t ss_crc(const uint8_t *p, size_t size)
{
    uint32_t crc = 0xffffffffU;
    size_t i;
    unsigned bit;
    for (i = 0; i < size; ++i)
    {
        crc ^= (i >= 24 && i < 28) ? 0 : p[i];
        for (bit = 0; bit < 8; ++bit)
        {
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

/*******************************************************************************
* Function Name  : ss_check
* Description    : 验证产品身份、版本、长度及完整性
* Input          : s - 存储上下文；data/size - 记录；generation - 输出代数
* Output         : generation - 有效记录代数
* Return         : STORAGE状态码
* Attention      : 外来格式不能当空白
*******************************************************************************/
static int ss_check(snapshot_store_t *s, const uint8_t *data, size_t size, uint64_t *generation)
{
    if (size < SNAPSHOT_HEADER_BYTES)
    {
        return STORAGE_CORRUPT;
    }
    if (ss_get32(data) != SNAPSHOT_MAGIC || ss_get32(data + 4) != SNAPSHOT_VERSION ||
        ss_get32(data + 8) != s->product_id)
    {
        return STORAGE_FOREIGN;
    }
    if (ss_get32(data + 12) != s->payload_size || size != SNAPSHOT_HEADER_BYTES + s->payload_size ||
        ss_get32(data + 24) != ss_crc(data, size))
    {
        return STORAGE_CORRUPT;
    }
    *generation = ss_get32(data + 16) | ((uint64_t)ss_get32(data + 20) << 32);
    return *generation ? STORAGE_OK : STORAGE_CORRUPT;
}

/*******************************************************************************
* Function Name  : ss_read
* Description    : 扫描双快照并恢复最新完整提交
* Input          : user - 存储上下文；data/size - 产品镜像输出
* Output         : data - 选中的镜像
* Return         : 空白、正常或明确错误
* Attention      : 任何读取错误或外来格式阻止初始化
*******************************************************************************/
static int ss_read(void *user, void *data, size_t size)
{
    snapshot_store_t *s = user;
    uint64_t generations[2] = {0, 0};
    int status[2];
    size_t actual;
    unsigned slot;
    if (!s || !data || !size || size > SNAPSHOT_MAX_PAYLOAD)
    {
        return STORAGE_NOT_READY;
    }
    s->ready = false;
    s->active = -1;
    s->warning = 0;
    s->corrupt_mask = 0;
    s->payload_size = size;
    s->generation = 0;
    for (slot = 0; slot < 2; ++slot)
    {
        status[slot] = s->files.probe(s->files.user, slot);
        if (status[slot] == STORAGE_EMPTY)
        {
            continue;
        }
        if (status[slot] != STORAGE_OK)
        {
            return STORAGE_IO_ERROR;
        }
        actual = 0;
        if (s->files.read(s->files.user, slot, s->verify, sizeof(s->verify), &actual) != STORAGE_OK)
        {
            return STORAGE_IO_ERROR;
        }
        status[slot] = ss_check(s, s->verify, actual, &generations[slot]);
        if (status[slot] == STORAGE_FOREIGN)
        {
            return STORAGE_FOREIGN;
        }
        if (status[slot] == STORAGE_CORRUPT)
        {
            s->corrupt_mask |= 1U << slot;
            s->warning = STORAGE_CORRUPT;
            continue;
        }
        if (s->active >= 0 && generations[slot] == s->generation &&
            memcmp(s->record, s->verify, actual) != 0)
        {
            return STORAGE_CORRUPT;
        }
        if (s->active < 0 || generations[slot] > s->generation)
        {
            memcpy(s->record, s->verify, actual);
            s->active = (int)slot;
            s->generation = generations[slot];
        }
    }
    if (s->active < 0)
    {
        if (s->corrupt_mask)
        {
            return STORAGE_CORRUPT;
        }
        s->ready = true;
        return STORAGE_EMPTY;
    }
    memcpy(data, s->record + SNAPSHOT_HEADER_BYTES, size);
    s->ready = true;
    return STORAGE_OK;
}

/*******************************************************************************
* Function Name  : ss_write
* Description    : 写入非活动快照并同步回读，完成后才切换代数
* Input          : user - 上下文；data/size - 新产品镜像
* Output         : 更新已确认的活动槽
* Return         : true完整提交；false失败
* Attention      : 失败后锁住写入，保留最后完整快照
*******************************************************************************/
static bool ss_write(void *user, const void *data, size_t size)
{
    snapshot_store_t *s = user;
    unsigned slot;
    uint64_t generation;
    uint64_t checked = 0;
    size_t actual = 0;
    size_t bytes = SNAPSHOT_HEADER_BYTES + size;
    if (!s || !s->ready || !data || size != s->payload_size || s->generation == UINT64_MAX)
    {
        return false;
    }
    slot = s->active == 0 ? 1U : 0U;
    if ((s->corrupt_mask & (1U << slot)) &&
        (!s->files.preserve || !s->files.preserve(s->files.user, slot)))
    {
        s->ready = false;
        return false;
    }
    generation = s->generation + 1;
    memset(s->record, 0, SNAPSHOT_HEADER_BYTES);
    ss_put32(s->record, SNAPSHOT_MAGIC);
    ss_put32(s->record + 4, SNAPSHOT_VERSION);
    ss_put32(s->record + 8, s->product_id);
    ss_put32(s->record + 12, (uint32_t)size);
    ss_put32(s->record + 16, (uint32_t)generation);
    ss_put32(s->record + 20, (uint32_t)(generation >> 32));
    memcpy(s->record + SNAPSHOT_HEADER_BYTES, data, size);
    ss_put32(s->record + 24, ss_crc(s->record, bytes));
    if (!s->files.write_sync(s->files.user, slot, s->record, bytes) ||
        s->files.read(s->files.user, slot, s->verify, sizeof(s->verify), &actual) != STORAGE_OK ||
        ss_check(s, s->verify, actual, &checked) != STORAGE_OK || checked != generation ||
        memcmp(s->record, s->verify, bytes) != 0)
    {
        s->ready = false;
        return false;
    }
    s->generation = generation;
    s->active = (int)slot;
    s->corrupt_mask &= ~(1U << slot);
    return true;
}

/*******************************************************************************
* Function Name  : ss_warning
* Description    : 返回恢复过程中发现的损坏告警
* Input          : user - 快照上下文
* Output         : 无
* Return         : 0或STORAGE_CORRUPT
* Attention      : 告警不清除已有快照
*******************************************************************************/
static int ss_warning(void *user)
{
    return ((snapshot_store_t *)user)->warning;
}

/*******************************************************************************
* Function Name  : snapshot_storage
* Description    : 构造独立产品的双快照存储端口
* Input          : store - 独占上下文；product_id - 身份；files - 文件接口
* Output         : 初始化上下文
* Return         : 产品存储接口
* Attention      : 只绑定接口，真正读取由read触发
*******************************************************************************/
storage_if_t snapshot_storage(snapshot_store_t *store, uint32_t product_id,
                              const snapshot_file_if_t *files)
{
    storage_if_t result = {store, ss_read, ss_write, ss_warning};
    memset(store, 0, sizeof(*store));
    store->files = *files;
    store->product_id = product_id;
    store->active = -1;
    return result;
}
