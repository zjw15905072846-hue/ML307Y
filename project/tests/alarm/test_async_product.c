/*------------------------------------------includes--------------------------------------------*/
#include "alarm_button/alarm_button.h"
#include <assert.h>
#include <stdio.h>
/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
static unsigned submissions;
static uint32_t request;
static bool led;
static bool buzzer;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : capture_output
* Description    : 捕获前台声光输出
* Input          : user - 保留；l/b - 输出状态
* Output         : led/buzzer
* Return         : true
* Attention      : 不执行后台工作
*******************************************************************************/
static bool capture_output(void *user, bool l, bool b)
{
    (void)user;
    led = l;
    buzzer = b;
    return true;
}

/*******************************************************************************
* Function Name  : queue_submit
* Description    : 模拟持久化工人长时间忙碌，只接收请求
* Input          : user - 保留；token - 请求；event - 快照
* Output         : request及提交次数
* Return         : ALARM_OK
* Attention      : 故意不返回保存结果
*******************************************************************************/
static int queue_submit(void *user, uint32_t token, const alarm_event_t *event)
{
    (void)user;
    assert(event->event_type == 0x0c);
    request = token;
    ++submissions;
    return ALARM_OK;
}

/*******************************************************************************
* Function Name  : main
* Description    : 验证慢存储不阻塞声光、保存关联、旧确认隔离及34秒结束
* Input          : 无
* Output         : 测试日志
* Return         : 0通过
* Attention      : 无网络和设备访问
*******************************************************************************/
int main(void)
{
    alarm_button_state_t button_state;
    alarm_button_config_t config = alarm_button_default_config();
    alarm_button_callbacks_t callbacks = {0, capture_output, 0, queue_submit};
    uint32_t first;
    unsigned cycle;
    assert(alarm_button_init(&button_state, &config, &callbacks) == ALARM_OK);
    alarm_button_update(&button_state, true, 0);
    alarm_button_update(&button_state, true, 30);
    first = request;
    assert(submissions == 1 && led && !buzzer);
    assert(button_state.indicator.event_id == 0 && button_state.pending_save_count == 1);
    for (cycle = 0; cycle < 6; ++cycle)
    {
        alarm_button_update(&button_state, true, 1030 + cycle * 500);
        assert(led && buzzer);
        alarm_button_update(&button_state, true, 1280 + cycle * 500);
        assert(!led && !buzzer);
    }
    alarm_button_update(&button_state, true, 4030);
    assert(led && !buzzer);
    alarm_button_update(&button_state, false, 4050);
    alarm_button_update(&button_state, false, 4080);
    alarm_button_update(&button_state, true, 4100);
    alarm_button_update(&button_state, true, 4130);
    assert(request != first && submissions == 2);
    alarm_button_on_save_result(&button_state, first, 11, ALARM_OK);
    alarm_button_on_alarm_confirmed(&button_state, 11);
    alarm_button_update(&button_state, true, 8130);
    assert(led && !buzzer && button_state.indicator.event_id == 0);
    alarm_button_on_save_result(&button_state, request, 12, ALARM_OK);
    alarm_button_on_alarm_confirmed(&button_state, 11);
    alarm_button_update(&button_state, true, 9000);
    assert(led);
    alarm_button_on_alarm_confirmed(&button_state, 12);
    alarm_button_update(&button_state, true, 9001);
    assert(!led && !buzzer && button_state.pending_save_count == 0);
    alarm_button_on_save_result(&button_state, request, 12, ALARM_OK);
    assert(button_state.pending_save_count == 0);
    puts("async UI: delayed persistence, full six cycles and request/confirmation isolation passed");
    return 0;
}
