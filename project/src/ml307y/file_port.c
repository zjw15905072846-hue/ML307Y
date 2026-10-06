/*------------------------------------------includes--------------------------------------------*/
#include "ml307y/ml307y_port.h"
#include "ml307y/diag_uart.h"
#include "snapshot_store.h"
#include "cm_fs.h"
#include <stdio.h>
#include "cm_mem.h"
#include <string.h>
/*-------------------------------------------define---------------------------------------------*/
#define ML307Y_FILE_PATH_BYTES 128 /* 每个产品快照路径的缓冲容量。 */

/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    snapshot_store_t snapshots;         /* 双槽提交与恢复状态。 */
    char paths[2][ML307Y_FILE_PATH_BYTES]; /* 两个独立快照文件路径。 */
} ml307y_file_store_t;

/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/* 底包扩展保留“不存在”和“读取错误”的区别。 */
extern int project_fs_probe(const char *path);

/*******************************************************************************
* Function Name  : ml307y_file_trace
* Description    : 输出文件提交的具体阶段和 SDK 返回值以定位实板保存失败
* Input          : stage - 操作；slot - 快照槽；result - 返回码；actual/expected - 字节数
* Output         : UART0 诊断，不输出文件内容
* Return         : 无
* Attention      : 仅后台调用；日志不改变文件错误的传播和双槽保护
*******************************************************************************/
static void ml307y_file_trace(const char *stage, unsigned slot, int32_t result,
                             size_t actual, size_t expected)
{
    ml307y_uart_diag_printf("[project][storage] stage=%s slot=%u result=%d bytes=%u/%u",
                           stage, slot, (int)result, (unsigned)actual, (unsigned)expected);
}

/*******************************************************************************
* Function Name  : ml307y_file_trace_space
* Description    : 记录截断同步前后的文件系统空间以核验负 28 的分配原因
* Input          : stage - 容量采样位置；slot - 正在复用的非活动槽
* Output         : UART0 输出查询返回值、空闲及总字节数
* Return         : 无
* Attention      : 容量查询只做诊断，失败不代替读写接口的实际结果
*******************************************************************************/
static void ml307y_file_trace_space(const char *stage, unsigned slot)
{
    cm_fs_system_info_t info = {0};
    int32_t result = cm_fs_getinfo(&info);
    ml307y_uart_diag_printf("[project][storage] stage=%s slot=%u result=%d free=%u total=%u",
                           stage, slot, (int)result, (unsigned)info.free_size,
                           (unsigned)info.total_size);
}

/*******************************************************************************
* Function Name  : ml307y_file_probe
* Description    : 探测快照文件，保持缺失与I/O错误的区别
* Input          : user - 文件上下文；slot - 0或1
* Output         : 无
* Return         : STORAGE状态码
* Attention      : 只通过已匹配的底包扩展探测
*******************************************************************************/
static int ml307y_file_probe(void *user, unsigned slot)
{
    ml307y_file_store_t *store = user;
    int result;
    if (slot > 1)
    {
        return STORAGE_IO_ERROR;
    }
    result = project_fs_probe(store->paths[slot]);
    ml307y_file_trace("probe", slot, result, 0, 0);
    return result;
}

/*******************************************************************************
* Function Name  : ml307y_file_read
* Description    : 读取完整文件并检查长度和关闭结果
* Input          : user - 上下文；slot - 槽；data/capacity - 缓冲；actual - 输出长度
* Output         : data及actual
* Return         : STORAGE状态码
* Attention      : 打开或短读失败不能当作空白
*******************************************************************************/
static int ml307y_file_read(void *user, unsigned slot, void *data, size_t capacity, size_t *actual)
{
    ml307y_file_store_t *store = user;
    int32_t length;
    int32_t fd;
    int32_t count;
    int32_t closed;
    int result = STORAGE_OK;
    size_t offset = 0;
    if (slot > 1)
    {
        return STORAGE_IO_ERROR;
    }
    length = cm_fs_filesize(store->paths[slot]);
    if (length < 0 || (size_t)length > capacity)
    {
        ml307y_file_trace("filesize", slot, length, 0, capacity);
        return STORAGE_IO_ERROR;
    }
    fd = cm_fs_open(store->paths[slot], CM_FS_RB);
    if (fd < 0)
    {
        ml307y_file_trace("open-read", slot, fd, 0, (size_t)length);
        return STORAGE_IO_ERROR;
    }
    /* SDK 可短读；累积读取直到达到文件原始长度。 */
    while (offset < (size_t)length)
    {
        count = cm_fs_read(fd, (uint8_t *)data + offset, (uint32_t)((size_t)length - offset));
        if (count <= 0 || (size_t)count > (size_t)length - offset)
        {
            ml307y_file_trace("read", slot, count, offset, (size_t)length);
            result = STORAGE_IO_ERROR;
            break;
        }
        offset += (size_t)count;
    }
    closed = cm_fs_close(fd);
    if (closed != 0)
    {
        ml307y_file_trace("close-read", slot, closed, offset, (size_t)length);
        result = STORAGE_IO_ERROR;
    }
    *actual = offset;
    if (result == STORAGE_OK)
    {
        ml307y_file_trace("read-complete", slot, 0, offset, (size_t)length);
    }
    return result;
}

