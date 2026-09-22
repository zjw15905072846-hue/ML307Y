/**
 * @file        cm_audio_common.h
 * @brief       Audio 通用接口(ML307Y Audio 默认不支持，需定制开发)
 * @copyright   Copyright © 2021 China Mobile IOT. All rights reserved.
 * @author      By cmiot3000
 * @date        2026/1/14
 *
 * @defgroup audio_common common
 * @ingroup AUDIO
 * @{
 */

#ifndef __CM_AUDIO_COMMON_H__
#define __CM_AUDIO_COMMON_H__

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

/** 音频播放格式支持         */
typedef enum
{
    CM_AUDIO_PLAY_FORMAT_PCM = 1, /*!< PCM格式 */
    CM_AUDIO_PLAY_FORMAT_WAV,     /*!< WAV格式 */
    CM_AUDIO_PLAY_FORMAT_MP3,     /*!< MP3格式 */
    CM_AUDIO_PLAY_FORMAT_AMRNB,   /*!< AMR-NB格式 */
    CM_AUDIO_PLAY_FORMAT_AMRWB,   /*!< AMR-WB格式 */
    CM_AUDIO_PLAY_FORMAT_FR,      /*!< 预留，以芯片实际支持情况为准 */
    CM_AUDIO_PLAY_FORMAT_HR,      /*!< 预留，以芯片实际支持情况为准 */
    CM_AUDIO_PLAY_FORMAT_EFR,     /*!< 预留，以芯片实际支持情况为准 */
    CM_AUDIO_PLAY_FORMAT_AAC,     /*!< 预留，以芯片实际支持情况为准 */
    CM_AUDIO_PLAY_FORMAT_MID,     /*!< 预留，以芯片实际支持情况为准 */
    CM_AUDIO_PLAY_FORMAT_QTY,     /*!< 预留，以芯片实际支持情况为准 */
    CM_AUDIO_PLAY_FORMAT_XXX,     /*!< 预留，以芯片实际支持情况为准 */
} cm_audio_play_format_e;

/** 音频录音格式支持 */
typedef enum
{
    CM_AUDIO_RECORD_FORMAT_PCM = 1,    /*!< PCM格式 */
    CM_AUDIO_RECORD_FORMAT_WAVPCM,     /*!< WAV格式 */
    CM_AUDIO_RECORD_FORMAT_MP3,        /*!< MP3格式 */
    CM_AUDIO_RECORD_FORMAT_AMRNB_475,  /*!< 4.75 kbps AMR格式 */
    CM_AUDIO_RECORD_FORMAT_AMRNB_515,  /*!< 5.15 kbps AMR格式 */
    CM_AUDIO_RECORD_FORMAT_AMRNB_590,  /*!< 5.90 kbps AMR格式 */
    CM_AUDIO_RECORD_FORMAT_AMRNB_670,  /*!< 6.70 kbps AMR格式 */
    CM_AUDIO_RECORD_FORMAT_AMRNB_740,  /*!< 7.40 kbps AMR格式 */
    CM_AUDIO_RECORD_FORMAT_AMRNB_795,  /*!< 7.95 kbps AMR格式 */
    CM_AUDIO_RECORD_FORMAT_AMRNB_1020, /*!< 10.20 kbps AMR格式 */
    CM_AUDIO_RECORD_FORMAT_AMRNB_1220, /*!< 12.20 kbps AMR格式 */
    CM_AUDIO_RECORD_FORMAT_AMRWB_660,  /*!< 6.60 kbps AWB格式 */
    CM_AUDIO_RECORD_FORMAT_AMRWB_885,  /*!< 8.85 kbps AWB格式 */
    CM_AUDIO_RECORD_FORMAT_AMRWB_1265, /*!< 12.65 kbps AWB格式 */
    CM_AUDIO_RECORD_FORMAT_AMRWB_1425, /*!< 14.25 kbps AWB格式 */
    CM_AUDIO_RECORD_FORMAT_AMRWB_1585, /*!< 15.85 kbps AWB格式 */
    CM_AUDIO_RECORD_FORMAT_AMRWB_1825, /*!< 18.25 kbps AWB格式 */
    CM_AUDIO_RECORD_FORMAT_AMRWB_1985, /*!< 19.85 kbps AWB格式 */
    CM_AUDIO_RECORD_FORMAT_AMRWB_2305, /*!< 23.05 kbps AWB格式 */
    CM_AUDIO_RECORD_FORMAT_AMRWB_2385, /*!< 23.85 kbps AWB格式 */
    CM_AUDIO_RECORD_FORMAT_XXX,        /*!< 预留，以芯片实际支持情况为准 */
} cm_audio_record_format_e;

/** PCM音频采样格式 */
typedef enum
{
    CM_AUDIO_SAMPLE_FORMAT_8BIT = 1, /*!< 8位 */
    CM_AUDIO_SAMPLE_FORMAT_16BIT,    /*!< 16位, 大小开端由芯片平台决定，每个OC SDK单独备注 */
    CM_AUDIO_SAMPLE_FORMAT_24BIT,    /*!< 24位, 大小开端由芯片平台决定，每个OC SDK单独备注 */
    CM_AUDIO_SAMPLE_FORMAT_32BIT,    /*!< 32位, 大小开端由芯片平台决定，每个OC SDK单独备注 */
} cm_audio_sample_format_e;

/** 音频播放支持的采样率（录音支持的采样率单独标注） */
typedef enum
{
    CM_AUDIO_SAMPLE_RATE_8000HZ = 8000,
    CM_AUDIO_SAMPLE_RATE_9600HZ = 9600,
    CM_AUDIO_SAMPLE_RATE_11025HZ = 11025,
    CM_AUDIO_SAMPLE_RATE_12000HZ = 12000,
    CM_AUDIO_SAMPLE_RATE_16000HZ = 16000,
    CM_AUDIO_SAMPLE_RATE_22050HZ = 22050,
    CM_AUDIO_SAMPLE_RATE_24000HZ = 24000,
    CM_AUDIO_SAMPLE_RATE_32000HZ = 32000,
    CM_AUDIO_SAMPLE_RATE_44100HZ = 44100,
    CM_AUDIO_SAMPLE_RATE_48000HZ = 48000,
    CM_AUDIO_SAMPLE_RATE_96000HZ = 96000,
    CM_AUDIO_SAMPLE_RATE_128000HZ = 128000,
} cm_audio_sample_rate_e;

/** PCM音频采样通道 */
typedef enum
{
    CM_AUDIO_SOUND_MONO = 1,   /*!< （默认）单通道 */
    CM_AUDIO_SOUND_STEREO = 2, /*!< 双通道（立体声） */
} cm_audio_sound_channel_e;

/** 音频采样参数结构体 */
typedef struct
{
    cm_audio_sample_format_e sample_format; /*!< 采样格式（PCM） */
    cm_audio_sample_rate_e rate;            /*!< 采样率（PCM） */
    cm_audio_sound_channel_e num_channels;  /*!< 采样声道（PCM） */
} cm_audio_sample_param_t;

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __CM_AUDIO_COMMON_H__ */

/** @}*/
