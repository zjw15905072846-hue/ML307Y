/*------------------------------------------includes--------------------------------------------*/
#include "test_support.h"
#include "../../src/ml307y/base_bridge.c"
#include "../../src/ml307y/file_port.c"
#include "alarm_button/alarm_core.h"
/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
static int raw_probe;
static int open_error;
static int sync_error;
static unsigned sync_failure_call;
static unsigned sync_calls;
static int close_error;
static int write_error;
static int volume_mode; /* 1：旧目标占满空间；2：其他文件占满，截断也不能回收。 */
static bool truncated_metadata_committed;
static unsigned active_slot_opens;
static unsigned no_space_writes;
static unsigned opens;
static unsigned writes;
static uint32_t chunk = 3;
static uint32_t position;
static uint32_t length;
static uint8_t bytes[SNAPSHOT_BYTES];
extern char alarm_mock_last_diagnostic[128];

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : xy_faccess
* Description    : 模拟保留LittleFS原始错误的底包接口
* Input          : path/mode - 请求
* Output         : 无
* Return         : 原始状态
* Attention      : 不使用吞掉错误的cm_fs_exist
*******************************************************************************/
int xy_faccess(const char *path, int mode)
{
    assert(path && mode == 0);
    return raw_probe;
}

/*******************************************************************************
* Function Name  : xy_rand
* Description    : 给桥接随机源提供测试值
* Input          : 无
* Output         : 无
* Return         : 固定值
* Attention      : 只用于测试
*******************************************************************************/
uint32_t xy_rand(void)
{
    return 0x12345678;
}

/*******************************************************************************
* Function Name  : cm_fs_open
* Description    : 模拟文件打开
* Input          : filename/flag - 请求
* Output         : 位置与计数
* Return         : fd或错误
* Attention      : 写模式重置当前测试文件
*******************************************************************************/
int32_t cm_fs_open(const char *filename, int32_t flag)
{
    assert(filename);
    ++opens;
    if (volume_mode && strstr(filename, "alarm.b"))
    {
        ++active_slot_opens;
    }
    if (open_error)
    {
        return -1;
    }
    position = 0;
    sync_calls = 0;
    if (flag == CM_FS_WB)
    {
        length = 0;
        /* TRUNC 先改内存状态；旧数据块仍被尚未提交的目录项引用。 */
        truncated_metadata_committed = false;
    }
    return 3;
}

/*******************************************************************************
* Function Name  : cm_fs_close
* Description    : 注入关闭失败
* Input          : fd - 句柄
* Output         : 无
* Return         : 状态
* Attention      : 仅测试
*******************************************************************************/
int32_t cm_fs_close(int32_t fd)
{
    assert(fd == 3);
    return close_error;
}

/*******************************************************************************
* Function Name  : cm_fs_sync
* Description    : 注入同步失败
* Input          : fd - 句柄
* Output         : 无
* Return         : 状态
* Attention      : 不模拟硬件掉电持久性
*******************************************************************************/
int32_t cm_fs_sync(int32_t fd)
{
    assert(fd == 3);
    ++sync_calls;
    if (sync_error && (!sync_failure_call || sync_calls == sync_failure_call))
    {
        return sync_error;
    }
    if (length == 0)
    {
        truncated_metadata_committed = true;
    }
    return 0;
}

/*******************************************************************************
* Function Name  : cm_fs_getinfo
* Description    : 模拟三层目录与双快照占用的 32 KiB 小容量文件系统
* Input          : info - 容量输出地址
* Output         : 当前空闲和总字节数
* Return         : 0 - 查询成功
* Attention      : 只模拟分配约束，不代替 SDK LittleFS 或实板运行
*******************************************************************************/
int32_t cm_fs_getinfo(cm_fs_system_info_t *info)
{
    assert(info);
    memset(info, 0, sizeof(*info));
    info->total_size = 32768U;
    info->free_size = !volume_mode || (volume_mode == 1 && truncated_metadata_committed) ? 4096U : 0U;
    return 0;
}

