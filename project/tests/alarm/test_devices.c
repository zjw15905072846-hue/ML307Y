/*------------------------------------------includes--------------------------------------------*/
#include "ml307y/alarm_key.h"
#include "ml307y/alarm_led.h"
#include "ml307y/alarm_buzzer.h"
#include "ml307y/alarm_battery.h"
#include "ml307y/base_gpio.h"
#include "cm_gpio.h"
#include "cm_pwm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
extern unsigned alarm_mock_led_pin_touches;
extern unsigned alarm_mock_gpio_writes;
extern unsigned alarm_mock_pwm_writes;
extern unsigned alarm_mock_pwm_closes;
extern bool alarm_mock_pwm_running;
extern cm_pwm_dev_e alarm_mock_pwm_device;
extern uint32_t alarm_mock_pwm_period;
extern uint32_t alarm_mock_pwm_high;
extern int alarm_mock_pwm_error;
extern unsigned alarm_mock_key_iomux_calls;
extern unsigned alarm_mock_key_gpio_inits;
extern unsigned alarm_mock_key_gpio_reads;
extern unsigned alarm_mock_key_interrupts;
extern cm_gpio_level_e alarm_mock_key_level;
extern char alarm_mock_last_diagnostic[128];
extern int alarm_mock_key_init_error;
extern int alarm_mock_key_read_error;
extern int alarm_mock_led_init_error;
extern int alarm_mock_led_write_error;
extern unsigned alarm_mock_led_writes;
extern bool alarm_mock_led_on;
extern int alarm_mock_key_wakeup_error;
extern void (*alarm_mock_key_notify)(void *);
extern void *alarm_mock_key_argument;
static unsigned wake_count;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : wake_key
* Description    : 验证按键唤醒通知的上下文
* Input          : argument - 唤醒计数
* Output         : 递增计数
* Return         : 无
* Attention      : 不模拟实际休眠
*******************************************************************************/
static void wake_key(void *argument)
{
    ++*(unsigned *)argument;
}
/*******************************************************************************
* Function Name  : main
* Description    : 验证按键轮询、LED 高亮低灭及蜂鸣器声频、静音与失败重试
* Input          : argc/argv - 可选 buzzer 参数用于单独复现蜂鸣器初始化失败
* Output         : 测试结果
* Return         : 0 - 通过
* Attention      : 含 HAL 失败、重复初始化和表外 CM GPIO 回归，不能代表实板验证
*******************************************************************************/
int main(int argc, char **argv)
{
    alarm_key_interface_t key = {0};
    alarm_led_interface_t led = {0};
    alarm_buzzer_interface_t buzzer = {0};
    alarm_battery_interface_t battery = {0};
    uint16_t millivolts = 0;
    bool pressed = false;
    uint32_t period = 1000000000U / ALARM_BUZZER_FREQUENCY_HZ;
    uint32_t high = period * ALARM_BUZZER_DUTY_PERCENT / 100U;
    unsigned writes;
    if (argc > 1 && strcmp(argv[1], "buzzer") == 0)
    {
        assert(ml307y_alarm_buzzer_init(&buzzer, 16, 0));
        puts("buzzer initialization passed");
        return 0;
    }
    assert(!ml307y_alarm_key_init(NULL, false));
    assert(!ml307y_alarm_key_init(&key, true));
    alarm_mock_key_init_error = -2;
    assert(!ml307y_alarm_key_init(&key, false) && !key.ready);
    alarm_mock_key_init_error = 0;
    alarm_mock_key_read_error = -5;
    assert(!ml307y_alarm_key_init(&key, false) && !key.ready);
    alarm_mock_key_read_error = 0;
    assert(ml307y_alarm_key_init(&key, false));
    assert(key.ready);
    assert(key.set_wakeup != NULL && !key.wake_configured && !key.wake_verified);
    alarm_mock_key_wakeup_error = -5;
    key.set_wakeup(key.user, wake_key, &wake_count);
    assert(!key.wake_configured && key.ready);
    alarm_mock_key_wakeup_error = 0;
    key.set_wakeup(key.user, wake_key, &wake_count);
    assert(key.wake_configured && !key.wake_verified);
    alarm_mock_key_notify(alarm_mock_key_argument);
    assert(wake_count == 1U);
    assert(alarm_mock_key_iomux_calls == 0);
    assert(alarm_mock_key_gpio_inits == 3);
    assert(alarm_mock_key_gpio_reads == 2);
    assert(alarm_mock_key_interrupts == 0);
    assert(key.read(key.user, &pressed) && !pressed);
    alarm_mock_key_level = CM_GPIO_LEVEL_LOW;
    assert(key.read(key.user, &pressed) && pressed);
    alarm_mock_key_read_error = -5;
    assert(!key.read(key.user, &pressed) && pressed);
    alarm_mock_key_read_error = 0;
    assert(!ml307y_alarm_key_init(&key, false));
    assert(strstr(alarm_mock_last_diagnostic, "alarm-key-already-initialized") != NULL);
    assert(!ml307y_alarm_led_init(&led, 26));
    alarm_mock_led_init_error = -3;
    assert(!ml307y_alarm_led_init(&led, ML307Y_LED_HAL_PIN) && !led.ready);
    alarm_mock_led_init_error = 0;
    assert(ml307y_alarm_led_init(&led, ML307Y_LED_HAL_PIN) && led.ready);
    assert(!alarm_mock_led_on);
    assert(!ml307y_alarm_led_init(&led, ML307Y_LED_HAL_PIN));
    assert(strstr(alarm_mock_last_diagnostic, "alarm-led-already-initialized") != NULL);
    assert(led.set(led.user, true) && alarm_mock_led_on);
    writes = alarm_mock_led_writes;
    assert(led.set(led.user, true) && alarm_mock_led_writes == writes);
    alarm_mock_led_write_error = -5;
    assert(!led.set(led.user, false) && alarm_mock_led_on);
    alarm_mock_led_write_error = 0;
    assert(led.set(led.user, false) && !alarm_mock_led_on);
    assert(alarm_mock_led_pin_touches == 0);
    assert(!ml307y_alarm_buzzer_init(NULL, 16, 4000U));
    assert(!ml307y_alarm_buzzer_init(&buzzer, 17, 4000U));
    assert(!ml307y_alarm_buzzer_init(&buzzer, 16, 20001U));
    alarm_mock_pwm_error = -5;
    assert(!ml307y_alarm_buzzer_init(&buzzer, 16, 4000U) && !buzzer.ready);
    alarm_mock_pwm_error = 0;
    writes = alarm_mock_gpio_writes;
    assert(ml307y_alarm_buzzer_init(&buzzer, 16, ALARM_BUZZER_FREQUENCY_HZ) && buzzer.ready);
    assert(!alarm_mock_pwm_running && alarm_mock_pwm_closes == 1U);
    assert(alarm_mock_pwm_writes == 2U);
    assert(alarm_mock_pwm_device == CM_PWM_DEV_0);
    assert(alarm_mock_pwm_period == period && alarm_mock_pwm_high == 0U);
    assert(alarm_mock_gpio_writes == writes);
    assert(!ml307y_alarm_buzzer_init(&buzzer, 16, 4000U));
    assert(buzzer.set(buzzer.user, false) && alarm_mock_pwm_writes == 2U);
    assert(buzzer.set(buzzer.user, true));
    assert(alarm_mock_pwm_period == period && alarm_mock_pwm_high == high);
    assert(alarm_mock_pwm_writes == 3U);
    assert(buzzer.set(buzzer.user, true) && alarm_mock_pwm_writes == 3U);
    alarm_mock_pwm_error = -5;
    assert(!buzzer.set(buzzer.user, false) && alarm_mock_pwm_running);
    alarm_mock_pwm_error = 0;
    assert(buzzer.set(buzzer.user, false) && !alarm_mock_pwm_running);
    assert(alarm_mock_pwm_closes == 3U);
    assert(buzzer.set(buzzer.user, false) && alarm_mock_pwm_closes == 3U);
    assert(buzzer.set(buzzer.user, true) && alarm_mock_pwm_running);
    assert(alarm_mock_pwm_high == high);
    assert(buzzer.set(buzzer.user, false) && !alarm_mock_pwm_running);
    assert(alarm_mock_gpio_writes == writes);
    ml307y_alarm_battery_bind(&battery);
    assert(battery.ready && battery.read_voltage(battery.user, &millivolts) && millivolts == 3700);
    puts("devices: key polling, active-high LED, PWM tone/silence and failure recovery passed");
    return 0;
}
