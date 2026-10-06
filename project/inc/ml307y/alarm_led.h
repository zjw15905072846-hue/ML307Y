#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "product_interface.h"

/*-------------------------------------------function---------------------------------------------*/
/* sdk_pin=41 为本版 HAL GPIO_PIN_B（物理 96 脚），高亮低灭；-1 显式禁用。 */
bool ml307y_alarm_led_init(alarm_led_interface_t *led, int sdk_pin);
