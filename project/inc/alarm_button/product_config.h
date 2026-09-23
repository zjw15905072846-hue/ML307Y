#pragma once
/*-------------------------------------------define---------------------------------------------*/
#define AB_DEBOUNCE_MS 30U /* 按下和释放共用的稳定门限。 */
#define AB_HEARTBEAT_MS (22U * 60U * 60U * 1000U) /* 手报业务心跳周期。 */
#define AB_INITIAL_MS 1000U /* 前一秒灯常亮、蜂鸣器静音。 */
#define AB_BLINK_MS 3000U /* 随后三秒同步闪灯和鸣叫。 */
#define AB_WAIT_MS 30000U /* 固定提示后等待成功确认的最长时间。 */
#define AB_HALF_PERIOD_MS 250U /* 闪鸣开或关各占一个半周期。 */
#define AB_ACK_TIMEOUT_MS 10000U /* 一次发送等待业务回执的门限。 */
#define AB_RETRY_MIN_MS 5000U /* 失败退避起始间隔。 */
#define AB_RETRY_MAX_MS 30000U /* 失败退避上限，不改变待确认记录。 */
