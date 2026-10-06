#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "alarm_button/provisioning.h"
/*-------------------------------------------define---------------------------------------------*/
#define ALARM_BUTTON_DEBOUNCE_MS 30U /* 按下和释放共用的稳定门限。 */
#ifndef ALARM_BUTTON_HEARTBEAT_MS
#define ALARM_BUTTON_HEARTBEAT_MS (ALARM_BUTTON_HEARTBEAT_HOURS * 60U * 60U * 1000U) /* 手报业务心跳周期。 */
#endif
#define ALARM_BUTTON_INITIAL_MS 1000U /* 前一秒灯常亮、蜂鸣器静音。 */
#define ALARM_BUTTON_BLINK_MS 3000U /* 随后三秒同步闪灯和鸣叫。 */
#define ALARM_BUTTON_WAIT_MS 30000U /* 固定提示后等待成功确认的最长时间。 */
#define ALARM_BUTTON_HALF_PERIOD_MS 250U /* 闪鸣开或关各占一个半周期。 */
#define ALARM_BUTTON_CONFIRMATION_TIMEOUT_MS 10000U /* 一次发送等待业务回执的门限。 */
#define ALARM_BUTTON_RETRY_MINIMUM_MS 5000U /* 失败退避起始间隔。 */
#define ALARM_BUTTON_RETRY_MAXIMUM_MS 30000U /* 失败退避上限，不改变待确认记录。 */
#ifndef ALARM_BUTTON_SLEEP_ENABLED
#define ALARM_BUTTON_SLEEP_ENABLED 1 /* 业务静止后允许 SDK 配置档位休眠；置零回退常醒。 */
#endif
#ifndef ALARM_BUTTON_SLEEP_DIAGNOSTICS
#define ALARM_BUTTON_SLEEP_DIAGNOSTICS 1 /* 普通任务打印休眠统计；回调仅记录。 */
#endif

#ifndef ALARM_BUTTON_OFFLINE_STANDBY
#define ALARM_BUTTON_OFFLINE_STANDBY 1 /* 默认业务结束后断开 MQTT、关闭射频；显式置零回退在线待机。 */
#endif
#ifndef ALARM_BUTTON_NETWORK_ATTEMPT_MS
#define ALARM_BUTTON_NETWORK_ATTEMPT_MS 120000U /* 无报警时，一轮恢复网络及控制业务的最长尝试。 */
#endif
#ifndef ALARM_BUTTON_NETWORK_RETRY_SLEEP_MS
#define ALARM_BUTTON_NETWORK_RETRY_SLEEP_MS 900000U /* 失败关网成功后等待十五分钟；按键可提前打断。 */
#endif
#define ALARM_BUTTON_NETWORK_STOP_TIMEOUT_MS 30000U /* 断连超时显式报错，保持工作锁继续收尾。 */
