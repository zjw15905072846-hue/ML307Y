/*------------------------------------------includes--------------------------------------------*/
#include "ml307y_port.h"
#include "snapshot_store.h"
#include "cm_fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*-------------------------------------------define---------------------------------------------*/
#define ML_FILE_PATH_BYTES 128

/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    snapshot_store_t snapshots;
    char paths[2][ML_FILE_PATH_BYTES];
} ml_file_store_t;

/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
extern int project_fs_probe(const char *path);

/*******************************************************************************
* Function Name  : ml_file_probe
* Description    : 探测快照文件，保持缺失与I/O错误的区别
* Input          : user - 文件上下文；slot - 0或1
* Output         : 无
* Return         : STORAGE状态码
* Attention      : 只通过已匹配的底包扩展探测
*******************************************************************************/
static int ml_file_probe(void *user, unsigned slot)
{
    ml_file_store_t *store = user;
    if (slot > 1)
    {
        return STORAGE_IO_ERROR;
    }
    return project_fs_probe(store->paths[slot]);
}

/*******************************************************************************
* Function Name  : ml_file_read
* Description    : 读取完整文件并检查长度和关闭结果
* Input          : user - 上下文；slot - 槽；data/capacity - 缓冲；actual - 输出长度
* Output         : data及actual
* Return         : STORAGE状态码
* Attention      : 打开或短读失败不能当作空白
*******************************************************************************/
static int ml_file_read(void *user, unsigned slot, void *data, size_t capacity, size_t *actual)
{
    ml_file_store_t *store = user;
    int32_t length;
    int32_t fd;
    int32_t count;
    int result = STORAGE_OK;
    size_t offset = 0;
    if (slot > 1)
    {
        return STORAGE_IO_ERROR;
    }
    length = cm_fs_filesize(store->paths[slot]);
    if (length < 0 || (size_t)length > capacity)
    {
        return STORAGE_IO_ERROR;
    }
    fd = cm_fs_open(store->paths[slot], CM_FS_RB);
    if (fd < 0)
    {
        return STORAGE_IO_ERROR;
    }
    while (offset < (size_t)length)
    {
        count = cm_fs_read(fd, (uint8_t *)data + offset, (uint32_t)((size_t)length - offset));
        if (count <= 0 || (size_t)count > (size_t)length - offset)
        {
            result = STORAGE_IO_ERROR;
            break;
        }
        offset += (size_t)count;
    }
    if (cm_fs_close(fd) != 0)
    {
        result = STORAGE_IO_ERROR;
    }
    *actual = offset;
    return result;
}

/*******************************************************************************
* Function Name  : ml_file_write
* Description    : 完整写入非活动槽并同步到介质
* Input          : user - 文件上下文；slot - 槽；data/size - 镜像
* Output         : 目标快照文件
* Return         : true写同步关闭均成功
* Attention      : 调用方已验证槽归属，不用于初始化未知文件
*******************************************************************************/
static bool ml_file_write(void *user, unsigned slot, const void *data, size_t size)
{
    ml_file_store_t *store = user;
    int32_t fd;
    int32_t written;
    size_t offset = 0;
    bool ok = true;
    if (slot > 1)
    {
        return false;
    }
    fd = cm_fs_open(store->paths[slot], CM_FS_WB);
    if (fd < 0)
    {
        return false;
    }
    while (offset < size)
    {
        written = cm_fs_write(fd, (const uint8_t *)data + offset, (uint32_t)(size - offset));
        if (written <= 0 || (size_t)written > size - offset)
        {
            ok = false;
            break;
        }
        offset += (size_t)written;
    }
    if (ok && cm_fs_sync(fd) != 0)
    {
        ok = false;
    }
    if (cm_fs_close(fd) != 0)
    {
        ok = false;
    }
    return ok;
}

/*******************************************************************************
* Function Name  : ml_file_preserve
* Description    : 为中断写入的坏快照保留独立副本再复用槽
* Input          : user - 文件上下文；slot - 已判定损坏的本产品槽
* Output         : 创建不覆盖已有文件的隔离副本
* Return         : true已保存；false停止写入
* Attention      : 未知产品或版本不会走此恢复路径
*******************************************************************************/
static bool ml_file_preserve(void *user, unsigned slot)
{
    ml_file_store_t *store = user;
    char destination[ML_FILE_PATH_BYTES + 24];
    unsigned index;
    int status;
    for (index = 0; index < 256; ++index)
    {
        snprintf(destination, sizeof(destination), "%s.corrupt.%u", store->paths[slot], index);
        status = project_fs_probe(destination);
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
* Function Name  : ml_storage_create
* Description    : 为选中产品绑定独立命名空间和双快照
* Input          : services - 产品服务容器
* Output         : services.storage
* Return         : true绑定成功
* Attention      : 不在启动入口执行文件I/O
*******************************************************************************/
bool ml_storage_create(product_services_t *services)
{
    ml_file_store_t *store;
    snapshot_file_if_t files;
    if (!services || strlen(services->storage_namespace) > 48)
    {
        return false;
    }
    store = calloc(1, sizeof(*store));
    if (!store)
    {
        return false;
    }
    snprintf(store->paths[0], sizeof(store->paths[0]), "products/%s/alarm.a",
             services->storage_namespace);
    snprintf(store->paths[1], sizeof(store->paths[1]), "products/%s/alarm.b",
             services->storage_namespace);
    files.user = store;
    files.probe = ml_file_probe;
    files.read = ml_file_read;
    files.write_sync = ml_file_write;
    files.preserve = ml_file_preserve;
    services->storage = snapshot_storage(&store->snapshots, services->product_id, &files);
    return true;
}

/*******************************************************************************
* Function Name  : ml_storage_warning
* Description    : 报告已恢复但保留坏快照的状态
* Input          : services - 产品服务
* Output         : 无
* Return         : 0正常或STORAGE_CORRUPT
* Attention      : 不会清除告警或更改文件
*******************************************************************************/
int ml_storage_warning(const product_services_t *services)
{
    const snapshot_store_t *store = services->storage.user;
    return store ? store->warning : STORAGE_NOT_READY;
}
