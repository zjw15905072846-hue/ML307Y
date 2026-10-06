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
static bool led_failed;
static unsigned buzzer_calls;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : capture_led
* Description    : 捕获前台 LED 输出
* Input          : user - 保留；on - 输出状态
* Output         : led
* Return         : true
* Attention      : 不执行后台工作
*******************************************************************************/
static bool capture_led(void *user, bool on)
{
    (void)user;
    if (!led_failed)
    {
        led = on;
    }
    return !led_failed;
}

/*******************************************************************************
* Function Name  : capture_buzzer
* Description    : 捕获前台蜂鸣器输出
* Input          : user - 保留；on - 输出状态
* Output         : buzzer
* Return         : true
* Attention      : 不执行后台工作
*******************************************************************************/
static bool capture_buzzer(void *user, bool on)
{
    (void)user;
    ++buzzer_calls;
    buzzer = on;
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
* Function Name  : test_led_failure_still_closes_buzzer
* Description    : 验证闪鸣灭相、四秒与三十四秒边界的LED故障不会跳过蜂鸣器关闭
* Input          : 无
* Output         : 器件调用计数与模拟输出
* Return         : 无；断言失败终止测试
* Attention      : 仅注入LED失败，不访问物理器件
*******************************************************************************/
static void test_led_failure_still_closes_buzzer(void)
{
    alarm_button_state_t button_state;
    alarm_button_config_t config = alarm_button_default_config();
    alarm_button_callbacks_t callbacks = {0, capture_led, capture_buzzer, 0, queue_submit};
    unsigned calls_before;
    unsigned index;
    uint32_t boundaries[] = {1280U, 4030U, 34030U};
    for (index = 0; index < sizeof(boundaries) / sizeof(boundaries[0]); ++index)
    {
        led_failed = false;
        assert(alarm_button_init(&button_state, &config, &callbacks) == ALARM_OK);
        alarm_button_update(&button_state, true, 0);
        alarm_button_update(&button_state, true, 30);
        alarm_button_update(&button_state, true, 1030);
        assert(buzzer);
        calls_before = buzzer_calls;
        led_failed = true;
        alarm_button_update(&button_state, true, boundaries[index]);
        assert(buzzer_calls == calls_before + 1U);
        assert(!buzzer);
        assert(button_state.last_error == ALARM_ERROR_NOT_READY);
    }
    led_failed = false;
}

/*******************************************************************************
* Function Name  : main
* Description    : 验证异步保存、声光时序、回执隔离及器件失败关闭路径
* Input          : 无
* Output         : 测试日志
* Return         : 0通过
* Attention      : 无网络和设备访问
*******************************************************************************/
int main(void)
{
    alarm_button_state_t button_state;
    alarm_button_config_t config = alarm_button_default_config();
    alarm_button_callbacks_t callbacks = {0, capture_led, capture_buzzer, 0, queue_submit};
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
    test_led_failure_still_closes_buzzer();
    puts("async UI: delayed persistence, LED failure buzzer shutdown and confirmation isolation passed");
    return 0;
}
