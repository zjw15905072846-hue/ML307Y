/*------------------------------------------includes--------------------------------------------*/
#include "alarm_button.h"
#include "product_config.h"
#include <string.h>
/*-------------------------------------------define---------------------------------------------*/
#define AB_EMERGENCY_EVENT 0x0CU

/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : ab_default_config
* Description    : 返回一键报警产品默认时序及22小时业务心跳
* Input          : 无
* Output         : 无
* Return         : 本产品独立默认配置
* Attention      : 公共层不保存全局产品参数
*******************************************************************************/
ab_config_t ab_default_config(void)
{
    ab_config_t c = {AB_DEBOUNCE_MS,
                     AB_HEARTBEAT_MS,
                     {AB_INITIAL_MS, AB_BLINK_MS, AB_WAIT_MS, AB_HALF_PERIOD_MS}};
    return c;
}

/*******************************************************************************
* Function Name  : ab_init
* Description    : 绑定明确端口和独占产品上下文
* Input          : app - 产品上下文；config - 时序；io - 声光端口；store/reporter - 独占业务上下文
* Output         : 初始化产品状态
* Return         : AL_OK或参数/时序配置错误
* Attention      : 初始化本身不发送报警，不绑定任何固定GPIO
*******************************************************************************/
int ab_init(ab_app_t *app, const ab_config_t *config, const ab_io_t *io, al_store_t *store,
            al_reporter_t *reporter)
{
    uint64_t total;
    if (!app || !config || !io || !io->output || (!io->submit_event && (!store || !reporter)))
    {
        return AL_ERR_ARGUMENT;
    }
    total =
        (uint64_t)config->pattern.initial_ms + config->pattern.blink_ms + config->pattern.wait_ms;
    if (!config->debounce_ms || !config->pattern.half_period_ms || total >= 0x80000000ULL ||
        config->heartbeat_ms >= 0x80000000U || !config->heartbeat_ms)
    {
        return AL_ERR_CONFIG;
    }
    memset(app, 0, sizeof(*app));
    app->config = *config;
    app->io = *io;
    app->store = store;
    app->reporter = reporter;
    return AL_OK;
}

/*******************************************************************************
* Function Name  : ab_fault
* Description    : 保留并报告本地故障，禁止伪装上传成功
* Input          : app - 产品上下文；error - 明确错误码
* Output         : 保存last_error并调用故障回调
* Return         : 无
* Attention      : 本地错误禁止进入正常休眠
*******************************************************************************/
static void ab_fault(ab_app_t *app, int error)
{
    app->last_error = error;
    if (app->io.fault)
    {
        app->io.fault(app->io.user, error);
    }
}

/*******************************************************************************
* Function Name  : ab_poll
* Description    : 确认每次按下后创建独立事件，提示和联网异步并行
* Input          : app - 产品上下文；pressed - 原始逻辑电平；now - 毫秒；telemetry - 最近有效快照
* Output         : 有效按下时持久化新事件，持续更新期望声光
* Return         : 无；异常写入last_error并回调
* Attention      : 单产品任务调用；Flash提交耗时需上板测量
*******************************************************************************/
void ab_poll(ab_app_t *app, bool pressed, uint32_t now, const al_event_t *telemetry)
{
    al_output_t out;
    al_event_t event;
    uint32_t id = 0;
    int r;
    if (!app)
    {
        return;
    }
    if (al_key_sample(&app->key, pressed, now, app->config.debounce_ms) == AL_KEY_PRESS)
    {
        /* 在Flash操作前立即点亮，持久化失败仍按未确认提示处理。 */
        if (!app->io.output(app->io.user, true, false))
        {
            ab_fault(app, AL_ERR_NOT_READY);
        }
        memset(&event, 0, sizeof(event));
        if (telemetry)
        {
            event = *telemetry;
        }
        event.id = 0;
        event.uptime_ms = now;
        event.event_type = AB_EMERGENCY_EVENT;
        app->background_idle = false;
        if (app->io.submit_event)
        {
            /* No file or network I/O in the foreground. Zero ID rejects early ACKs. */
            app->latest_event_id = 0;
            al_indicator_start(&app->indicator, &app->config.pattern, 0, now);
            if (app->latest_request == UINT32_MAX)
            {
                ab_fault(app, AL_ERR_EXHAUSTED);
            }
            else
            {
                ++app->latest_request;
                r = app->io.submit_event(app->io.user, app->latest_request, &event);
                if (r == AL_OK)
                {
                    ++app->pending_saves;
                }
                else
                {
                    ab_fault(app, r);
                }
            }
        }
        else
        {
            /* Portable synchronous harness; production binds the asynchronous submit interface. */
            r = al_store_enqueue(app->store, &event, &id);
            app->latest_event_id = id;
            al_indicator_start(&app->indicator, &app->config.pattern, id, now);
            if (r)
            {
                ab_fault(app, r);
            }
            else
            {
                app->last_error = AL_OK;
            }
        }
    }
    out = al_indicator_tick(&app->indicator, now);
    if (!app->io.output(app->io.user, out.led, out.buzzer))
    {
        ab_fault(app, AL_ERR_NOT_READY);
    }
    app->output = out;
}

