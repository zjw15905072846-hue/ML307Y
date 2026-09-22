/**
 * @file        cm_demo_adc.c
 * @brief       OpenCPU adc测试例程
 * @copyright   Copyright © 2021 China Mobile IOT. All rights reserved.
 */
 

 /****************************************************************************
 * Included Files
 ****************************************************************************/
#include <stdint.h>
#include "cm_adc.h"
#include "cm_os.h"
#include "cm_demo_uart.h"
#include "cm_demo_adc.h"
#include "cm_iomux.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define CM_DEMO_ADC_LOG     cm_demo_printf
#define STR_LEN(s) (sizeof(s) - 1)
#define STR_ITEM(s) (s), STR_LEN(s)
#define CM_ADC_0_PIN    CM_IOMUX_PIN_9  // 替换为实际的引脚号，需要根据硬件原理图确定
#define CM_ADC_1_PIN    CM_IOMUX_PIN_96 // 替换为实际的引脚号，需要根据硬件原理图确定
#define CM_IOMUX_FUNC_ADC  0x1          // 参照资源综述

/****************************************************************************
 * Private Types
 ****************************************************************************/


 
/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
 
 
/****************************************************************************
 * Private Data
 ****************************************************************************/
 
 
/****************************************************************************
 * Private Functions
 ****************************************************************************/


/****************************************************************************
 * Public Functions
 ****************************************************************************/
/**
 * @brief  adc接口函数测试示例
 *
 * @return  void
 *
 * @details NONE
 */
void cm_test_adc(EmbeddedCli *cli, char *args, void *context)
{
    int32_t voltage=0;
    uint32_t voltage_vbat=0;
    int32_t temperature = 0;  // 添加温度变量
    int32_t ret;
    
    CM_DEMO_ADC_LOG("adc test start!!\n");
    
    ret = cm_adc_vbat_read(&voltage_vbat);
    if(ret != RET_SUCCESS)
    {
        CM_DEMO_ADC_LOG("adc vbat read err,ret=%d\n", ret);
        return;
    }
    CM_DEMO_ADC_LOG("adc vbat read:%ld(mv)!!\n",voltage_vbat);

    // 在读取CM_ADC_0之前设置iomux
    ret = cm_iomux_set_pin_func(CM_ADC_0_PIN, CM_IOMUX_FUNC_ADC);
    if(ret != RET_SUCCESS)
    {
        CM_DEMO_ADC_LOG("cm_iomux_set_pin_func for CM_ADC_0 err,ret=%d\n", ret);
        return;
    }

    //测试ADC0
    ret = cm_adc_read(CM_ADC_0,&voltage);
    if(ret != RET_SUCCESS)
    {
        CM_DEMO_ADC_LOG("adcCM_ADC_0 read err,ret=%d\n", ret);
        return;
    }
    CM_DEMO_ADC_LOG("adc CM_ADC_0 read:%ld(mv)!!\n",voltage);
    
    // 在读取CM_ADC_1之前设置iomux
    ret = cm_iomux_set_pin_func(CM_ADC_1_PIN, CM_IOMUX_FUNC_ADC);
    if(ret != RET_SUCCESS)
    {
        CM_DEMO_ADC_LOG("cm_iomux_set_pin_func for CM_ADC_1 err,ret=%d\n", ret);
        return;
    }

    ret = cm_adc_read(CM_ADC_1,&voltage);
    if(ret != RET_SUCCESS)
    {
        CM_DEMO_ADC_LOG("adcCM_ADC_1 read err,ret=%d\n", ret);
        return;
    }
    CM_DEMO_ADC_LOG("adc CM_ADC_1 read:%ld(mv)!!\n",voltage);

    // 添加温度测试
    CM_DEMO_ADC_LOG("temperature test start!!\n");
    ret = cm_adc_temperature_read(&temperature);
    if(ret != RET_SUCCESS)
    {
        CM_DEMO_ADC_LOG("chip temperature read err,ret=%d\n", ret);
        return;
    }
    CM_DEMO_ADC_LOG("chip temperature read:%ld(°C)!!\n", temperature);
    
    CM_DEMO_ADC_LOG("adc test end!!\n");
}