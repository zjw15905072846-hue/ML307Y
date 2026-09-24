#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "product_interface.h"

/*-------------------------------------------function---------------------------------------------*/
/* 直接配置本板 26 脚输入；唤醒未核验时前台轮询读取按键。 */
bool ml307y_alarm_key_init(alarm_key_interface_t *key, bool wake_verified);
