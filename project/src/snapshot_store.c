/*------------------------------------------includes--------------------------------------------*/
#include "snapshot_store.h"
#include <string.h>
/*-------------------------------------------define---------------------------------------------*/
#define SNAPSHOT_MAGIC 0x53504E31U /* 快照格式标识，用来区分非本格式数据。 */
#define SNAPSHOT_VERSION 1U        /* 当前快照头的格式版本。 */

/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : snapshot_get32
* Description    : 读取固定小端字段，不依赖结构体填充
* Input          : bytes - 至少四字节输入
* Output         : 无
* Return         : 解码值
* Attention      : 只访问给定缓冲
*******************************************************************************/
static uint32_t snapshot_get32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

/*******************************************************************************
* Function Name  : snapshot_put32
* Description    : 写入固定小端字段
* Input          : bytes - 四字节输出；value - 数值
* Output         : bytes - 编码结果
* Return         : 无
* Attention      : 不访问存储设备
*******************************************************************************/
static void snapshot_put32(uint8_t *bytes, uint32_t value)
{
    unsigned index;
    for (index = 0; index < 4; ++index)
    {
        bytes[index] = (uint8_t)(value >> (8U * index));
    }
}

/*******************************************************************************
* Function Name  : snapshot_crc
* Description    : 校验快照头与数据，校验字段自身按零计算
* Input          : bytes - 完整记录；size - 字节数
* Output         : 无
* Return         : CRC32
* Attention      : 不修改输入
*******************************************************************************/
static uint32_t snapshot_crc(const uint8_t *bytes, size_t size)
{
    uint32_t crc = 0xffffffffU;
    size_t index;
    unsigned bit;
    for (index = 0; index < size; ++index)
    {
        crc ^= (index >= 24 && index < 28) ? 0 : bytes[index];
        for (bit = 0; bit < 8; ++bit)
        {
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

/*******************************************************************************
* Function Name  : snapshot_check
* Description    : 验证产品身份、版本、长度及完整性
* Input          : snapshot - 存储上下文；data/size - 记录；generation - 输出代数
* Output         : generation - 有效记录代数
* Return         : STORAGE状态码
* Attention      : 外来格式不能当空白
*******************************************************************************/
static int snapshot_check(snapshot_store_t *snapshot, const uint8_t *data, size_t size, uint64_t *generation)
{
    if (size < SNAPSHOT_HEADER_BYTES)
    {
        return STORAGE_CORRUPT;
    }
    if (snapshot_get32(data) != SNAPSHOT_MAGIC || snapshot_get32(data + 4) != SNAPSHOT_VERSION ||
        snapshot_get32(data + 8) != snapshot->product_id)
    {
        return STORAGE_FOREIGN;
    }
    if (snapshot_get32(data + 12) != snapshot->payload_size || size != SNAPSHOT_HEADER_BYTES + snapshot->payload_size ||
        snapshot_get32(data + 24) != snapshot_crc(data, size))
    {
        return STORAGE_CORRUPT;
    }
    *generation = snapshot_get32(data + 16) | ((uint64_t)snapshot_get32(data + 20) << 32);
    return *generation ? STORAGE_OK : STORAGE_CORRUPT;
}

/*******************************************************************************
* Function Name  : snapshot_load
* Description    : 扫描双快照并恢复最新完整提交
* Input          : user - 存储上下文；data/size - 产品镜像输出
* Output         : data - 选中的镜像
* Return         : 空白、正常或明确错误
* Attention      : 任何读取错误或外来格式阻止初始化
*******************************************************************************/
static int snapshot_load(void *user, void *data, size_t size)
{
    snapshot_store_t *snapshot = user;
    uint64_t generations[2] = {0, 0};
    int status[2];
    size_t actual;
    unsigned slot;
    if (!snapshot || !data || !size || size > SNAPSHOT_MAXIMUM_PAYLOAD)
    {
        return STORAGE_NOT_READY;
    }
    snapshot->ready = false;
    snapshot->active = -1;
    snapshot->warning = 0;
    snapshot->corrupt_mask = 0;
    snapshot->payload_size = size;
    snapshot->generation = 0;
    for (slot = 0; slot < 2; ++slot)
    {
        status[slot] = snapshot->files.probe(snapshot->files.user, slot);
        if (status[slot] == STORAGE_EMPTY)
        {
            continue;
        }
        if (status[slot] != STORAGE_OK)
        {
            return STORAGE_IO_ERROR;
        }
        actual = 0;
        if (snapshot->files.read(snapshot->files.user, slot, snapshot->verify, sizeof(snapshot->verify), &actual) != STORAGE_OK)
        {
            return STORAGE_IO_ERROR;
        }
        status[slot] = snapshot_check(snapshot, snapshot->verify, actual, &generations[slot]);
        if (status[slot] == STORAGE_FOREIGN)
        {
            return STORAGE_FOREIGN;
        }
        if (status[slot] == STORAGE_CORRUPT)
        {
            snapshot->corrupt_mask |= 1U << slot;
            snapshot->warning = STORAGE_CORRUPT;
            continue;
        }
        /* 相同代数却内容不同意味着无法确定哪份是真实提交。 */
        if (snapshot->active >= 0 && generations[slot] == snapshot->generation &&
            memcmp(snapshot->record, snapshot->verify, actual) != 0)
        {
            return STORAGE_CORRUPT;
        }
        if (snapshot->active < 0 || generations[slot] > snapshot->generation)
        {
            memcpy(snapshot->record, snapshot->verify, actual);
            snapshot->active = (int)slot;
            snapshot->generation = generations[slot];
        }
    }
    if (snapshot->active < 0)
    {
        /* 两槽均空才允许初始化；只剩损坏槽时不能当作新设备覆盖。 */
        if (snapshot->corrupt_mask)
        {
            return STORAGE_CORRUPT;
        }
        snapshot->ready = true;
        return STORAGE_EMPTY;
    }
    memcpy(data, snapshot->record + SNAPSHOT_HEADER_BYTES, size);
    snapshot->ready = true;
    return STORAGE_OK;
}

/*******************************************************************************
* Function Name  : snapshot_read
* Description    : 恢复产品镜像并记录启动时已核验的写入基线
* Input          : user - 快照上下文；data/size - 产品镜像输出
* Output         : data及已确认镜像
* Return         : 明确的存储状态码
* Attention      : 内部写重试只扫描介质，不替换故障前基线
*******************************************************************************/
static int snapshot_read(void *user, void *data, size_t size)
{
    snapshot_store_t *snapshot = user;
    int result = snapshot_load(user, data, size);
    if (result == STORAGE_OK)
    {
        memcpy(snapshot->confirmed_payload, data, size);
        snapshot->has_confirmed_payload = true;
    }
    else if (result == STORAGE_EMPTY)
    {
        snapshot->has_confirmed_payload = false;
    }
    return result;
}

/*******************************************************************************
* Function Name  : snapshot_write
* Description    : 写入非活动快照并同步回读，完成后才切换代数
* Input          : user - 上下文；data/size - 新产品镜像
* Output         : 更新已确认的活动槽
* Return         : true完整提交；false失败
* Attention      : 故障后先扫描；只允许旧镜像或同一候选重试，其他镜像保持隔离
*******************************************************************************/
static bool snapshot_write(void *user, const void *data, size_t size)
{
    snapshot_store_t *snapshot = user;
    unsigned slot;
    uint64_t generation;
    uint64_t checked = 0;
    size_t actual = 0;
    size_t bytes = SNAPSHOT_HEADER_BYTES + size;
    uint8_t recovered[SNAPSHOT_MAXIMUM_PAYLOAD];
    int result;
    if (!snapshot || !data || !size || size > SNAPSHOT_MAXIMUM_PAYLOAD || size != snapshot->payload_size)
    {
        return false;
    }
    if (!snapshot->ready)
    {
        result = snapshot_load(snapshot, recovered, size);
        if (result == STORAGE_OK)
        {
            /* 候选可能已经完整写入；另一业务的镜像不能覆盖未决提交。 */
            if (memcmp(recovered, data, size) != 0 &&
                (!snapshot->has_confirmed_payload || memcmp(recovered, snapshot->confirmed_payload, size) != 0))
            {
                snapshot->ready = false;
                return false;
            }
        }
        else if (result != STORAGE_EMPTY || snapshot->has_confirmed_payload)
        {
            snapshot->ready = false;
            return false;
        }
        /* 即使读到了相同候选，也重新同步写入并核验后才返回提交成功。 */
    }
    if (snapshot->generation == UINT64_MAX)
    {
        return false;
    }
    slot = snapshot->active == 0 ? 1U : 0U;
    /* 覆盖坏槽前先保留原件，便于排查损坏原因。 */
    if ((snapshot->corrupt_mask & (1U << slot)) &&
        (!snapshot->files.preserve || !snapshot->files.preserve(snapshot->files.user, slot)))
    {
        snapshot->ready = false;
        return false;
    }
    generation = snapshot->generation + 1;
    memset(snapshot->record, 0, SNAPSHOT_HEADER_BYTES);
    snapshot_put32(snapshot->record, SNAPSHOT_MAGIC);
    snapshot_put32(snapshot->record + 4, SNAPSHOT_VERSION);
    snapshot_put32(snapshot->record + 8, snapshot->product_id);
    snapshot_put32(snapshot->record + 12, (uint32_t)size);
    snapshot_put32(snapshot->record + 16, (uint32_t)generation);
    snapshot_put32(snapshot->record + 20, (uint32_t)(generation >> 32));
    memcpy(snapshot->record + SNAPSHOT_HEADER_BYTES, data, size);
    snapshot_put32(snapshot->record + 24, snapshot_crc(snapshot->record, bytes));
    /* 同步写入并回读逐字节确认后，才宣布新代数有效。 */
    if (!snapshot->files.write_sync(snapshot->files.user, slot, snapshot->record, bytes) ||
        snapshot->files.read(snapshot->files.user, slot, snapshot->verify, sizeof(snapshot->verify), &actual) != STORAGE_OK ||
        snapshot_check(snapshot, snapshot->verify, actual, &checked) != STORAGE_OK || checked != generation ||
        memcmp(snapshot->record, snapshot->verify, bytes) != 0)
    {
        snapshot->ready = false;
        return false;
    }
    snapshot->generation = generation;
    snapshot->active = (int)slot;
    snapshot->corrupt_mask &= ~(1U << slot);
    memcpy(snapshot->confirmed_payload, data, size);
    snapshot->has_confirmed_payload = true;
    return true;
}

/*******************************************************************************
* Function Name  : snapshot_warning
* Description    : 返回恢复过程中发现的损坏告警
* Input          : user - 快照上下文
* Output         : 无
* Return         : 0或STORAGE_CORRUPT
* Attention      : 告警不清除已有快照
*******************************************************************************/
static int snapshot_warning(void *user)
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
storage_interface_t snapshot_storage(snapshot_store_t *store, uint32_t product_id,
                              const snapshot_file_interface_t *files)
{
    storage_interface_t result = {store, snapshot_read, snapshot_write, snapshot_warning};
    memset(store, 0, sizeof(*store));
    store->files = *files;
    store->product_id = product_id;
    store->active = -1;
    return result;
}
