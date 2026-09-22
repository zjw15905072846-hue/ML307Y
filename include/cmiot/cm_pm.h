/**
 * @file        cm_pm.h
 * @brief       PM接口
 * @copyright   Copyright © 2021 China Mobile IOT. All rights reserved.
 * @author      By cmoit
 * @date        2021/04/18
 *
 * @defgroup pm pm
 * @ingroup PM
 * @{
 */

#ifndef __CM_PM_H__
#define __CM_PM_H__

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

/** 上电原因 */
typedef enum
{
    CM_PM_POWER_ON = 0,     /**正常上电 */
    CM_PM_PIN_RESET,        /**PIN按键复位 */
    CM_PM_SOFT_RESET,       /**软件复位 */
    CM_PM_UTC_WAKEUP,       /**UTC超时唤醒 */
    CM_PM_EXTPIN_WAKEUP,    /**WAKEUP-PIN唤醒 */
    CM_PM_WDT_RESET,        /**硬件看门狗复位 */
    CM_PM_UNKNOWN_ON,       /**未知 */
}cm_pm_power_on_reason_e;

/** 协议栈低功耗状态，不支持 */
typedef enum
{
    CM_PM_PS_STATUS_CONNECT = 0,  /**连接态 */
    CM_PM_PS_STATUS_IDLE,     /**空闲态 */
    CM_PM_PS_STATUS_PSM,      /**PSM */

    CM_PM_PS_STATUS_MAX,
}cm_pm_ps_status_e;

/** 模组低功耗状态 */
typedef enum
{
    CM_PM_SLEEP_STATUS_ACTIVE = 0,    /**ACTIVE */
    CM_PM_SLEEP_STATUS_LIGHT,     /**浅睡眠，不支持*/
    CM_PM_SLEEP_STATUS_DEEP,      /**深睡眠 */

    CM_PM_SLEEP_STATUS_MAX,
}cm_pm_sleep_status_e;

/** 模组低功耗模式 */
typedef enum
{
    CM_PM_SLEEP_MODE_ACTIVE = 0,    /**关闭休眠模式 */
    CM_PM_SLEEP_MODE_LIGHT,     /**允许入浅睡眠 */
    CM_PM_SLEEP_MODE_DEEP,      /**允许入浅睡眠和深睡眠 */

    CM_PM_SLEEP_MODE_MAX,
}cm_pm_sleep_mode_e;

/** 协议栈低功耗回调，不支持 */
typedef void (*cm_pm_psind_cb)(char status);

/** 模组低功耗回调 */
typedef void (*cm_pm_sleepind_cb)(char status, int32_t source);

/** 低功耗模式配置结构体 */
typedef struct
{
    int mode;           /**休眠模式 */
    bool permanent;     /**是否保存至flash,设置时有效，获取时忽略 */
}cm_pm_sleep_mode_t;

/** 低功耗管理配置选项 */
typedef enum
{
    CM_PM_CFG_PSIND,        /**设置协议栈低功耗上报 */
    CM_PM_CFG_SLEEPIND,     /**设置模组低功耗上报，对应cm_pm_sleepind_cb */
    CM_PM_CFG_SLEEPMODE,    /**设置模组低功耗模式，对应cm_pm_sleep_mode_t */
    CM_PM_CFG_DELAYSLEEP,   /**设置延时休眠时间，不支持*/
    CM_PM_CFG_DTR,          /**设置DTR控制休眠，不支持 */

    CM_PM_CFG_MAX,
}cm_pm_cfg_type_e;

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
 * @brief 关机
 *  
 * @return 空
 *  
 * @details More details
 */
void cm_pm_poweroff(void);

/**
 * @brief 模组重启
 * 
 * @param [in] type 重启类型代码，请参考模组电源管理文档。ML307Y无实际效果。
 * 
 * @return 空
 *  
 * @details More details
 */
void cm_pm_reboot(uint8_t type);

/**
 * @brief 获取上电原因
 * 
 * @return 
 *   >= 0  - 上电原因代码，请参考模组电源管理文档
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
cm_pm_power_on_reason_e cm_pm_get_power_on_reason(void);

/**
 * @brief 上锁深睡眠模式
 *  
 * @return 空
 *  
 * @details 执行后，模组将无法进入深睡眠状态
 */
void cm_pm_work_lock(void);

/**
 * @brief 解锁深睡眠模式
 *  
 * @return 空
 *  
 * @details 执行后，模组将允许进入深睡眠状态
 */
void cm_pm_work_unlock(void);

/**
 * @brief 低功耗管理配置
 * 
 * @param [in] type 配置类型，详见cm_pm_cfg_type_e
 * @param [in] info 配置信息
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details info需传入对应类型结构体
 */
int cm_pm_set_cfg(int type, void *info);

/**
 * @brief 获取低功耗管理配置
 * 
 * @param [in] type 配置类型，详见cm_pm_cfg_type_e
 * @param [out] info 信息缓存指针
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details info需传入对应类型结构体指针
 */
int cm_pm_get_cfg(int type, void *info);


#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __CM_PM_H__ */

/** @}*/
