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
static int close_error;
static unsigned opens;
static unsigned writes;
static uint32_t chunk = 3;
static uint32_t position;
static uint32_t length;
static uint8_t bytes[SNAPSHOT_BYTES];

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
    if (open_error)
    {
        return -1;
    }
    position = 0;
    if (flag == CM_FS_WB)
    {
        length = 0;
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
    return sync_error;
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
    chunk = 0;
    assert(!ml307y_file_write(f, 1, "x", 1));
    chunk = 3;
    sync_error = -1;
    assert(!ml307y_file_write(f, 1, "x", 1));
    sync_error = 0;
    close_error = -1;
    assert(!ml307y_file_write(f, 1, "x", 1));
    assert(ml307y_file_read(f, 1, output, sizeof(output), &actual) == STORAGE_IO_ERROR);
    close_error = 0;
    open_error = 1;
    assert(!ml307y_file_write(f, 1, "x", 1));
    assert(ml307y_file_read(f, 1, output, sizeof(output), &actual) == STORAGE_IO_ERROR);
    puts("CM file: missing vs I/O errors, namespaces, chunked I/O, zero write, sync/open/close failures "
         "OK");
    return 0;
}
