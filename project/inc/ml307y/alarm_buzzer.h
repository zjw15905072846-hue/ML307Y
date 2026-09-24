#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "product_interface.h"

/*-------------------------------------------function---------------------------------------------*/
/* 核验物理 74 脚映射并初始化有源 GPIO 或无源 PWM 蜂鸣器。 */
bool ml307y_alarm_buzzer_init(alarm_buzzer_interface_t *buzzer, int sdk_pin, uint32_t frequency_hz);