/*******************************************************************************
* Function Name  : ab_ack
* Description    : 业务回执完成对应持久事件，再匹配最近一次提示
* Input          : app - 产品上下文；sequence/response - 经验证的业务回执；now - 毫秒
* Output         : 完成对应持久记录，仅匹配最近事件的提示
* Return         : AL_OK或业务/存储错误
* Attention      : MQTT发送成功不得调用为业务成功
*******************************************************************************/
int ab_ack(ab_app_t *app, uint16_t sequence, uint8_t response, uint32_t now)
{
    uint32_t id = 0;
    int r;
    if (!app || !app->reporter)
    {
        return AL_ERR_ARGUMENT;
    }
    r = al_reporter_ack(app->reporter, sequence, response, now, &id);
    if (r == AL_OK)
    {
        al_indicator_ack(&app->indicator, id);
    }
    else if (r != AL_ERR_STALE && r != AL_ERR_REJECTED)
    {
        ab_fault(app, r);
    }
    return r;
}

/*******************************************************************************
* Function Name  : ab_can_sleep
* Description    : 无记录、无声光、按键释放且无故障时才允许休眠
* Input          : app - 产品上下文
* Output         : 无
* Return         : true - 本地业务已静止；false - 仍有工作或故障
* Attention      : 平台层还须确认网络已退出、后台查询已完成且唤醒已验证
*******************************************************************************/
bool ab_can_sleep(const ab_app_t *app)
{
    if (!app || app->last_error != AL_OK || app->key.stable || app->key.candidate ||
        app->indicator.active || app->pending_saves)
    {
        return false;
    }
    if (app->io.submit_event)
    {
        return app->background_idle;
    }
    return app->store && app->store->ready && app->store->image.count == 0;
}

/*******************************************************************************
* Function Name  : ab_saved
* Description    : 处理按请求顺序返回的后台持久化结果
* Input          : app - 前台上下文；request - 请求编号；event_id - 持久事件；result - 保存状态
* Output         : 更新待保存数量及最近提示对应的事件ID
* Return         : 无
* Attention      : 旧请求结果不能重新绑定新提示；重复完成不会重复减计数
*******************************************************************************/
void ab_saved(ab_app_t *app, uint32_t request, uint32_t event_id, int result)
{
    if (!app || !request || request <= app->completed_request || request > app->latest_request)
    {
        return;
    }
    app->completed_request = request;
    if (app->pending_saves)
    {
        --app->pending_saves;
    }
    if (result != AL_OK || event_id == 0)
    {
        ab_fault(app, result == AL_OK ? AL_ERR_STORAGE : result);
        return;
    }
    if (request == app->latest_request)
    {
        app->latest_event_id = event_id;
        app->indicator.event_id = event_id;
        app->last_error = AL_OK;
    }
}

/*******************************************************************************
* Function Name  : ab_confirmed
* Description    : 处理后台已持久完成的业务确认
* Input          : app - 前台上下文；event_id - 完成的事件ID
* Output         : 匹配时标注提示确认
* Return         : 无
* Attention      : 不以MQTT发送成功调用；不打断前四秒声光
*******************************************************************************/
void ab_confirmed(ab_app_t *app, uint32_t event_id)
{
    if (app)
    {
        al_indicator_ack(&app->indicator, event_id);
    }
}
