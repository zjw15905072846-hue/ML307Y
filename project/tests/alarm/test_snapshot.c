/*------------------------------------------includes--------------------------------------------*/
#include "snapshot_store.h"
#include "alarm_button/alarm_core.h"
#include "test_support.h"
/*-------------------------------------------define---------------------------------------------*/
#define PRODUCT 0x41420101U

/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    uint8_t data[2][SNAPSHOT_BYTES];
    size_t length[2];
    bool exists[2];
    int probe_error;
    int read_error;
    int write_mode;
    unsigned writes;
    unsigned preserved;
} disk_t;

/*-------------------------------------------variables-------------------------------------------*/
static disk_t disk;
static snapshot_store_t snapshots;
static alarm_event_store_t queue;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : probe
* Description    : 模拟保留错误码的探测
* Input          : user/slot - 介质与槽
* Output         : 无
* Return         : 存储状态
* Attention      : 故障不能变成空白
*******************************************************************************/
static int probe(void *user, unsigned slot)
{
    disk_t *d = user;
    return d->probe_error ? STORAGE_IO_ERROR : d->exists[slot] ? STORAGE_OK : STORAGE_EMPTY;
}

/*******************************************************************************
* Function Name  : read_file
* Description    : 模拟快照读取和失败
* Input          : user/slot/data/capacity/actual - 文件读取参数
* Output         : data/actual
* Return         : 状态码
* Attention      : 不进行写入
*******************************************************************************/
static int read_file(void *user, unsigned slot, void *data, size_t capacity, size_t *actual)
{
    disk_t *d = user;
    if (d->read_error || !d->exists[slot] || capacity < d->length[slot])
    {
        return STORAGE_IO_ERROR;
    }
    *actual = d->length[slot];
    memcpy(data, d->data[slot], *actual);
    return STORAGE_OK;
}

/*******************************************************************************
* Function Name  : write_file
* Description    : 注入短写、同步和回读故障
* Input          : user/slot/data/size - 写入请求
* Output         : 模拟介质
* Return         : 是否完整完成
* Attention      : 保留活动槽
*******************************************************************************/
static bool write_file(void *user, unsigned slot, const void *data, size_t size)
{
    disk_t *d = user;
    ++d->writes;
    if (d->write_mode == 1)
    {
        return false;
    }
    d->exists[slot] = true;
    d->length[slot] = d->write_mode == 2 ? size / 2 : size;
    memcpy(d->data[slot], data, d->length[slot]);
    if (d->write_mode == 4)
    {
        d->data[slot][size - 1] ^= 1;
    }
    if (d->write_mode == 5)
    {
        d->read_error = 1;
    }
    return d->write_mode != 2 && d->write_mode != 3;
}

/*******************************************************************************
* Function Name  : preserve
* Description    : 记录坏槽隔离动作
* Input          : user/slot - 介质与槽
* Output         : 隔离计数
* Return         : true
* Attention      : 模拟不覆盖保留副本
*******************************************************************************/
static bool preserve(void *user, unsigned slot)
{
    disk_t *d = user;
    (void)slot;
    ++d->preserved;
    return true;
}

/*******************************************************************************
* Function Name  : open_queue
* Description    : 重新绑定双快照并模拟重启恢复
* Input          : 无
* Output         : 全局队列
* Return         : 打开状态
* Attention      : 无直接初始化空白判断
*******************************************************************************/
static int open_queue(void)
{
    snapshot_file_interface_t files = {&disk, probe, read_file, write_file, preserve};
    storage_interface_t port = snapshot_storage(&snapshots, PRODUCT, &files);
    return alarm_store_open(&queue, PRODUCT, &port);
}

/*******************************************************************************
* Function Name  : fresh
* Description    : 准备空白介质
* Input          : 无
* Output         : 测试夹具
* Return         : 无
* Attention      : 只作用于内存
*******************************************************************************/
static void fresh(void)
{
    memset(&disk, 0, sizeof(disk));
    assert(open_queue() == ALARM_OK);
}

