/*------------------------------------------includes--------------------------------------------*/
#include "alarm_button.h"
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
* Return         : AL_OK
* Attention      : 故意不返回保存结果
*******************************************************************************/
static int queue_submit(void *user, uint32_t token, const al_event_t *event)
{
    (void)user;
    assert(event->event_type == 0x0c);
    request = token;
    ++submissions;
    return AL_OK;
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
    ab_app_t app;
    ab_config_t config = ab_default_config();
    ab_io_t io = {0, capture_output, 0, queue_submit};
    uint32_t first;
    unsigned cycle;
    assert(ab_init(&app, &config, &io, NULL, NULL) == AL_OK);
    ab_poll(&app, true, 0, NULL);
    ab_poll(&app, true, 30, NULL);
    first = request;
    assert(submissions == 1 && led && !buzzer);
    assert(app.latest_event_id == 0 && app.pending_saves == 1);
    for (cycle = 0; cycle < 6; ++cycle)
    {
        ab_poll(&app, true, 1030 + cycle * 500, NULL);
        assert(led && buzzer);
        ab_poll(&app, true, 1280 + cycle * 500, NULL);
        assert(!led && !buzzer);
    }
    ab_poll(&app, true, 4030, NULL);
    assert(led && !buzzer);
    ab_poll(&app, false, 4050, NULL);
    ab_poll(&app, false, 4080, NULL);
    ab_poll(&app, true, 4100, NULL);
    ab_poll(&app, true, 4130, NULL);
    assert(request != first && submissions == 2);
    ab_saved(&app, first, 11, AL_OK);
    ab_confirmed(&app, 11);
    ab_poll(&app, true, 8130, NULL);
    assert(led && !buzzer && app.latest_event_id == 0);
    ab_saved(&app, request, 12, AL_OK);
    ab_confirmed(&app, 11);
    ab_poll(&app, true, 9000, NULL);
    assert(led);
    ab_confirmed(&app, 12);
    ab_poll(&app, true, 9001, NULL);
    assert(!led && !buzzer && app.pending_saves == 0);
    ab_saved(&app, request, 12, AL_OK);
    assert(app.pending_saves == 0);
    puts("async UI: delayed persistence, full six cycles and request/ACK isolation passed");
    return 0;
}
