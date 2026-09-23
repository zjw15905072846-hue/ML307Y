/*------------------------------------------includes--------------------------------------------*/
#include "ml307y_port.h"
#include "board.h"
#include "product_build_config.h"
#include "cm_gpio.h"
#include "cm_iomux.h"
#include "cm_pwm.h"
#include "cm_adc.h"
#include <stdlib.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    ab_board_t board;
    ab_board_config_t config;
    void (*wake_notify)(void *);
    void *wake_argument;
} ml_alarm_hal_t;

/*-------------------------------------------variables-------------------------------------------*/
/* Verified software table in the shipped open_mode GPIO library. Physical 26/96 are absent. */
static const int s_pad_by_gpio[] = {16, 25, 49, 76, 77, 86, 87, 17, 18,
                                    19, 22, 23, 28, 29, 20, 21, 74, 75};
static ml_alarm_hal_t *s_irq_owner;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : ml_pad
* Description    : 查询当前SDK公开GPIO编号对应的物理焊盘
* Input          : gpio - CM逻辑编号
* Output         : 无
* Return         : 物理脚号或-1
* Attention      : 不能将模组物理编号直接用作CM_GPIO_NUM
*******************************************************************************/
static int ml_pad(int gpio)
{
    if (gpio < 0 || gpio >= (int)(sizeof(s_pad_by_gpio) / sizeof(s_pad_by_gpio[0])))
    {
        return -1;
    }
    return s_pad_by_gpio[gpio];
}

/*******************************************************************************
* Function Name  : ml_key_interrupt
* Description    : 将键边沿通知前台，具体消抖仍在产品任务
* Input          : 无
* Output         : 零等待唤醒消息
* Return         : 无
* Attention      : 中断内不写Flash、不联网、不控制声光
*******************************************************************************/
static void ml_key_interrupt(void)
{
    if (s_irq_owner && s_irq_owner->wake_notify)
    {
        s_irq_owner->wake_notify(s_irq_owner->wake_argument);
    }
}

/*******************************************************************************
* Function Name  : ml_alarm_initialize
* Description    : 按核验结果设置引脚复用及输入输出
* Input          : user - 板端口；config - 经核验映射
* Output         : GPIO与PWM配置
* Return         : true初始化完成
* Attention      : 原板26和96脚未被当前公开表覆盖时拒绝启用
*******************************************************************************/
static bool ml_alarm_initialize(void *user, const ab_board_config_t *config)
{
    ml_alarm_hal_t *hal = user;
    cm_gpio_cfg_t key = {CM_GPIO_MODE_NUM, CM_GPIO_DIRECTION_INPUT, CM_GPIO_PULL_UP};
    cm_gpio_cfg_t output = {CM_GPIO_MODE_NUM, CM_GPIO_DIRECTION_OUTPUT, CM_GPIO_PULL_DOWN};
    if (!config->pinmap_verified || ml_pad(config->key_sdk_pin) != AB_MODULE_KEY_PIN ||
        ml_pad(config->led_sdk_pin) != AB_MODULE_LED_PIN ||
        ml_pad(config->buzzer_sdk_pin) != AB_MODULE_BUZZER_PIN ||
        (s_irq_owner && s_irq_owner != hal))
    {
        return false;
    }
    hal->config = *config;
    if (cm_iomux_set_pin_func((cm_iomux_pin_e)AB_MODULE_KEY_PIN, CM_IOMUX_FUNC_FUNCTION2) != 0 ||
        cm_iomux_set_pin_func((cm_iomux_pin_e)AB_MODULE_LED_PIN, CM_IOMUX_FUNC_FUNCTION2) != 0 ||
        cm_gpio_init((cm_gpio_num_e)config->key_sdk_pin, &key) != 0 ||
        cm_gpio_init((cm_gpio_num_e)config->led_sdk_pin, &output) != 0 ||
        cm_gpio_set_level((cm_gpio_num_e)config->led_sdk_pin, CM_GPIO_LEVEL_LOW) != 0)
    {
        return false;
    }
    if (config->buzzer_hz)
    {
        if (cm_iomux_set_pin_func(CM_IOMUX_PIN_74, CM_IOMUX_FUNC_FUNCTION1) != 0 ||
            cm_pwm_open_ns(CM_PWM_DEV_0, 1000000000U / config->buzzer_hz, 0) != 0)
        {
            return false;
        }
    }
    else if (cm_iomux_set_pin_func(CM_IOMUX_PIN_74, CM_IOMUX_FUNC_FUNCTION2) != 0 ||
             cm_gpio_init((cm_gpio_num_e)config->buzzer_sdk_pin, &output) != 0 ||
             cm_gpio_set_level((cm_gpio_num_e)config->buzzer_sdk_pin, CM_GPIO_LEVEL_LOW) != 0)
    {
        return false;
    }
    s_irq_owner = hal;
    if (cm_gpio_interrupt_register((cm_gpio_num_e)config->key_sdk_pin, ml_key_interrupt) != 0 ||
        cm_gpio_interrupt_enable((cm_gpio_num_e)config->key_sdk_pin, CM_GPIO_IT_EDGE_BOTH) != 0)
    {
        return false;
    }
    return true;
}

/*******************************************************************************
* Function Name  : ml_alarm_key
* Description    : 读取按下接地的并联逻辑按键
* Input          : user - 板上下文；pressed - 输出
* Output         : pressed
* Return         : true有效读取
* Attention      : 读取失败不伪装成松开
*******************************************************************************/
static bool ml_alarm_key(void *user, bool *pressed)
{
    ml_alarm_hal_t *hal = user;
    cm_gpio_level_e level;
    if (cm_gpio_get_level((cm_gpio_num_e)hal->config.key_sdk_pin, &level) != 0)
    {
        return false;
    }
    *pressed = level == CM_GPIO_LEVEL_LOW;
    return true;
}

