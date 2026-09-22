/**
 * @file        cm_audio_player.h
 * @brief       Audio player接口(ML307Y Audio 默认不支持，需定制开发)
 * @copyright   Copyright © 2021 China Mobile IOT. All rights reserved.
 * @author      By cmiot3000
 * @date        2026/1/14
 *
 * @defgroup player player
 * @ingroup AUDIO
 * @{
 */

#ifndef __CM_AUDIO_PLAYER_H__
#define __CM_AUDIO_PLAYER_H__

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include "cm_audio_common.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/** 流式播放内部fifo缓冲区大小（字节），128kbps的音频流可缓存约2s数据 */
#define CM_AUDIO_PLAY_STREAM_BUFFER_SIZE         (32*1024)

/** 播放流水位线事件 */
typedef enum
{
    CM_AUDIO_PLAY_STREAM_EVENT_LOW_WATER,   /*!< fifo 数据量低于低水位线，上层可继续写入 */
    CM_AUDIO_PLAY_STREAM_EVENT_HIGH_WATER,  /*!< fifo 数据量高于高水位线，上层应暂缓写入 */
    CM_AUDIO_PLAY_STREAM_EVENT_EMPTY,       /*!< fifo 为空，解码线程即将underrun */
    CM_AUDIO_PLAY_STREAM_EVENT_MAX,         /*!< 哨兵值，初始化使用 */
} cm_audio_play_stream_event_t;

/** 播放流水位线回调函数 */
typedef int (* cm_audio_play_stream_cb_t)(cm_audio_play_stream_event_t event, uint32_t fifo_len);

/****************************************************************************
 * Public Types
 ****************************************************************************/

/** 音频播放通道支持 */
typedef enum
{
    CM_AUDIO_PLAY_CHANNEL_RECEIVER = 1, /*!< 听筒通道 */
    CM_AUDIO_PLAY_CHANNEL_HEADSET,      /*!< 耳机通道 */
    CM_AUDIO_PLAY_CHANNEL_SPEAKER,      /*!< 扬声器通道 */
    CM_AUDIO_PLAY_CHANNEL_REMOTE,       /*!< 远端播放（需建立通话） */
} cm_audio_play_channel_e;

/** 音频播放回调的事件类型 */
typedef enum
{
    CM_AUDIO_PLAY_EVENT_FINISHED = 1, /*!< 播放结束 */
    CM_AUDIO_PLAY_EVENT_INTERRUPT,    /*!< 播放中断 */
    CM_AUDIO_PLAY_EVENT_XXX,          /*!< 预留，以芯片实际支持情况为准 */
} cm_audio_play_event_e;

/** 音频播放设置类型 */
typedef enum
{
    CM_AUDIO_PLAY_CFG_CHANNEL = 1, /*!< 播放通道，支持范围参见cm_audio_play_channel_e枚举量 */
    CM_AUDIO_PLAY_CFG_VOLUME,      /*!< 播放音量，支持范围参见0~100 */
} cm_audio_play_cfg_type_e;

/**
 *   流播放事件枚举
*/
typedef enum
{
    CM_AUDIO_XXX_XXX_XXX1 = 0,              /*!< 预留，以芯片实际支持情况为准 */
    CM_AUDIO_XXX_XXX_XXX2,                  /*!< 预留，以芯片实际支持情况为准 */
} cm_audio_player_stream_event_e;

/** 外置 Codec I2S Pin 脚配置 */
typedef struct
{
    uint32_t sclk_pin;   /*!< I2S SCLK 引脚号 */
    uint32_t lrck_pin;   /*!< I2S LRCK 引脚号 */
    uint32_t sdout_pin;  /*!< I2S SDOUT 引脚号 */
    uint32_t sdin_pin;   /*!< I2S SDIN 引脚号 */
} cm_audio_external_codec_pin_cfg_t;

/** 外置 Codec I2S 硬件配置 */
typedef struct
{
    uint32_t csp_id;       /*!< CSP 外设选择: 0=CSP0, 1=CSP1, 2=CSP2, 3=CSP3 */
    uint32_t samp_bits;    /*!< 采样位宽: 16, 24 */
    uint32_t data_width;   /*!< 数据宽度: 16, 32 */
    uint32_t i2s_format;   /*!< I2S 格式: 0=标准, 1=LSBJ, 2=MSBJ */
    uint32_t trx_mode;     /*!< 收发模式: 0=TX, 1=RX, 2=TRX */
    cm_audio_external_codec_pin_cfg_t pins;  /*!< I2S Pin 脚配置 */
} cm_audio_external_codec_i2s_cfg_t;

