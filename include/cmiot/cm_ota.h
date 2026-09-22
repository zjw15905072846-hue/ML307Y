/**
 * @file        cm_ota.h
 * @brief       通用ota接口
 * @copyright   Copyright © 2021 China Mobile IOT. All rights reserved.
 * @author      By WangPeng
 * @date        2021/5/14
 *
 * @defgroup ota ota
 * @ingroup OTA
 * @{
 */

#ifndef __CM_OTA_H__
#define __CM_OTA_H__

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

/****************************************************************************

/**
 *  @brief 查询用于升级的文件系统或flash空间大小
 *  
 *  @return 
 *   >= 0 - 空间大小 \n
 *   <  0 - 失败
 *  
 */
int32_t cm_ota_get_capacity(void);

/**
 *  @brief 通用OTA升级初始化
 *  
 *  @return 
 *   =  0 - 成功 \n
 *   <  0 - 失败
 *  
 */
int32_t cm_ota_init(void);

/**
 *  @brief 通用OTA升级，保存升级包至文件系统或flash
 *  
 *  @param [in] buffer  下载固件包数据
 *  @param [in] size   	下载固件包长度
 *  @return 
 *   =  0 - 成功 \n
 *   <  0 - 失败
 *  
 */
int32_t cm_ota_firmware_write(const char *buffer, size_t size);

/**
 *  @brief 查询已写入文件系统或flash中的升级包大小
 *  
 *  @return 
 *   >=  0 - 返回已写入文件系统或flash中的升级包大小 \n
 *  
 */
int32_t cm_ota_get_written_size(void);

/**
 *  @brief 擦除已写入文件系统或flash中的升级包
 *  
 *  @return 
 *   =  0 - 成功 \n
 *   <  0 - 失败
 *  
 */
int32_t cm_ota_firmware_erase(void);

/**
 *  @brief 触发升级
 *  
 *  @return 
 *   =  0 - 成功 \n
 *   <  0 - 失败
 *  
 *  @details 须在写入升级包操作后执行
 */
int32_t cm_ota_upgrade(void);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __CM_OTA_H__ */

/** @}*/
