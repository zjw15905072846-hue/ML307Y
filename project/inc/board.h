#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include "battery_interface.h"
/*-------------------------------------------define---------------------------------------------*/
/* 原理图上的模组物理脚号，不能直接当作 CM SDK GPIO 编号。 */
#define ALARM_BUTTON_MODULE_KEY_PIN 26
#define ALARM_BUTTON_MODULE_LED_PIN 96
#define ALARM_BUTTON_MODULE_BUZZER_PIN 74

/*-------------------------------------------typedef---------------------------------------------*/
/* SDK 引脚映射与蜂鸣器载波必须经本板实测后才能使能输出。 */
typedef struct
{
    int key_sdk_pin; /* 两个并联按键的输入映射；负值表示未绑定。 */
    int led_sdk_pin; /* 灯输出的 SDK 映射；负值表示未绑定。 */
    int buzzer_sdk_pin; /* 蜂鸣器输出的 SDK 映射；负值表示未绑定。 */
    bool pinmap_verified; /* 映射未核验时板初始化必须失败。 */
    bool wake_verified; /* 休眠唤醒核验独立于普通 GPIO 读写。 */
    uint32_t buzzer_hz; /* 0=已确认有源蜂鸣器，非0=无源蜂鸣器载波 */
} alarm_board_config_t;

typedef struct
{
    void *user; /* 板级硬件适配上下文。 */
    /* 成功后硬件进入可控状态；失败不得宣称板已就绪。 */
    bool (*initialize)(void *user, const alarm_board_config_t *config);
    /* false 表示读失败，不能解释为按键松开。 */
    bool (*read_key)(void *user, bool *pressed);
    /* 统一写入声光目标状态；失败时上层不能更新已应用状态。 */
    bool (*write_outputs)(void *user, bool led, bool buzzer);
    battery_read_mv_function read_battery_voltage; /* 只返回真实内部 BATTERY_VOLTAGE 毫伏值。 */
} alarm_board_operations_t;

typedef struct
{
    alarm_board_config_t config;
    alarm_board_operations_t operations;
    bool ready; /* 资源检查、硬件初始化与关闭输出均成功后置位。 */
    bool led; /* 最近一次成功应用的灯状态。 */
    bool buzzer; /* 最近一次成功应用的蜂鸣器状态。 */
} alarm_board_t;

/*-------------------------------------------function---------------------------------------------*/
/* 未确认引脚或资源重叠时拒绝初始化；硬件能力不靠网名推断。 */
int alarm_board_init(alarm_board_t *board, const alarm_board_config_t *config, const alarm_board_operations_t *operations);
bool alarm_board_key(alarm_board_t *board, bool *pressed);
bool alarm_board_output(void *board, bool led, bool buzzer);
bool alarm_board_battery_voltage(alarm_board_t *board, uint16_t *millivolts);