/*******************************************************************************
* Function Name  : test_retry_without_restart
* Description    : 验证运行中写失败恢复、未知提交隔离及原请求重试不重复入队
* Input          : 无
* Output         : 队列、快照与写次数断言
* Return         : 无；断言失败终止测试
* Attention      : 完整写入后同步或回读失败时，禁止另一候选镜像覆盖它
*******************************************************************************/
static void test_retry_without_restart(void)
{
    alarm_event_t event = {0};
    uint32_t id;
    uint16_t sequence;
    unsigned mode;
    unsigned writes;
    event.event_type = 0x0c;
    for (mode = 1; mode <= 5; ++mode)
    {
        fresh();
        event.uptime_ms = 100U;
        assert(alarm_store_enqueue(&queue, &event, &id) == ALARM_OK);
        event.uptime_ms = 200U;
        disk.write_mode = (int)mode;
        assert(alarm_store_enqueue(&queue, &event, &id) == ALARM_ERROR_STORAGE && id == 0);
        disk.write_mode = 0;
        disk.read_error = 0;
        if (mode == 3 || mode == 5)
        {
            writes = disk.writes;
            assert(alarm_store_sequence(&queue, &sequence) == ALARM_ERROR_STORAGE);
            assert(disk.writes == writes && queue.image.count == 1);
        }
        assert(alarm_store_enqueue(&queue, &event, &id) == ALARM_OK && id == 2);
        assert(queue.image.count == 2 && queue.image.events[1].uptime_ms == 200U);
        assert(open_queue() == ALARM_OK && queue.image.count == 2);
    }
    fresh();
    disk.write_mode = 1;
    assert(alarm_store_enqueue(&queue, &event, &id) == ALARM_ERROR_STORAGE);
    disk.write_mode = 0;
    disk.read_error = 1;
    writes = disk.writes;
    assert(alarm_store_enqueue(&queue, &event, &id) == ALARM_ERROR_STORAGE);
    assert(disk.writes == writes && queue.image.count == 0);
    disk.read_error = 0;
    assert(alarm_store_enqueue(&queue, &event, &id) == ALARM_OK && id == 1);
}

/*******************************************************************************
* Function Name  : main
* Description    : 验证快照恢复、拒绝未知数据及故障不丢旧记录
* Input          : 无
* Output         : 断言结果
* Return         : 0成功
* Attention      : 主机故障模型不替代真实断电测试
*******************************************************************************/
int main(void)
{
    alarm_event_t event = {0};
    uint32_t id;
    unsigned writes;
    unsigned i;
    int active;
    uint8_t before[SNAPSHOT_BYTES];
    size_t bytes;
    fresh();
    assert(disk.writes == 1);
    event.event_type = 0x0c;
    assert(alarm_store_enqueue(&queue, &event, &id) == ALARM_OK && id == 1);
    assert(open_queue() == ALARM_OK && alarm_store_pending(&queue) == 1);
    assert(alarm_store_enqueue(&queue, &event, &id) == ALARM_OK && id == 2);
    assert(open_queue() == ALARM_OK && alarm_store_pending(&queue) == 2);

    for (i = 1; i <= 4; ++i)
    {
        fresh();
        assert(alarm_store_enqueue(&queue, &event, &id) == ALARM_OK);
        active = snapshots.active;
        bytes = disk.length[active];
        memcpy(before, disk.data[active], bytes);
        disk.write_mode = (int)i;
        id = 0;
        assert(alarm_store_enqueue(&queue, &event, &id) == ALARM_ERROR_STORAGE && id == 0);
        assert(queue.image.count == 1);
        assert(memcmp(before, disk.data[active], bytes) == 0);
        disk.write_mode = 0;
        assert(open_queue() == ALARM_OK);
        assert(queue.image.count >= 1);
        if (i == 2 || i == 4)
        {
            assert(snapshots.warning == STORAGE_CORRUPT);
            assert(alarm_store_enqueue(&queue, &event, &id) == ALARM_OK);
            assert(disk.preserved == 1);
        }
    }
    fresh();
    assert(alarm_store_enqueue(&queue, &event, &id) == ALARM_OK);
    writes = disk.writes;
    disk.probe_error = 1;
    assert(open_queue() == ALARM_ERROR_STORAGE && disk.writes == writes);
    disk.probe_error = 0;
    disk.read_error = 1;
    assert(open_queue() == ALARM_ERROR_STORAGE && disk.writes == writes);
    disk.read_error = 0;

    for (i = 0; i < 3; ++i)
    {
        fresh();
        assert(alarm_store_enqueue(&queue, &event, &id) == ALARM_OK);
        disk.data[0][i * 4] ^= 1;
        writes = disk.writes;
        assert(open_queue() == ALARM_ERROR_FOREIGN);
        assert(disk.writes == writes);
    }
    memset(&disk, 0, sizeof(disk));
    disk.exists[0] = true;
    disk.length[0] = 12;
    assert(open_queue() == ALARM_ERROR_CORRUPT && disk.writes == 0);

    fresh();
    for (i = 0; i < ALARM_CAPACITY; ++i)
    {
        assert(alarm_store_enqueue(&queue, &event, &id) == ALARM_OK);
    }
    writes = disk.writes;
    assert(alarm_store_enqueue(&queue, &event, &id) == ALARM_ERROR_FULL);
    assert(disk.writes == writes && queue.image.count == ALARM_CAPACITY);
    disk.write_mode = 1;
    assert(alarm_store_remove(&queue, queue.image.events[0].id) == ALARM_ERROR_STORAGE);
    assert(queue.image.count == ALARM_CAPACITY);
    disk.write_mode = 0;
    assert(open_queue() == ALARM_OK && queue.image.count == ALARM_CAPACITY);
    test_retry_without_restart();
    puts("snapshot: runtime retry, uncertain commit isolation, blank, recovery, foreign, read/short-write/sync/verify faults, full, delete "
         "failure OK");
    return 0;
}
