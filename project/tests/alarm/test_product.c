/*------------------------------------------includes--------------------------------------------*/
#include "alarm_button/alarm_button.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
static al_image_t disk;
static al_store_t store;
static al_reporter_t reporter;
static ab_app_t app;
static bool blank = true;
static bool fail = false;
static uint16_t seq;
static bool led;
static bool buzzer;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : read_disk
* Description    : 读取测试镜像
* Input          : 见签名；ctx/store 为独占上下文，now 为单调毫秒
* Output         : 上下文及显式输出参数
* Return         : 见签名；状态码 AL_OK 表示成功，负值表示失败
* Attention      : 由一个业务任务串行调用；存储端口必须提供原子提交
*******************************************************************************/
static int read_disk(void *u, void *p, size_t n)
{
    (void)u;
    if (blank)
    {
        return AL_STORAGE_EMPTY;
    }
    memcpy(p, &disk, n);
    return 0;
}

/*******************************************************************************
* Function Name  : write_disk
* Description    : 原子提交测试镜像
* Input          : 见签名；ctx/store 为独占上下文，now 为单调毫秒
* Output         : 上下文及显式输出参数
* Return         : 见签名；状态码 AL_OK 表示成功，负值表示失败
* Attention      : 由一个业务任务串行调用；存储端口必须提供原子提交
*******************************************************************************/
static bool write_disk(void *u, const void *p, size_t n)
{
    (void)u;
    if (fail)
    {
        return false;
    }
    memcpy(&disk, p, n);
    blank = false;
    return true;
}

/*******************************************************************************
* Function Name  : output
* Description    : 记录声光实际命令
* Input          : 见签名；ctx/store 为独占上下文，now 为单调毫秒
* Output         : 上下文及显式输出参数
* Return         : 见签名；状态码 AL_OK 表示成功，负值表示失败
* Attention      : 由一个业务任务串行调用；存储端口必须提供原子提交
*******************************************************************************/
static bool output(void *u, bool l, bool b)
{
    (void)u;
    led = l;
    buzzer = b;
    return true;
}

/*******************************************************************************
* Function Name  : send_record
* Description    : 捕获队首发送序号
* Input          : 见签名；ctx/store 为独占上下文，now 为单调毫秒
* Output         : 上下文及显式输出参数
* Return         : 见签名；状态码 AL_OK 表示成功，负值表示失败
* Attention      : 由一个业务任务串行调用；存储端口必须提供原子提交
*******************************************************************************/
static bool send_record(void *u, const al_event_t *e, uint16_t s)
{
    (void)u;
    (void)e;
    seq = s;
    return true;
}

/*******************************************************************************
* Function Name  : main
* Description    : 模拟两次按键、乱序ACK、34秒继续重试及本地保存失败
* Input          : 见签名；ctx/store 为独占上下文，now 为单调毫秒
* Output         : 上下文及显式输出参数
* Return         : 见签名；状态码 AL_OK 表示成功，负值表示失败
* Attention      : 由一个业务任务串行调用；存储端口必须提供原子提交
*******************************************************************************/
int main(void)
{
    al_storage_port_t p = {0, read_disk, write_disk};
    ab_io_t io = {0, output, 0};
    ab_config_t c = ab_default_config();
    uint32_t first;
    uint32_t second;
    uint16_t old;
    assert(al_store_open(&store, AB_PRODUCT_ID, &p) == 0);
    al_reporter_init(&reporter, &store, send_record, 0, 10000, 5000, 30000);
    assert(ab_init(&app, &c, &io, &store, &reporter) == 0);
    ab_poll(&app, true, 0, 0);
    assert(!led);
    ab_poll(&app, true, 30, 0);
    assert(led && !buzzer && store.image.count == 1);
    first = app.latest_event_id;
    al_reporter_poll(&reporter, true, 30);
    old = seq;
    ab_poll(&app, true, 1030, 0);
    assert(led && buzzer);
    ab_poll(&app, true, 1280, 0);
    assert(!led && !buzzer);
    ab_poll(&app, false, 1500, 0);
    ab_poll(&app, false, 1530, 0);
    ab_poll(&app, true, 1600, 0);
    ab_poll(&app, true, 1630, 0);
    second = app.latest_event_id;
    assert(first != second && store.image.count == 2);
    assert(ab_ack(&app, old, 0, 2000) == 0);
    ab_poll(&app, true, 5630, 0);
    assert(led && !buzzer);
    al_reporter_poll(&reporter, true, 5630);
    assert(seq != old);
    assert(ab_ack(&app, old, 0, 5631) == AL_ERR_STALE);
    ab_poll(&app, false, 6000, 0);
    ab_poll(&app, false, 6030, 0);
    ab_poll(&app, false, 35630, 0);
    assert(!led && !buzzer && store.image.count == 1 && !ab_can_sleep(&app));
    al_reporter_poll(&reporter, true, 35630);
    al_reporter_poll(&reporter, true, 40630);
    assert(ab_ack(&app, seq, 0, 40631) == 0);
    assert(ab_can_sleep(&app));
    fail = true;
    ab_poll(&app, true, 41000, 0);
    ab_poll(&app, true, 41030, 0);
    assert(app.latest_event_id == 0 && app.last_error == AL_ERR_STORAGE && store.image.count == 0);
    ab_poll(&app, false, 75030, 0);
    ab_poll(&app, false, 75060, 0);
    assert(!ab_can_sleep(&app));
    puts("product: repeated presses, ACK ownership, background retry and faults passed");
    return 0;
}
