#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "product_interface.h"

/*-------------------------------------------function---------------------------------------------*/
/* 经配套底包把物理 26 脚配置为 AGPIO0 输入上拉；wake_verified 必须为 false。 */
bool ml307y_alarm_key_init(alarm_key_interface_t *key, bool wake_verified);
