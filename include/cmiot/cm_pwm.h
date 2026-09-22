/**
 * @file        cm_pwm.h
 * @brief       PWM接口
 * @copyright   Copyright © 2021 China Mobile IOT. All rights reserved.
 * @author      By cmiot
 * @date        2021/03/09
 *
 * @defgroup pwm pwm
 * @ingroup PI
 * @{
 */
/****************************************************************************
 ***********************PWM使用注意事项*************************************
 * 1、禁止将多个引脚映射到同PWM设备上（DEV);
 ***************************************************************************/

#ifndef __CM_PWM_H__
#define __CM_PWM_H__

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Types
 ****************************************************************************/

/** 设备ID */
typedef enum{
    CM_PWM_DEV_0,              /*!< 设备1*/
    CM_PWM_DEV_1,              /*!< 设备2*/
    CM_PWM_DEV_NUM
} cm_pwm_dev_e;

/****************************************************************************
 * Public Data
 ****************************************************************************/


/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif
/**
 *  @brief 打开PWM设备
 *  
 *  @param [in] dev PWM设备ID
 *  @param [in] period 周期(ns),有效值:63~5120000
 *  @param [in] period_h 周期高电平占用时间(ns),有效值:0ns,或者32~period
 *  
 *  @return 
 *    = 0 - 成功
 *    < 0 - 失败, 返回值为错误码
*/
int32_t cm_pwm_open_ns(cm_pwm_dev_e dev, uint32_t period, uint32_t period_h);

/**
 *  @brief 关闭PWM设备
 *  
 *  @param [in] dev PWM设备ID
 *  
 *  @return 
 *    = 0 - 成功
 *    < 0 - 失败, 返回值为错误码
 *  
 *  @details 需要在open函数之后使用
 */
int32_t cm_pwm_close(cm_pwm_dev_e dev);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __CM_PWM_H__ */

/** @}*/

