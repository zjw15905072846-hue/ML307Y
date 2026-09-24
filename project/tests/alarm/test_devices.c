/*------------------------------------------includes--------------------------------------------*/
#include "ml307y/alarm_key.h"
#include "ml307y/alarm_led.h"
#include "ml307y/alarm_buzzer.h"
#include "ml307y/alarm_battery.h"
#include "cm_gpio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
extern unsigned alarm_mock_led_pin_touches;
extern unsigned alarm_mock_gpio_writes;
extern unsigned alarm_mock_key_iomux_calls;
extern unsigned alarm_mock_key_gpio_inits;
extern unsigned alarm_mock_key_gpio_reads;
extern unsigned alarm_mock_key_interrupts;
extern cm_gpio_level_e alarm_mock_key_level;
extern char alarm_mock_last_diagnostic[128];

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : main
* Description    : 验证直接配置 26 脚后按键可读取，且不启用未核验的唤醒
* Input          : 无
* Output         : 测试结果
* Return         : 0 - 通过
* Attention      : LED 禁用时不访问物理 96 脚
*******************************************************************************/
int main(void)
{
    alarm_key_interface_t key = {0};
    alarm_led_interface_t led = {0};
    alarm_buzzer_interface_t buzzer = {0};
    alarm_battery_interface_t battery = {0};
    uint16_t millivolts = 0;
    bool pressed = false;
    unsigned writes;
    assert(ml307y_alarm_key_init(&key, false));
    assert(key.ready);
    assert(key.set_wakeup == NULL);
    assert(alarm_mock_key_iomux_calls == 1);
    assert(alarm_mock_key_gpio_inits == 1);
    assert(alarm_mock_key_gpio_reads == 1);
    assert(alarm_mock_key_interrupts == 0);
    assert(key.read(key.user, &pressed) && !pressed);
    alarm_mock_key_level = CM_GPIO_LEVEL_LOW;
    assert(key.read(key.user, &pressed) && pressed);
    assert(!ml307y_alarm_key_init(&key, false));
    assert(strstr(alarm_mock_last_diagnostic, "alarm-key-already-initialized") != NULL);
    assert(!ml307y_alarm_led_init(&led, 26));
    assert(strstr(alarm_mock_last_diagnostic, "alarm-led-disabled") != NULL);
    assert(ml307y_alarm_led_init(&led, -1) && led.ready);
    assert(!ml307y_alarm_led_init(&led, -1));
    assert(strstr(alarm_mock_last_diagnostic, "alarm-led-already-initialized") != NULL);
    assert(led.set(led.user, true));
    assert(alarm_mock_led_pin_touches == 0);
    assert(ml307y_alarm_buzzer_init(&buzzer, 16, 0) && buzzer.ready);
    writes = alarm_mock_gpio_writes;
    assert(buzzer.set(buzzer.user, true) && alarm_mock_gpio_writes == writes + 1);
    ml307y_alarm_battery_bind(&battery);
    assert(battery.ready && battery.read_voltage(battery.user, &millivolts) && millivolts == 3700);
    puts("devices: key bound, LED disabled, buzzer and battery bound");
    return 0;
}