/** 外置 Codec 开关控制回调 */
typedef void (*cm_audio_external_codec_control_cb_t)(bool open);

/** 外置 Codec 音量设置回调 */
typedef void (*cm_audio_external_codec_vol_set_cb_t)(uint8_t volume, uint8_t set_reg);

/** 外置 Codec 音量获取回调 */
typedef void (*cm_audio_external_codec_vol_get_cb_t)(uint8_t *volume);

/** 外置 Codec 上电回调 */
typedef void (*cm_audio_external_codec_poweron_cb_t)(void);

/** 外置 Codec 下电回调 */
typedef void (*cm_audio_external_codec_poweroff_cb_t)(void);

/** 外置 Codec 注册配置 */
typedef struct
{
    cm_audio_external_codec_i2s_cfg_t i2s_cfg;              /*!< I2S 硬件配置 */
    cm_audio_external_codec_control_cb_t control_cb;         /*!< Codec 开关回调 */
    cm_audio_external_codec_vol_set_cb_t vol_set_cb;         /*!< 音量设置回调 */
    cm_audio_external_codec_vol_get_cb_t vol_get_cb;         /*!< 音量获取回调 */
    cm_audio_external_codec_poweron_cb_t poweron_cb;         /*!< Codec 上电回调 */
    cm_audio_external_codec_poweroff_cb_t poweroff_cb;       /*!< Codec 下电回调 */
} cm_audio_external_codec_cfg_t;


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

/****************************************************************************/

/**
*  @brief 流播放状态回调（仅适用于ASR平台芯片）
*
*  @param [in] event 事件
*
*/
typedef void (*cm_audio_player_stream_cb)(cm_audio_player_stream_event_e event);

/**
 * @brief 播放回调函数
 *
 * @param [in] event 事件类型
 * @param [in] param  事件参数
 *
 * @details  须在播放API中传入
 */
typedef void (*cm_audio_play_callback_t)(cm_audio_play_event_e event, void *param);

/**
 * @brief 设置播放参数
 *
 * @param [in] type  设置参数类型
 * @param [in] value 设置参数数值
 *
 * @return
 *   =  0 - 成功 \n
 *   = -1 - 失败
 */
int32_t cm_audio_play_set_cfg(cm_audio_play_cfg_type_e type, void *value);

/**
 * @brief 读取播放参数
 *
 * @param [in]  type  读取参数类型
 * @param [out] value 读取参数数值
 *
 * @return
 *   =  0 - 成功 \n
 *   = -1 - 失败
 */
int32_t cm_audio_play_get_cfg(cm_audio_play_cfg_type_e type, void *value);

/**
 *  @brief 从内存中播放音频数据
 *
 *  @param [in] data         播放音频数据
 *  @param [in] size         播放音频数据长度
 *  @param [in] format       播放音频格式
 *  @param [in] sample_param 播放音频PCM采样参数（format参数为CM_AUDIO_PLAY_FORMAT_PCM使用，其余情况传入NULL）
 *  @param [in] cb           音频播放回调函数（回调函数在音频处理线程中被执行）
 *  @param [in] cb_param     用户参数（与cm_audio_play_callback回调函数中param参数相对应）
 *
 *  @return
 *   =  0 - 成功 \n
 *   = -1 - 失败
 *
 *  @details 在播放器启动之前，整个流应该位于内存中
 */
int32_t cm_audio_play(const void *data, uint32_t size, cm_audio_play_format_e format, cm_audio_sample_param_t *sample_param, cm_audio_play_callback_t cb, void *cb_param);

/**
 *  @brief 从文件系统播放音频文件
 *
 *  @param [in] path         文件路径/名称
 *  @param [in] format       播放格式
 *  @param [in] sample_param 播放音频PCM采样参数（format参数为CM_AUDIO_PLAY_FORMAT_PCM使用，其余情况传入NULL）
 *  @param [in] cb           音频播放回调函数（回调函数在音频处理线程中被执行）
 *  @param [in] cb_param     用户参数（与cm_audio_play_callback回调函数中param参数相对应）
 *
 *  @return
 *   =  0 - 成功 \n
 *   = -1 - 失败
 */
