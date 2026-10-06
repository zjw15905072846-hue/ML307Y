/*------------------------------------------includes--------------------------------------------*/
#include "ml307y/alarm_battery.h"
#include "cm_adc.h"
#include <assert.h>
#include <stdio.h>

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
static uint32_t readings[9] = {4400U, 3700U, 3500U, 3800U, 3600U, 3900U, 3400U, 4000U, 3300U};
static unsigned calls;
static unsigned failed_call;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : cm_adc_vbat_read
* Description    : 注入电压序列和指定采样失败
* Input          : voltage - 输出地址
* Output         : voltage - 当前模拟毫伏值
* Return         : 0 - 成功；-7 - 指定采样失败
* Attention      : 仅供模拟器测试，不调用真实 ADC
*******************************************************************************/
int32_t cm_adc_vbat_read(uint32_t *voltage)
{
    assert(voltage != NULL);
    ++calls;
    if (calls == failed_call)
    {
        return -7;
    }
    *voltage = readings[(calls - 1U) % ALARM_BATTERY_SAMPLE_COUNT];
    return 0;
}

/*******************************************************************************
* Function Name  : main
* Description    : 验证中值滤波、失败保护及百分比估算的端点、单调性和限幅
* Input          : 无
* Output         : 测试断言及结果
* Return         : 0 - 全部通过
* Attention      : 不代表实板电压精度或电池容量标定
*******************************************************************************/
int main(void)
{
    alarm_battery_interface_t battery = {0};
    uint16_t millivolts = 1234U;
    uint8_t percent = 255U;
    uint8_t previous = 0U;
    uint32_t voltage;
    unsigned index;
    ml307y_alarm_battery_bind(NULL);
    ml307y_alarm_battery_bind(&battery);
    assert(battery.ready && battery.read_voltage != NULL);
    assert(!battery.read_voltage(battery.user, NULL) && calls == 0U);
    assert(battery.read_voltage(battery.user, &millivolts));
    assert(millivolts == 3700U && calls == ALARM_BATTERY_SAMPLE_COUNT);
    for (index = 1U; index <= ALARM_BATTERY_SAMPLE_COUNT; ++index)
    {
        calls = 0U;
        failed_call = index;
        millivolts = 1234U;
        assert(!battery.read_voltage(battery.user, &millivolts));
        assert(millivolts == 1234U && calls == index);
    }
    failed_call = 0U;
    calls = 0U;
    readings[2] = ALARM_BATTERY_MINIMUM_MILLIVOLTS - 1U;
    assert(!battery.read_voltage(battery.user, &millivolts));
    assert(millivolts == 1234U);
    calls = 0U;
    readings[2] = ALARM_BATTERY_MAXIMUM_MILLIVOLTS + 1U;
    assert(!battery.read_voltage(battery.user, &millivolts));
    assert(millivolts == 1234U);
    assert(battery.estimate_percent != NULL);
    assert(!battery.estimate_percent(battery.user, 3700U, NULL));
    assert(!battery.estimate_percent(battery.user, ALARM_BATTERY_MINIMUM_MILLIVOLTS - 1U, &percent));
    assert(percent == 255U);
    assert(!battery.estimate_percent(battery.user, ALARM_BATTERY_MAXIMUM_MILLIVOLTS + 1U, &percent));
    assert(percent == 255U);
    assert(battery.estimate_percent(battery.user, ALARM_BATTERY_EMPTY_MILLIVOLTS, &percent) && percent == 0U);
    assert(battery.estimate_percent(battery.user, ALARM_BATTERY_FULL_MILLIVOLTS, &percent) && percent == 100U);
    assert(battery.estimate_percent(battery.user,
        (ALARM_BATTERY_EMPTY_MILLIVOLTS + ALARM_BATTERY_FULL_MILLIVOLTS) / 2U, &percent) && percent == 50U);
    for (voltage = ALARM_BATTERY_MINIMUM_MILLIVOLTS; voltage <= ALARM_BATTERY_MAXIMUM_MILLIVOLTS; ++voltage)
    {
        assert(battery.estimate_percent(battery.user, (uint16_t)voltage, &percent));
        assert(percent <= 100U && percent >= previous);
        if (voltage <= ALARM_BATTERY_EMPTY_MILLIVOLTS)
        {
            assert(percent == 0U);
        }
        if (voltage >= ALARM_BATTERY_FULL_MILLIVOLTS)
        {
            assert(percent == 100U);
        }
        previous = percent;
    }
    puts("battery: median, SDK failures, invalid voltage, estimate endpoints and monotonicity passed");
    return 0;
}
