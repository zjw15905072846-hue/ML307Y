/*------------------------------------------includes--------------------------------------------*/
#include "board.h"
#include "alarm_core.h"
#include <string.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : ab_board_init
* Description    : 检查板级映射及三路资源独占后绑定硬件接口
* Input          : board - 板上下文；config - 经核验映射；ops - 显式硬件接口
* Output         : 成功后绑定端口、关闭输出并置ready
* Return         : AL_OK或配置/硬件未就绪错误
* Attention      : 资源冲突或映射未核验时不初始化硬件
*******************************************************************************/
int ab_board_init(ab_board_t *board, const ab_board_config_t *config, const ab_board_ops_t *ops)
{
    if (!board || !config || !ops || !ops->initialize || !ops->read_key || !ops->write_outputs ||
        !ops->read_vbat)
    {
        return AL_ERR_ARGUMENT;
    }
    memset(board, 0, sizeof(*board));
    board->config = *config;
    board->ops = *ops;
    if (!config->pinmap_verified || config->key_sdk_pin < 0 || config->led_sdk_pin < 0 ||
        config->buzzer_sdk_pin < 0)
    {
        return AL_ERR_NOT_READY;
    }
    if (config->key_sdk_pin == config->led_sdk_pin ||
        config->key_sdk_pin == config->buzzer_sdk_pin ||
        config->led_sdk_pin == config->buzzer_sdk_pin || config->buzzer_hz > 20000)
    {
        return AL_ERR_CONFIG;
    }
    if (!ops->initialize(ops->user, config))
    {
        return AL_ERR_NOT_READY;
    }
    board->ready = true;
    if (!ops->write_outputs(ops->user, false, false))
    {
        board->ready = false;
        return AL_ERR_NOT_READY;
    }
    return AL_OK;
}

/*******************************************************************************
* Function Name  : ab_board_key
* Description    : 读取有效低电平逻辑按键，失效不能伪装松手
* Input          : board - 已初始化板；pressed - 输出逻辑按下状态
* Output         : pressed - 任一并联按键闭合时为true
* Return         : true - 读取成功；false - 无有效读数
* Attention      : 不在驱动层消抖；读取失败不得解释为松开
*******************************************************************************/
bool ab_board_key(ab_board_t *board, bool *pressed)
{
    return board && board->ready && pressed && board->ops.read_key(board->ops.user, pressed);
}

/*******************************************************************************
* Function Name  : ab_board_output
* Description    : 所有产品声光只经此入口驱动，缓存成功电平避免反复重启PWM
* Input          : user - 板上下文；led/buzzer - 期望开关状态
* Output         : 成功后更新已应用电平缓存
* Return         : true - 已应用或无需改变；false - 驱动失败
* Attention      : 产品任务是唯一声光控制方，缓存失败时不推进
*******************************************************************************/
bool ab_board_output(void *user, bool led, bool buzzer)
{
    ab_board_t *board = user;
    if (!board || !board->ready)
    {
        return false;
    }
    if (led == board->led && buzzer == board->buzzer)
    {
        return true;
    }
    if (!board->ops.write_outputs(board->ops.user, led, buzzer))
    {
        return false;
    }
    board->led = led;
    board->buzzer = buzzer;
    return true;
}

/*******************************************************************************
* Function Name  : ab_board_vbat
* Description    : 读取内部电源采样，拒绝不合理数值
* Input          : board - 已初始化板；millivolts - 输出电压
* Output         : millivolts - 内部VBAT采样
* Return         : true - 成功且在2500..4500mV；false - 无效
* Attention      : 不采样用于LED的ADC1；可在后台任务调用
*******************************************************************************/
bool ab_board_vbat(ab_board_t *board, uint16_t *millivolts)
{
    return board && board->ready && millivolts &&
           board->ops.read_vbat(board->ops.user, millivolts) && *millivolts >= 2500 &&
           *millivolts <= 4500;
}
