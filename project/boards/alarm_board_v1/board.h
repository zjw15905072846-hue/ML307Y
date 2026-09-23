#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include "battery_if.h"
/*-------------------------------------------define---------------------------------------------*/
#define AB_MODULE_KEY_PIN 26
#define AB_MODULE_LED_PIN 96
#define AB_MODULE_BUZZER_PIN 74

/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    int key_sdk_pin;
    int led_sdk_pin;
    int buzzer_sdk_pin;
    bool pinmap_verified;
    bool wake_verified;
    uint32_t buzzer_hz; /* 0=已确认有源蜂鸣器，非0=无源蜂鸣器载波 */
} ab_board_config_t;

typedef struct
{
    void *user;
    bool (*initialize)(void *user, const ab_board_config_t *config);
    bool (*read_key)(void *user, bool *pressed);
    bool (*write_outputs)(void *user, bool led, bool buzzer);
    battery_read_mv_fn read_vbat;
} ab_board_ops_t;

typedef struct
{
    ab_board_config_t config;
    ab_board_ops_t ops;
    bool ready;
    bool led;
    bool buzzer;
} ab_board_t;

/*-------------------------------------------function---------------------------------------------*/
/* 未确认引脚或资源重叠时拒绝初始化；硬件能力不靠网名推断。 */
int ab_board_init(ab_board_t *board, const ab_board_config_t *config, const ab_board_ops_t *ops);
bool ab_board_key(ab_board_t *board, bool *pressed);
bool ab_board_output(void *board, bool led, bool buzzer);
bool ab_board_vbat(ab_board_t *board, uint16_t *millivolts);
