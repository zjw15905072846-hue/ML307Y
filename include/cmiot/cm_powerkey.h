/**
 * @file        cm_powerkey.h
 * @brief       POWERKEY接口
 * @copyright   Copyright © 2021 China Mobile IOT. All rights reserved.
 * @author      By zyf
 * @date        2021/04/18
 *
 * @defgroup powerkey powerkey
 * @ingroup POWERKEY
 * @{
 */
/**********************************************************************************
 ***********************POWERKEY接口使用注意事项*************************************
 * 1、回调函数中不可执行耗时任务和其它重进入函数，比如打印等;
 ***************************************************************************/

#ifndef __CM_POWERKEY_H__
#define __CM_POWERKEY_H__


/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Types
 ****************************************************************************/
 typedef enum
{
    CM_POWERKEY_EVENT_RELEASE = 0,         /*!<按键被释放*/
    CM_POWERKEY_EVENT_PRESS = 1,           /*!<按键被按下*/
} cm_powerkey_event_e;
    
typedef void (*cm_pm_powerkey_cb_t)(cm_powerkey_event_e event, void *param);//回调函数中不可执行耗时任务和其它重进入函数

/****************************************************************************
 * Public Data
 ****************************************************************************/
extern cm_pm_powerkey_cb_t cm_powerkey_callback;
extern void * cm_powerkey_param;
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
 * @brief powerkey按键回调函数注册
 *  
 * @param [in]  callback回调函数指针
 * @return
 *  = 0  - 成功 \n
 *  < 0  - 失败, 返回值为错误码
 *  
 * @details 如果回调函数非空，无论长短按都将调用回调函数，且不会关机，仅做按键功能，如果为空，将长按为关机功能\n
 */
int32_t cm_pm_powerkey_callback(cm_pm_powerkey_cb_t callback, void *param);

/**
 * @brief powerkey 控制函数
 *  
 * @param [in]  enable:0，中断失能；1，中断使能
 * @return
 *  = 0  - 成功 \n
 *  < 0  - 失败, 返回值为错误码
 *  
 * @details n
 */
int32_t cm_pm_powerkey_control(bool enable);

#undef EXTERN
#ifdef __cplusplus
}
#endif


#endif /* __CM_POWERKEY_H__ */

/** @}*/
