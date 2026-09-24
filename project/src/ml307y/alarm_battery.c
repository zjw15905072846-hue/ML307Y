/*------------------------------------------includes--------------------------------------------*/
#include "ml307y/alarm_battery.h"
#include "cm_adc.h"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : ml307y_alarm_battery_read_voltage
* Description    : 通过 SDK 内部 VBAT 通道读取电池电压
* Input          : user - 保留；millivolts - 输出地址
* Output         : millivolts - 有效的真实电压
* Return         : true - 电压在 2500..4500 mV；false - 读取失败或范围无效
* Attention      : 不占用 LED 的 ADC1，也不推算充电状态
*******************************************************************************/
static bool ml307y_alarm_battery_read_voltage(void *user, uint16_t *millivolts)
{
    uint32_t value;
    (void)user;
    if (!millivolts || cm_adc_vbat_read(&value) != 0 || value < 2500U || value > 4500U)
    {
        return false;
    }
    *millivolts = (uint16_t)value;
    return true;
}

/*******************************************************************************
* Function Name  : ml307y_alarm_battery_bind
* Description    : 将内部 VBAT 读取函数绑定到产品电池接口
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
        battery->ready = true;
    }
}