/*******************************************************************************
* Function Name  : cm_fs_write
* Description    : 模拟正数短写或零写
* Input          : fd/buffer/size - 请求
* Output         : 缓冲和位置
* Return         : 实际写入量
* Attention      : 正数短写应继续完成
*******************************************************************************/
int32_t cm_fs_write(int32_t fd, const void *buffer, uint32_t size)
{
    uint32_t n = size > chunk ? chunk : size;
    assert(fd == 3 && position + n <= sizeof(bytes));
    ++writes;
    if (write_error)
    {
        return write_error;
    }
    if (volume_mode == 2 || (volume_mode == 1 && !truncated_metadata_committed))
    {
        ++no_space_writes;
        return -28;
    }
    memcpy(bytes + position, buffer, n);
    position += n;
    length = position;
    return (int32_t)n;
}

/*******************************************************************************
* Function Name  : cm_fs_read
* Description    : 模拟正数短读或EOF
* Input          : fd/buffer/size - 请求
* Output         : buffer
* Return         : 实际读取量
* Attention      : 不能把未完成读取当正常镜像
*******************************************************************************/
int32_t cm_fs_read(int32_t fd, void *buffer, uint32_t size)
{
    uint32_t n = size > chunk ? chunk : size;
    assert(fd == 3 && position + n <= length);
    memcpy(buffer, bytes + position, n);
    position += n;
    return (int32_t)n;
}

/*******************************************************************************
* Function Name  : cm_fs_filesize
* Description    : 返回测试文件长度
* Input          : filename - 路径
* Output         : 无
* Return         : 长度
* Attention      : 只读
*******************************************************************************/
int32_t cm_fs_filesize(const char *filename)
{
    assert(filename);
    return (int32_t)length;
}

/*******************************************************************************
* Function Name  : cm_fs_move
* Description    : 模拟保留坏快照副本
* Input          : src/dest - 路径
* Output         : 无
* Return         : 0
* Attention      : 隔离副本逻辑另由快照测试覆盖
*******************************************************************************/
int32_t cm_fs_move(const char *src, const char *dest)
{
    assert(src && dest && strcmp(src, dest));
    return 0;
}

/*******************************************************************************
* Function Name  : test_full_volume_snapshot_reuse
* Description    : 重现满卷覆盖旧快照时的负 28，并验证失败不触碰活动槽
* Input          : file_store - 真实文件适配上下文；storage - 双快照接口
* Output         : 提交结果、槽归属和错误传播断言
* Return         : 无
* Attention      : 模拟目录项提交前保留旧块；不删除文件、不宣称实板通过
*******************************************************************************/
static void test_full_volume_snapshot_reuse(ml307y_file_store_t *file_store,
                                          storage_interface_t *storage)
{
    uint8_t payload[sizeof(alarm_storage_image_t)];
    unsigned before;
    bool committed;
    memset(payload, 0x5a, sizeof(payload));
    chunk = SNAPSHOT_BYTES;
    volume_mode = 1;
    file_store->snapshots.ready = true;
    file_store->snapshots.active = 1;
    file_store->snapshots.generation = 7;
    file_store->snapshots.payload_size = sizeof(payload);
    committed = storage->write(storage->user, payload, sizeof(payload));
    if (!committed)
    {
        fprintf(stderr, "%s\n", alarm_mock_last_diagnostic);
    }
    assert(committed);
    assert(length == 1980U && no_space_writes == 0);
    assert(file_store->snapshots.active == 0 && file_store->snapshots.generation == 8);
    assert(active_slot_opens == 0);

    /* 截断同步失败时不开始写新内容，也不改变活动槽和代数。 */
    file_store->snapshots.active = 1;
    file_store->snapshots.generation = 7;
    sync_error = -5;
    sync_failure_call = 1;
    before = writes;
    assert(!storage->write(storage->user, payload, sizeof(payload)));
    assert(writes == before && !file_store->snapshots.ready);
    assert(file_store->snapshots.active == 1 && file_store->snapshots.generation == 7);
    assert(strstr(alarm_mock_last_diagnostic, "stage=truncate-sync slot=0 result=-5"));
    sync_error = 0;
    sync_failure_call = 0;

    /* 已释放目标旧块后仍可能发生写入故障；最后完整槽继续保留。 */
    file_store->snapshots.ready = true;
    write_error = -5;
    assert(!storage->write(storage->user, payload, sizeof(payload)));
    assert(!file_store->snapshots.ready && file_store->snapshots.active == 1);
    assert(file_store->snapshots.generation == 7 && active_slot_opens == 0);
    write_error = 0;

    /* 真正满卷仍应失败并暴露负 28，不能靠忽略错误宣布提交。 */
    file_store->snapshots.ready = true;
    volume_mode = 2;
    assert(!storage->write(storage->user, payload, sizeof(payload)));
    assert(no_space_writes == 1 && active_slot_opens == 0);
    assert(!file_store->snapshots.ready && file_store->snapshots.active == 1);
    assert(file_store->snapshots.generation == 7);
    assert(strstr(alarm_mock_last_diagnostic, "stage=write slot=0 result=-28"));
    volume_mode = 0;
    chunk = 3;
}

