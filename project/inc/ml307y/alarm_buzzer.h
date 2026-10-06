#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "product_interface.h"

/*-------------------------------------------define---------------------------------------------*/
/* XCL-5020ATP 规格书：4 kHz、50% 方波。修改这两个宏后重新构建即可。 */
#ifndef ALARM_BUZZER_FREQUENCY_HZ
#define ALARM_BUZZER_FREQUENCY_HZ 4000U /* 声频，单位 Hz；本板必须使用非零 PWM。 */
#endif
#ifndef ALARM_BUZZER_DUTY_PERCENT
#define ALARM_BUZZER_DUTY_PERCENT 50U /* 发声时的高电平百分比；静音固定为 0%。 */
#endif
/* 下限确保周期不超过 SDK 的 5120000 ns；占空比禁止配置为恒定电平。 */
#if ALARM_BUZZER_FREQUENCY_HZ < 200 || ALARM_BUZZER_FREQUENCY_HZ > 20000
#error "Buzzer frequency must be between 200 and 20000 Hz"
#endif
#if ALARM_BUZZER_DUTY_PERCENT < 1 || ALARM_BUZZER_DUTY_PERCENT > 99
#error "Buzzer duty must be between 1 and 99 percent"
#endif

/*-------------------------------------------function---------------------------------------------*/
/* 物理 74 脚 PWM0 初始化静音；频率参数采用上面的宏，零值仅保留给有源兼容调用。 */
bool ml307y_alarm_buzzer_init(alarm_buzzer_interface_t *buzzer, int sdk_pin, uint32_t frequency_hz);
