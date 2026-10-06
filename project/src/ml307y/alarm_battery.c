/*------------------------------------------includes--------------------------------------------*/
#include "ml307y/alarm_battery.h"
#include "cm_adc.h"

/*-------------------------------------------define---------------------------------------------*/
#if ALARM_BATTERY_SAMPLE_COUNT < 3U || ALARM_BATTERY_SAMPLE_COUNT > 9U || (ALARM_BATTERY_SAMPLE_COUNT % 2U) == 0U
#error "ALARM_BATTERY_SAMPLE_COUNT must be an odd number from 3 to 9"
#endif
#if ALARM_BATTERY_MINIMUM_MILLIVOLTS == 0U || ALARM_BATTERY_MAXIMUM_MILLIVOLTS > 65535U || \
    ALARM_BATTERY_MINIMUM_MILLIVOLTS > ALARM_BATTERY_EMPTY_MILLIVOLTS || \
    ALARM_BATTERY_EMPTY_MILLIVOLTS >= ALARM_BATTERY_FULL_MILLIVOLTS || \
    ALARM_BATTERY_FULL_MILLIVOLTS > ALARM_BATTERY_MAXIMUM_MILLIVOLTS
#error "Battery limits must satisfy 0 < minimum <= empty < full <= maximum <= 65535"
#endif
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : ml307y_alarm_battery_read_voltage
* Description    : 通过内部 VBAT 通道多次采样并取中值，抑制孤立异常读数
* Input          : user - 保留；millivolts - 输出地址
* Output         : millivolts - 成功时写入滤波后的毫伏值；失败时不改写
* Return         : true - 全部采样有效；false - 空地址、SDK 失败或读数越界
* Attention      : 仅后台业务调用；不占用 ADC1，不以旧数据填补失败读数
*******************************************************************************/
static bool ml307y_alarm_battery_read_voltage(void *user, uint16_t *millivolts)
{
    uint32_t value;
    uint16_t samples[ALARM_BATTERY_SAMPLE_COUNT];
    unsigned index;
    unsigned position;
    (void)user;
    if (!millivolts)
    {
        return false;
    }
    for (index = 0U; index < ALARM_BATTERY_SAMPLE_COUNT; ++index)
    {
        if (cm_adc_vbat_read(&value) != 0 || value < ALARM_BATTERY_MINIMUM_MILLIVOLTS ||
            value > ALARM_BATTERY_MAXIMUM_MILLIVOLTS)
        {
            return false;
        }
        position = index;
        while (position > 0U && samples[position - 1U] > value)
        {
            samples[position] = samples[position - 1U];
            --position;
        }
        samples[position] = (uint16_t)value;
    }
    *millivolts = samples[ALARM_BATTERY_SAMPLE_COUNT / 2U];
    return true;
}

/*******************************************************************************
* Function Name  : ml307y_alarm_battery_estimate_percent
* Description    : 将同一次有效电压按可配置空满端点线性估算为百分比
* Input          : user - 保留；millivolts - 已采集毫伏值；percent - 输出地址
* Output         : percent - 成功时写入 0..100，失败时不改写
* Return         : true - 估算成功；false - 空地址或电压超出有效范围
* Attention      : 四舍五入并限幅；仅供联调估算，不等同真实剩余容量或充满状态
*******************************************************************************/
static bool ml307y_alarm_battery_estimate_percent(void *user, uint16_t millivolts, uint8_t *percent)
{
    uint32_t span = ALARM_BATTERY_FULL_MILLIVOLTS - ALARM_BATTERY_EMPTY_MILLIVOLTS;
    (void)user;
    if (!percent || millivolts < ALARM_BATTERY_MINIMUM_MILLIVOLTS ||
        millivolts > ALARM_BATTERY_MAXIMUM_MILLIVOLTS)
    {
        return false;
    }
    if (millivolts <= ALARM_BATTERY_EMPTY_MILLIVOLTS)
    {
        *percent = 0U;
    }
    else if (millivolts >= ALARM_BATTERY_FULL_MILLIVOLTS)
    {
        *percent = 100U;
    }
    else
    {
        *percent = (uint8_t)(((uint32_t)(millivolts - ALARM_BATTERY_EMPTY_MILLIVOLTS) * 100U +
            span / 2U) / span);
    }
    return true;
}

/*******************************************************************************
* Function Name  : ml307y_alarm_battery_bind
* Description    : 将内部 VBAT 采样与电量估算函数绑定到产品电池接口
* Input          : battery - 产品电池接口
* Output         : 电池接口就绪状态
* Return         : 无
* Attention      : 当前 SDK 无需另行初始化内部 VBAT 通道
*******************************************************************************/
void ml307y_alarm_battery_bind(alarm_battery_interface_t *battery)
{
    if (battery)
    {
        battery->user = NULL;
        battery->read_voltage = ml307y_alarm_battery_read_voltage;
        battery->estimate_percent = ml307y_alarm_battery_estimate_percent;
        battery->ready = true;
    }
}
