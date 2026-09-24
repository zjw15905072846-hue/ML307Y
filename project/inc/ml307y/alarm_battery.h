#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "product_interface.h"

/*-------------------------------------------function---------------------------------------------*/
/* 绑定内部 VBAT 读取接口；SDK 无需额外打开 ADC 设备。 */
void ml307y_alarm_battery_bind(alarm_battery_interface_t *battery);
