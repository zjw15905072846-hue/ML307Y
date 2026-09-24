/*------------------------------------------includes--------------------------------------------*/
#include "board.h"
#include "alarm_button/alarm_core.h"
#include <string.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : alarm_board_init
* Description    : 检查板级映射及三路资源独占后绑定硬件接口
* Input          : board - 板上下文；config - 经核验映射；operations - 显式硬件接口
* Output         : 成功后绑定端口、关闭输出并置ready
* Return         : ALARM_OK或配置/硬件未就绪错误
* Attention      : 资源冲突或映射未核验时不初始化硬件
*******************************************************************************/
int alarm_board_init(alarm_board_t *board, const alarm_board_config_t *config, const alarm_board_operations_t *operations)
{
    if (!board || !config || !operations || !operations->initialize || !operations->read_key || !operations->write_outputs ||
        !operations->read_battery_voltage)
    {
        return ALARM_ERROR_ARGUMENT;
    }
    memset(board, 0, sizeof(*board));
    board->config = *config;
    board->operations = *operations;
    /* 引脚映射需先经板级核验；未知映射不能碰硬件。 */
    if (!config->pinmap_verified || config->key_sdk_pin < 0 || config->led_sdk_pin < 0 ||
        config->buzzer_sdk_pin < 0)
    {
        return ALARM_ERROR_NOT_READY;
    }
    if (config->key_sdk_pin == config->led_sdk_pin ||
        config->key_sdk_pin == config->buzzer_sdk_pin ||
        config->led_sdk_pin == config->buzzer_sdk_pin || config->buzzer_hz > 20000)
    {
        return ALARM_ERROR_CONFIG;
    }
    if (!operations->initialize(operations->user, config))
    {
        return ALARM_ERROR_NOT_READY;
    }
    board->ready = true;
    /* 初始化后先强制关闭两路输出，失败则撤销就绪状态。 */
    if (!operations->write_outputs(operations->user, false, false))
    {
        board->ready = false;
        return ALARM_ERROR_NOT_READY;
    }
    return ALARM_OK;
}

/*******************************************************************************
* Function Name  : alarm_board_key
* Description    : 读取有效低电平逻辑按键，失效不能伪装松手
* Input          : board - 已初始化板；pressed - 输出逻辑按下状态
* Output         : pressed - 任一并联按键闭合时为true
* Return         : true - 读取成功；false - 无有效读数
* Attention      : 不在驱动层消抖；读取失败不得解释为松开
*******************************************************************************/
bool alarm_board_key(alarm_board_t *board, bool *pressed)
{
    return board && board->ready && pressed && board->operations.read_key(board->operations.user, pressed);
}

/*******************************************************************************
* Function Name  : alarm_board_output
* Description    : 所有产品声光只经此入口驱动，缓存成功电平避免反复重启PWM
* Input          : user - 板上下文；led/buzzer - 期望开关状态
* Output         : 成功后更新已应用电平缓存
* Return         : true - 已应用或无需改变；false - 驱动失败
* Attention      : 产品任务是唯一声光控制方，缓存失败时不推进
*******************************************************************************/
bool alarm_board_output(void *user, bool led, bool buzzer)
{
    alarm_board_t *board = user;
    if (!board || !board->ready)
    {
        return false;
    }
    if (led == board->led && buzzer == board->buzzer)
    {
        return true;
    }
    /* 只在硬件写成功后更新缓存，失败留待下一次重试。 */
    if (!board->operations.write_outputs(board->operations.user, led, buzzer))
    {
        return false;
    }
    board->led = led;
    board->buzzer = buzzer;
    return true;
}

/*******************************************************************************
* Function Name  : alarm_board_battery_voltage
* Description    : 读取内部电源采样，拒绝不合理数值
* Input          : board - 已初始化板；millivolts - 输出电压
* Output         : millivolts - 内部BATTERY_VOLTAGE采样
* Return         : true - 成功且在2500..4500mV；false - 无效
* Attention      : 不采样用于LED的ADC1；可在后台任务调用
*******************************************************************************/
bool alarm_board_battery_voltage(alarm_board_t *board, uint16_t *millivolts)
{
    return board && board->ready && millivolts &&
           board->operations.read_battery_voltage(board->operations.user, millivolts) && *millivolts >= 2500 &&
           *millivolts <= 4500;
}