/*******************************************************************************
* Function Name  : main
* Description    : 验证真实CM文件适配错误路径和产品命名空间
* Input          : 无
* Output         : 断言
* Return         : 0成功
* Attention      : 不在真实Flash执行写入
*******************************************************************************/
int main(void)
{
    product_services_t first = {0};
    product_services_t second = {0};
    ml307y_file_store_t *f;
    ml307y_file_store_t *s;
    uint8_t output[16];
    size_t actual;
    unsigned before;
    alarm_event_store_t queue;
    raw_probe = -2;
    assert(project_fs_probe("products/alarm_button/alarm.a") == STORAGE_EMPTY);
    raw_probe = -5;
    assert(project_fs_probe("products/alarm_button/alarm.a") == STORAGE_IO_ERROR);
    raw_probe = -84;
    assert(project_fs_probe("products/alarm_button/alarm.a") == STORAGE_IO_ERROR);
    assert(project_fs_probe(NULL) == STORAGE_IO_ERROR);
    first.product_id = 0x41420101;
    first.storage_namespace = "alarm_button";
    second.product_id = 0x54500101;
    second.storage_namespace = "template_test";
    assert(ml307y_storage_create(&first) && ml307y_storage_create(&second));
    f = first.storage.user;
    s = second.storage.user;
    assert(strcmp(f->paths[0], s->paths[0]) != 0 && opens == 0);
    before = writes;
    assert(alarm_store_open(&queue, first.product_id, &first.storage) == ALARM_ERROR_STORAGE);
    assert(writes == before && opens == 0);
    raw_probe = 0;
    assert(project_fs_probe(f->paths[0]) == STORAGE_OK);
    assert(ml307y_file_write(f, 0, "abcdefgh", 8));
    assert(writes == before + 3 && length == 8);
    assert(ml307y_file_read(f, 0, output, sizeof(output), &actual) == STORAGE_OK);
    assert(actual == 8 && memcmp(output, "abcdefgh", 8) == 0);
    test_full_volume_snapshot_reuse(f, &first.storage);
    chunk = 0;
    assert(!ml307y_file_write(f, 1, "x", 1));
    assert(strstr(alarm_mock_last_diagnostic, "stage=write slot=1 result=0") != NULL);
    chunk = 3;
    sync_error = -1;
    sync_failure_call = 2;
    assert(!ml307y_file_write(f, 1, "x", 1));
    assert(strstr(alarm_mock_last_diagnostic, "stage=sync slot=1 result=-1") != NULL);
    sync_error = 0;
    sync_failure_call = 0;
    close_error = -1;
    assert(!ml307y_file_write(f, 1, "x", 1));
    assert(strstr(alarm_mock_last_diagnostic, "stage=close-write slot=1 result=-1") != NULL);
    assert(ml307y_file_read(f, 1, output, sizeof(output), &actual) == STORAGE_IO_ERROR);
    assert(strstr(alarm_mock_last_diagnostic, "stage=close-read slot=1 result=-1") != NULL);
    close_error = 0;
    open_error = 1;
    assert(!ml307y_file_write(f, 1, "x", 1));
    assert(strstr(alarm_mock_last_diagnostic, "stage=open-write slot=1 result=-1") != NULL);
    assert(ml307y_file_read(f, 1, output, sizeof(output), &actual) == STORAGE_IO_ERROR);
    assert(strstr(alarm_mock_last_diagnostic, "stage=open-read slot=1 result=-1") != NULL);
    puts("CM file: full-volume snapshot reuse, active-slot preservation, missing vs I/O errors, "
         "chunked I/O and write/sync/open/close failures "
         "OK");
    return 0;
}