/*******************************************************************************
* Function Name  : ml307y_file_write
* Description    : 先同步非活动槽的截断以回收旧块，再完整写入并同步新快照
* Input          : user - 文件上下文；slot - 槽；data/size - 镜像
* Output         : 目标快照文件
* Return         : true写同步关闭均成功
* Attention      : 仅复用调用方已确认的非活动槽；活动快照保留到新槽回读验证成功
*******************************************************************************/
static bool ml307y_file_write(void *user, unsigned slot, const void *data, size_t size)
{
    ml307y_file_store_t *store = user;
    int32_t fd;
    int32_t written;
    int32_t result;
    size_t offset = 0;
    bool ok = true;
    if (slot > 1)
    {
        return false;
    }
    ml307y_file_trace_space("space-before", slot);
    fd = cm_fs_open(store->paths[slot], CM_FS_WB);
    if (fd < 0)
    {
        ml307y_file_trace("open-write", slot, fd, 0, size);
        return false;
    }
    /* LittleFS 的 TRUNC 先修改打开文件的内存状态，目录仍可能引用旧块。
     * 先持久化空文件，允许复用非活动槽的旧数据块；当前有效槽不受影响。 */
    result = cm_fs_sync(fd);
    if (result != 0)
    {
        ml307y_file_trace("truncate-sync", slot, result, 0, size);
        ok = false;
    }
    else
    {
        ml307y_file_trace_space("space-after-truncate", slot);
    }
    while (ok && offset < size)
    {
        written = cm_fs_write(fd, (const uint8_t *)data + offset, (uint32_t)(size - offset));
        if (written <= 0 || (size_t)written > size - offset)
        {
            ml307y_file_trace("write", slot, written, offset, size);
            ok = false;
            break;
        }
        offset += (size_t)written;
    }
    /* 写满后同步介质，关闭失败也视作提交失败。 */
    if (ok)
    {
        result = cm_fs_sync(fd);
        if (result != 0)
        {
            ml307y_file_trace("sync", slot, result, offset, size);
            ok = false;
        }
    }
    result = cm_fs_close(fd);
    if (result != 0)
    {
        ml307y_file_trace("close-write", slot, result, offset, size);
        ok = false;
    }
    if (ok)
    {
        ml307y_file_trace("write-complete", slot, 0, offset, size);
    }
    return ok;
}

/*******************************************************************************
* Function Name  : ml307y_file_preserve
* Description    : 为中断写入的坏快照保留独立副本再复用槽
* Input          : user - 文件上下文；slot - 已判定损坏的本产品槽
* Output         : 创建不覆盖已有文件的隔离副本
* Return         : true已保存；false停止写入
* Attention      : 未知产品或版本不会走此恢复路径
*******************************************************************************/
static bool ml307y_file_preserve(void *user, unsigned slot)
{
    ml307y_file_store_t *store = user;
    char destination[ML307Y_FILE_PATH_BYTES + 24];
    unsigned index;
    int status;
    for (index = 0; index < 256; ++index)
    {
        snprintf(destination, sizeof(destination), "%s.corrupt.%u", store->paths[slot], index);
        status = project_fs_probe(destination);
        /* 只移动到确实不存在的副本名，不覆盖之前保留的坏文件。 */
        if (status == STORAGE_EMPTY)
        {
            return cm_fs_move(store->paths[slot], destination) == 0;
        }
        if (status != STORAGE_OK)
        {
            return false;
        }
    }
    return false;
}

/*******************************************************************************
* Function Name  : ml307y_storage_create
* Description    : 为选中产品绑定独立命名空间和双快照
* Input          : services - 产品服务容器
* Output         : services.storage
* Return         : true绑定成功
* Attention      : 不在启动入口执行文件I/O
*******************************************************************************/
bool ml307y_storage_create(product_services_t *services)
{
    ml307y_file_store_t *store;
    snapshot_file_interface_t files;
    if (!services || strlen(services->storage_namespace) > 48)
    {
        return false;
    }
    store = cm_calloc(1, sizeof(*store));
    if (!store)
    {
        return false;
    }
    snprintf(store->paths[0], sizeof(store->paths[0]), "products/%s/alarm.a",
             services->storage_namespace);
    snprintf(store->paths[1], sizeof(store->paths[1]), "products/%s/alarm.b",
             services->storage_namespace);
    files.user = store;
    files.probe = ml307y_file_probe;
    files.read = ml307y_file_read;
    files.write_sync = ml307y_file_write;
    files.preserve = ml307y_file_preserve;
    services->storage = snapshot_storage(&store->snapshots, services->product_id, &files);
    return true;
}

/*******************************************************************************
* Function Name  : ml307y_storage_warning
* Description    : 报告已恢复但保留坏快照的状态
* Input          : services - 产品服务
* Output         : 无
* Return         : 0正常或STORAGE_CORRUPT
* Attention      : 不会清除告警或更改文件
*******************************************************************************/
int ml307y_storage_warning(const product_services_t *services)
{
    const snapshot_store_t *store = services->storage.user;
    return store ? store->warning : STORAGE_NOT_READY;
}
