/**
 * @file        cm_audio_recorder.h
 * @brief       Audio recorder接口(ML307Y Audio 默认不支持，需定制开发)
 * @copyright   Copyright © 2021 China Mobile IOT. All rights reserved.
 * @author      By cmiot3000
 * @date        2026/1/14
 *
 * @defgroup recorder recorder
 * @ingroup AUDIO
 * @{
 */

#ifndef __CM_AUDIO_RECORDER_H__
#define __CM_AUDIO_RECORDER_H__

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include "cm_audio_common.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Types
 ****************************************************************************/

/** 录音通道支持 */
typedef enum
{
    CM_AUDIO_RECORD_CHANNEL_MAIN = 1, /*!< 主MIC通道 */
    CM_AUDIO_RECORD_CHANNEL_HP,       /*!< 耳机MIC通道 */
    CM_AUDIO_RECORD_CHANNEL_REMOTE,   /*!< 从远端通话中录音（需建立通话） */
} cm_audio_record_channel_e;

/** 录音回调的事件类型 */
typedef enum
{
    CM_AUDIO_RECORD_EVENT_DATA = 1,  /*!< 录音数据上报 */
    CM_AUDIO_RECORD_EVENT_FINISHED,  /*!< 录音结束 */
    CM_AUDIO_RECORD_EVENT_INTERRUPT, /*!< 录音中断 */
    CM_AUDIO_RECORD_EVENT_XXX,       /*!< 预留，以芯片实际支持情况为准 */
} cm_audio_record_event_e;

/** 录音设置类型 */
typedef enum
{
    CM_AUDIO_RECORD_CFG_CHANNEL = 1, /*!< 录音通道，支持范围参见cm_audio_record_channel_e枚举量 */
    CM_AUDIO_RECORD_CFG_GAIN,        /*!< 录音增益，支持范围参见0~100 */
} cm_audio_record_cfg_type_e;

/*
 * 录音数据结构体
 */
typedef struct
{
    uint8_t *data; /*!< 录音数据 */
    uint32_t len;  /*!< 录音数据长度 */
    void *user;    /*!< 用户传入参数 */
} cm_audio_record_data_t;

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C" {
#else
#define EXTERN extern
#endif

/****************************************************************************

/**
 * @brief 录音回调函数
 *
 * @param [in] event 事件类型
 * @param [in] param 事件参数（事件类型为CM_AUDIO_RECORD_DATA时，需要将param强转为cm_audio_record_data_t型。其余情况该参数为用户传入参数）
 *
 * @details  须在录音API中传入
 */
typedef void (*cm_audio_record_callback_t)(cm_audio_record_event_e event, void *param);

/**
 * @brief 设置录音参数
 *
 * @param [in] type  设置参数类型
 * @param [in] value 设置参数数值
 *
 * @return
 *   =  0 - 成功 \n
 *   = -1 - 失败
 */
int32_t cm_audio_record_set_cfg(cm_audio_record_cfg_type_e type, void *value);

/**
 * @brief 读取录音参数
 *
 * @param [in]  type  读取参数类型
 * @param [out] value 读取参数数值
 *
 * @return
 *   =  0 - 成功 \n
 *   = -1 - 失败
 */
int32_t cm_audio_record_get_cfg(cm_audio_record_cfg_type_e type, void *value);

/**
 *  @brief 开始录音
 *
 *  @param [in] format       录制音频格式
 *  @param [in] sample_param 录制音频PCM采样参数
 *  @param [in] cb           录音回调函数（回调函数不能阻塞）
 *  @param [in] cb_param     用户参数（参见cm_audio_record_callback回调函数中param参数描述）
 *
 *  @return
 *   =  0 - 成功 \n
 *   = -1 - 失败
 *
 *  @details 回调函数不能阻塞
 */
int32_t cm_audio_recorder_start(cm_audio_record_format_e format, cm_audio_sample_param_t *sample_param, cm_audio_record_callback_t cb, void *cb_param);

/**
 *  @brief 结束录音
 */
void cm_audio_recorder_stop(void);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __CM_AUDIO_RECORDER_H__ */

/** @}*/