int32_t cm_audio_play_file(const char *path, cm_audio_play_format_e format, cm_audio_sample_param_t *sample_param, cm_audio_play_callback_t cb, void *cb_param);

/**
 *  @brief 暂停播放
 *
 *  @return
 *   =  0 - 成功 \n
 *   = -1 - 失败
 */
int32_t cm_audio_player_pause(void);

/**
 *  @brief 继续播放
 *
 *  @return
 *   =  0 - 成功 \n
 *   = -1 - 失败
 */
int32_t cm_audio_player_resume(void);

/**
 *  @brief 停止播放
 *
 *  @return
 *   =  0 - 成功 \n
 *   = -1 - 失败
 */
int32_t cm_audio_player_stop(void);

 /**
 *  @brief 注册流播放状态回调
 *
 *  @return void
 *
 *  @details 
 */
void cm_audio_player_stream_cb_reg(cm_audio_player_stream_cb cb);

/**
 *  @brief 从管道/消息队列中播放音频（开启）
 *
 *  @param [in] format       播放格式
 *  @param [in] sample_param 播放音频PCM采样参数（format参数为CM_AUDIO_PLAY_FORMAT_PCM使用，其余情况传入NULL）
 *
 *  @return
 *   =  0 - 成功 \n
 *   = -1 - 失败
 *
 *  @details 
 */
int32_t cm_audio_player_stream_open(cm_audio_play_format_e format, cm_audio_sample_param_t *sample_param);

/**
 *  @brief 往管道/消息队列中发送要播放的音频数据
 *
 *  @param [in] data 播放的数据
 *  @param [in] size 播放数据的长度
 *
 *  @return
 *   >= 0 - 实际写入的数据长度 \n
 *   = -1 - 失败
 *
 *  @details 
 */
int32_t cm_audio_player_stream_push(uint8_t *data, uint32_t size);

/**
 *  @brief 从管道/消息队列中播放音频（关闭）
 *
 *  @details More details
 */
void cm_audio_player_stream_close(void);

/**
 *  @brief 设置播放流水位线回调，fifo数据量达到水位线时通过回调通知上层，用于控制写入节奏避免fifo溢出或underrun
 *
 *  @param [in] cb 水位线回调函数，回调中不可阻塞，不可做耗时操作
 *                 回调参数 event 取值：
 *                   - CM_AUDIO_PLAY_STREAM_EVENT_HIGH_WATER  fifo数据量 >= high_water，上层应暂缓写入
 *                   - CM_AUDIO_PLAY_STREAM_EVENT_LOW_WATER   fifo数据量 <= low_water，上层可继续写入
 *                   - CM_AUDIO_PLAY_STREAM_EVENT_EMPTY       fifo为空，解码线程即将underrun，需尽快写入数据
 *                 回调参数 fifo_len：当前fifo中的数据量（字节）
 *  @param [in] low_water 低水位线阈值（字节），例设为 CM_AUDIO_PLAY_STREAM_BUFFER_SIZE / 4
 *  @param [in] high_water 高水位线阈值（字节），例设为 CM_AUDIO_PLAY_STREAM_BUFFER_SIZE * 3 / 4
 *
 *  @note
 *  - 需在 cm_audio_player_stream_open 调用之后、首次 cm_audio_player_stream_push 之前调用
 *  - 回调在写入线程（HIGH_WATER）或解码线程（LOW_WATER/EMPTY）中执行，不可阻塞
 *  - 每个事件仅触发一次，fifo回到正常水位区间后自动重置，可再次触发
 *  - 未调用此接口注册回调时，水位线检测不生效，行为与修改前完全一致
 *  - 播放流结束时（fifo数据全部消费完毕），会触发 EMPTY 事件通知上层
 */
void cm_audio_player_stream_set_watermark_cb(cm_audio_play_stream_cb_t cb, uint32_t low_water, uint32_t high_water);

/**
 * @brief 注册外置 Codec 驱动配置
 *
 * @param [in] cfg 外置 Codec 配置（含 I2S 硬件参数和回调函数）
 *
 * @return
 *   =  0 - 成功 \n
 *   = -1 - 失败
 *
 * @note 须在首次使用音频播放/录音前调用
 *       未注册时各 device 接口函数安全返回，不执行实际操作
 */
int32_t cm_audio_external_codec_register(const cm_audio_external_codec_cfg_t *cfg);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __CM_AUDIO_PLAYER_H__ */

/** @}*/
