#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "product_interface.h"

/*-------------------------------------------function---------------------------------------------*/
/* SDK 编号为 -1 时禁用 LED，不访问物理 96 脚。 */
bool ml307y_alarm_led_init(alarm_led_interface_t *led, int sdk_pin);
