/*------------------------------------------includes--------------------------------------------*/
#include "alarm_button/alarm_button.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
static alarm_storage_image_t disk;
static alarm_event_store_t store;
static alarm_event_reporter_t reporter;
static alarm_button_state_t button_state;
static bool blank = true;
static bool fail = false;
static uint16_t seq;
static bool led;
static bool buzzer;
static uint32_t pending_request;
static alarm_event_t pending_event;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : read_disk
* Description    : 读取测试镜像
* Input          : 见签名；context/store 为独占上下文，now 为单调毫秒
* Output         : 上下文及显式输出参数
* Return         : 见签名；状态码 ALARM_OK 表示成功，负值表示失败
* Attention      : 由一个业务任务串行调用；存储端口必须提供原子提交
*******************************************************************************/
static int read_disk(void *u, void *p, size_t n)
{
    (void)u;
    if (blank)
    {
        return ALARM_STORAGE_EMPTY;
    }
    memcpy(p, &disk, n);
    return 0;
}

/*******************************************************************************
* Function Name  : write_disk
* Description    : 原子提交测试镜像
* Input          : 见签名；context/store 为独占上下文，now 为单调毫秒
* Output         : 上下文及显式输出参数
* Return         : 见签名；状态码 ALARM_OK 表示成功，负值表示失败
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
* Function Name  : set_led
* Description    : 记录 LED 输出
* Input          : user - 保留；on - 目标状态
* Output         : led
* Return         : true
* Attention      : 仅供测试
*******************************************************************************/
static bool set_led(void *user, bool on)
{
    (void)user;
    led = on;
    return true;
}

/*******************************************************************************
* Function Name  : set_buzzer
* Description    : 记录蜂鸣器输出
* Input          : user - 保留；on - 目标状态
* Output         : buzzer
* Return         : true
* Attention      : 仅供测试
*******************************************************************************/
static bool set_buzzer(void *user, bool on)
{
    (void)user;
    buzzer = on;
    return true;
}

/*******************************************************************************
* Function Name  : send_record
* Description    : 捕获队首发送序号
* Input          : 见签名；context/store 为独占上下文，now 为单调毫秒
* Output         : 上下文及显式输出参数
* Return         : 见签名；状态码 ALARM_OK 表示成功，负值表示失败
* Attention      : 由一个业务任务串行调用；存储端口必须提供原子提交
*******************************************************************************/
static bool send_record(void *u, const alarm_event_t *e, uint16_t s)
{
    (void)u;
    (void)e;
    seq = s;
    return true;
}

/*******************************************************************************
* Function Name  : submit_event
* Description    : 模拟前台只投递保存请求
* Input          : u - 保留；request - 请求编号；event - 事件快照
* Output         : 待处理请求
* Return         : ALARM_OK表示入队
* Attention      : 真正写入由complete_save模拟后台执行
*******************************************************************************/
static int submit_event(void *u, uint32_t request, const alarm_event_t *event)
{
    (void)u;
    pending_request = request;
    pending_event = *event;
    return ALARM_OK;
}

/*******************************************************************************
* Function Name  : complete_save
* Description    : 模拟后台提交快照并通知前台
* Input          : 无
* Output         : 持久队列和前台保存状态
* Return         : 成功时的事件ID，失败时为0
* Attention      : 前台提交函数本身不执行存储
*******************************************************************************/
static uint32_t complete_save(void)
{
    uint32_t id = 0;
    int result = alarm_store_enqueue(&store, &pending_event, &id);
    alarm_button_on_save_result(&button_state, pending_request, id, result);
    return id;
}

/*******************************************************************************
* Function Name  : complete_confirmation
* Description    : 模拟后台完成业务回执并通知前台
* Input          : sequence - 平台回执序号；now - 当前毫秒
* Output         : 持久队列和当前提示状态
* Return         : 报警回执处理结果
* Attention      : 仅持久删除成功时通知前台确认
*******************************************************************************/
static int complete_confirmation(uint16_t sequence, uint32_t now)
{
    uint32_t completed_id = 0;
    int result = alarm_reporter_on_platform_confirmation(&reporter, sequence, 0, now, &completed_id);
    if (result == ALARM_OK)
    {
        alarm_button_on_alarm_confirmed(&button_state, completed_id);
    }
    return result;
}

/*******************************************************************************
* Function Name  : main
* Description    : 模拟两次按键、乱序CONFIRMATION、34秒继续重试及本地保存失败
* Input          : 见签名；context/store 为独占上下文，now 为单调毫秒
* Output         : 上下文及显式输出参数
* Return         : 见签名；状态码 ALARM_OK 表示成功，负值表示失败
* Attention      : 由一个业务任务串行调用；存储端口必须提供原子提交
*******************************************************************************/
int main(void)
{
    alarm_storage_port_t p = {0, read_disk, write_disk, NULL};
    alarm_button_callbacks_t callbacks = {0, set_led, set_buzzer, 0, submit_event};
    alarm_button_config_t c = alarm_button_default_config();
    uint32_t first;
    uint32_t second;
    uint16_t old;
    assert(alarm_store_open(&store, ALARM_BUTTON_PRODUCT_ID, &p) == 0);
    alarm_reporter_init(&reporter, &store, send_record, 0, 10000, 5000, 30000);
    assert(alarm_button_init(&button_state, &c, &callbacks) == 0);
    alarm_button_update(&button_state, true, 0);
    assert(!led);
    alarm_button_update(&button_state, true, 30);
    complete_save();
    assert(led && !buzzer && store.image.count == 1);
    first = button_state.indicator.event_id;
    alarm_reporter_poll(&reporter, true, 30);
    old = seq;
    alarm_button_update(&button_state, true, 1030);
    assert(led && buzzer);
    alarm_button_update(&button_state, true, 1280);
    assert(!led && !buzzer);
    alarm_button_update(&button_state, false, 1500);
    alarm_button_update(&button_state, false, 1530);
    alarm_button_update(&button_state, true, 1600);
    alarm_button_update(&button_state, true, 1630);
    complete_save();
    second = button_state.indicator.event_id;
    assert(first != second && store.image.count == 2);
    assert(complete_confirmation(old, 2000) == 0);
    alarm_button_update(&button_state, true, 5630);
    assert(led && !buzzer);
    alarm_reporter_poll(&reporter, true, 5630);
    assert(seq != old);
    assert(complete_confirmation(old, 5631) == ALARM_ERROR_STALE);
    alarm_button_update(&button_state, false, 6000);
    alarm_button_update(&button_state, false, 6030);
    alarm_button_update(&button_state, false, 35630);
    assert(!led && !buzzer && store.image.count == 1 && !alarm_button_can_sleep(&button_state));
    alarm_reporter_poll(&reporter, true, 35630);
    alarm_reporter_poll(&reporter, true, 40630);
    assert(complete_confirmation(seq, 40631) == 0);
    button_state.background_idle = true;
    assert(alarm_button_can_sleep(&button_state));
    fail = true;
    alarm_button_update(&button_state, true, 41000);
    alarm_button_update(&button_state, true, 41030);
    complete_save();
    assert(button_state.indicator.event_id == 0 && button_state.last_error == ALARM_ERROR_STORAGE && store.image.count == 0);
    alarm_button_update(&button_state, false, 75030);
    alarm_button_update(&button_state, false, 75060);
    assert(!alarm_button_can_sleep(&button_state));
    puts("product: repeated presses, confirmation ownership, background retry and faults passed");
    return 0;
}
