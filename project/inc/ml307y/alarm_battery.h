#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "product_interface.h"

/*-------------------------------------------define---------------------------------------------*/
/* 2026-09-29 用户批准电压估算。端点是联调配置，不是协议或电芯标定曲线。 */
#ifndef ALARM_BATTERY_EMPTY_MILLIVOLTS
#define ALARM_BATTERY_EMPTY_MILLIVOLTS 3000U
#endif
#ifndef ALARM_BATTERY_FULL_MILLIVOLTS
#define ALARM_BATTERY_FULL_MILLIVOLTS 4200U
#endif
/* 保留原有有效电压范围；低于空电端点仍可上报真实电压和估算 0%。 */
#ifndef ALARM_BATTERY_MINIMUM_MILLIVOLTS
#define ALARM_BATTERY_MINIMUM_MILLIVOLTS 2500U
#endif
#ifndef ALARM_BATTERY_MAXIMUM_MILLIVOLTS
#define ALARM_BATTERY_MAXIMUM_MILLIVOLTS 4500U
#endif
/* 每次业务采样读取奇数次，取中值；任一次失败则本次采样无效。 */
#ifndef ALARM_BATTERY_SAMPLE_COUNT
#define ALARM_BATTERY_SAMPLE_COUNT 5U
#endif

/*-------------------------------------------function---------------------------------------------*/
/* 绑定内部 VBAT 采样及同一电压的估算接口；不使用外部 ADC 引脚。 */
void ml307y_alarm_battery_bind(alarm_battery_interface_t *battery);