/*******************************************************************************
* Function Name  : ml_alarm_outputs
* Description    : 唯一声光驱动入口，节奏由公共执行器决定
* Input          : user - 板；led/buzzer - 开关
* Output         : 引脚电平及PWM门控
* Return         : true全部成功
* Attention      : 无源蜂鸣器静音保持同一复用并使用零占空比
*******************************************************************************/
static bool ml_alarm_outputs(void *user, bool led, bool buzzer)
{
    ml_alarm_hal_t *hal = user;
    uint32_t period;
    if (cm_gpio_set_level((cm_gpio_num_e)hal->config.led_sdk_pin,
                          led ? CM_GPIO_LEVEL_HIGH : CM_GPIO_LEVEL_LOW) != 0)
    {
        return false;
    }
    if (hal->config.buzzer_hz)
    {
        period = 1000000000U / hal->config.buzzer_hz;
        return cm_pwm_open_ns(CM_PWM_DEV_0, period, buzzer ? period / 2U : 0U) == 0;
    }
    return cm_gpio_set_level((cm_gpio_num_e)hal->config.buzzer_sdk_pin,
                             buzzer ? CM_GPIO_LEVEL_HIGH : CM_GPIO_LEVEL_LOW) == 0;
}

/*******************************************************************************
* Function Name  : ml_alarm_vbat
* Description    : 通过SDK内部VBAT采样读取电池电压
* Input          : user - 保留；millivolts - 输出
* Output         : millivolts
* Return         : true有效
* Attention      : 不使用连接LED的ADC1，不构造充电状态
*******************************************************************************/
static bool ml_alarm_vbat(void *user, uint16_t *millivolts)
{
    uint32_t value;
    (void)user;
    if (cm_adc_vbat_read(&value) != 0 || value < 2500 || value > 4500)
    {
        return false;
    }
    *millivolts = (uint16_t)value;
    return true;
}

/*******************************************************************************
* Function Name  : ml_board_key
* Description    : 将通用板接口转发到已检查的报警板
* Input          : user - 板；pressed - 输出
* Output         : 按键状态
* Return         : true成功
* Attention      : 保留板初始化门控
*******************************************************************************/
static bool ml_board_key(void *user, bool *pressed)
{
    return ab_board_key(&((ml_alarm_hal_t *)user)->board, pressed);
}

/*******************************************************************************
* Function Name  : ml_board_output
* Description    : 经板级缓存应用声光状态
* Input          : user - 板；led/buzzer - 开关
* Output         : 实际声光
* Return         : true成功
* Attention      : GPIO只能由此入口控制
*******************************************************************************/
static bool ml_board_output(void *user, bool led, bool buzzer)
{
    return ab_board_output(&((ml_alarm_hal_t *)user)->board, led, buzzer);
}

/*******************************************************************************
* Function Name  : ml_board_vbat
* Description    : 经板级校验返回内部电压
* Input          : user - 板；millivolts - 输出
* Output         : 电压
* Return         : true有效
* Attention      : 后台任务调用
*******************************************************************************/
static bool ml_board_vbat(void *user, uint16_t *millivolts)
{
    return ab_board_vbat(&((ml_alarm_hal_t *)user)->board, millivolts);
}

/*******************************************************************************
* Function Name  : ml_board_wakeup
* Description    : 绑定前台非阻塞唤醒通知
* Input          : user - 板；notify/argument - 回调
* Output         : 板唤醒回调
* Return         : 无
* Attention      : 通知函数只投递消息
*******************************************************************************/
static void ml_board_wakeup(void *user, void (*notify)(void *), void *argument)
{
    ml_alarm_hal_t *hal = user;
    hal->wake_argument = argument;
    hal->wake_notify = notify;
}

/*******************************************************************************
* Function Name  : alarm_board_prepare
* Description    : 把选中报警板绑定到产品服务
* Input          : services - 服务容器
* Output         : 板接口及能力
* Return         : true就绪
* Attention      : 未核验映射只输出诊断，不猜接线、不初始化持久队列
*******************************************************************************/
bool alarm_board_prepare(product_services_t *services)
{
    ml_alarm_hal_t *hal = calloc(1, sizeof(*hal));
    ab_board_ops_t ops;
    ab_board_config_t config = {AB_KEY_SDK_PIN,     AB_LED_SDK_PIN,   AB_BUZZER_SDK_PIN,
                                AB_PINMAP_VERIFIED, AB_WAKE_VERIFIED, AB_BUZZER_HZ};
    int result;
    if (!hal)
    {
        return false;
    }
    ops.user = hal;
    ops.initialize = ml_alarm_initialize;
    ops.read_key = ml_alarm_key;
    ops.write_outputs = ml_alarm_outputs;
    ops.read_vbat = ml_alarm_vbat;
    result = ab_board_init(&hal->board, &config, &ops);
    if (result != 0)
    {
        services->system.fault("alarm-board-unverified-pin26-pin96", result);
        free(hal);
        return false;
    }
    services->board.user = hal;
    services->board.read_key = ml_board_key;
    services->board.outputs = ml_board_output;
    services->board.vbat = ml_board_vbat;
    services->board.set_wakeup = ml_board_wakeup;
    services->board.ready = true;
    services->board.wake_verified = config.wake_verified;
    return true;
}
