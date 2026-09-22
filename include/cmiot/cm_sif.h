/**
 * @file        cm_sif.h
 * @brief       sif协议功能接口头文件
 * @copyright   Copyright © 2024 China Mobile IOT. All rights reserved.
 * @date        2024/9/25
 */
/***********************SIF使用注意事项*************************************
 * 此为数据标准格式，其时间和类型在cm_sif_cfg_t中可以设置
// DATA (0): |_______1ms低电平_____|______0.5ms高电平_____|
// DATA (1): |______0.5ms低电平____|_______1ms高电平______|
 * 不支持TX RX回环测试，即TX和RX不能连接在一起进行测试
 * start_level和stop_level只支持设置为相同电平
 * 目前设置的最大发送和接收长度为64字节
***************************************************************************/

    
#ifndef __CM_SIF_H__
#define __CM_SIF_H__


/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <stdint.h>
#include "cm_gpio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/


/****************************************************************************
 * Public Types
 ****************************************************************************/
/**
 * @brief SIF 支持路数枚举
 */
typedef enum {
    CM_SIF_ID_0,
    CM_SIF_ID_1,
    CM_SIF_ID_2,
    CM_SIF_ID_MAX
} cm_sif_id_e;

/**
 * @brief SIF 每个字节顺序发送/接收位顺序枚举
 */
typedef enum {
    CM_SIF_BIT_ORDER_MBS = 0,        /*!< 最高位优先(MSB) */
    CM_SIF_BIT_ORDER_LBS = 1         /*!< 最低位优先(LSB) */
} cm_sif_bit_order_type_e;

/**
 * @brief SIF电平类型定义
 */
typedef enum {
    CM_SIF_LEVEL_LOW = 0,         /*!< 低电平 */
    CM_SIF_LEVEL_HIGH = 1         /*!< 高电平 */
} cm_sif_level_type_e;

/**
 * @brief SIF电平翻转类型定义
 */
typedef enum {
    CM_SIF_LEVEL_NORMAL = 0,      /*!< 默认电平 */
    CM_SIF_LEVEL_INVERT = 1       /*!< 电平翻转 */
} cm_sif_level_invert_type_e;

/**
 * @brief SIF数据位翻转类型定义
 */
typedef enum {
    CM_SIF_DATA_NORMAL = 0,       /*!< 正常数据 */
    CM_SIF_DATA_INVERT = 1        /*!< 数据位翻转 */
} cm_sif_data_invert_type_e;

/**
 *  @brief sif接收数据回调函数
 *  
 *  @param [out] sif_id sif id
 *  @param [out] data 待读数据缓存
 *  @param [out] len  长度
 *  
 *  \details 回调函数中仅可保存数据，不可执行其他业务
 */
typedef void (*cm_sif_recv_cb)(cm_sif_id_e sif_id, uint8_t *data, uint32_t len);

/**
 * @brief SIF配置结构体
 */
typedef struct{
    cm_gpio_num_e tx_gpio;                      /*!< 用于发送数据的gpio num                    */
    cm_gpio_num_e rx_gpio;                      /*!< 用于接收数据的gpio num                    */
    cm_sif_bit_order_type_e bit_order_type;     /*!< 数据位发送接/收顺序，0:MBS(最高位优先)，1:LBS(最低位优先) */
    cm_sif_level_type_e rx_start_level;         /*!< RX初始电平，0：低电平；1：高电平           */
    cm_sif_level_type_e rx_stop_level;          /*!< RX停止电平，0：低电平；1：高电平           */
    cm_sif_level_type_e tx_start_level;         /*!< TX初始电平，0：低电平；1：高电平           */
    cm_sif_level_type_e tx_stop_level;          /*!< TX停止电平，0：低电平；1：高电平           */
    cm_sif_level_invert_type_e rx_invert_level; /*!< RX电平翻转，0：默认电平；1：高低电平全部翻转,配置为1一般用于用户做了反向电路 */
    cm_sif_level_invert_type_e tx_invert_level; /*!< TX电平翻转，0：默认电平；1：高低电平全部翻转,TX一般不配置为1，如果用于测试需和rx_invert_level值相同 */
    cm_sif_data_invert_type_e rx_data_invert;   /*!< 接收数据每一位取反，用于数据位低电平比高电平长是为1，反之为0的数据控制，无特殊要求，默认为0 */
    cm_sif_data_invert_type_e tx_data_invert;   /*!< 发送数据每一位取反，用于数据位低电平比高电平长是为1，反之为0的数据控制，无特殊要求，默认为0 */
    uint32_t sync_level_long_time_us;           /*!< 同步信号中长电平持续时间，单位us          */
    uint32_t sync_level_short_time_us;          /*!< 同步信号中短电平持续时间，单位us          */
    uint32_t logic_short_time_us;               /*!< 一个逻辑信号中较短电平持续时间，单位us     */
    uint32_t logic_long_time_us;                /*!< 一个逻辑信号中较长电平持续时间，单位us     */
    uint32_t stop_level_time_us;                /*!< 停止信号中低电平持续时间，单位us*/
    uint32_t rx_ring_buffer_size;               /*!< 接收数据环形缓冲区大小，单位字节          */
    uint32_t rx_recv_timeout;                   /*!< 接收数据超时时间，单位ms，默认0采用串口阻塞模式接收数据，适用于标准波形；当接收波形不标准时，需要设置为超过一帧数据最大时长即可          */
    cm_sif_recv_cb recv_cb;                     /*!< 用于接收数据的回调函数，不可以处理耗时任务*/
} cm_sif_cfg_t;

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
 *  @brief sif发送
 *  
 *  @param [in] sif_id sif id
 *  @param [in] data 待写入数据
 *  @param [in] len  待写入字节数
 *  
 *  @return 
 *    = 实际写入长度 - 成功 \n
 *    < 0 - 失败, 返回错误码
 *  
 *  @details 
 */
int32_t cm_sif_slave_write(cm_sif_id_e sif_id, const uint8_t *data, uint32_t len);

/**
 * @brief 初始化sif功能
 *
 * @param [in] sif_id sif id
 * @param [in] config 配置参数
 *
 * @return  
 *   = 0  - 成功 \n
 *    < 0 - 失败, 返回错误码
 *
 * @details 配置参数不能为空指针
 */
int32_t cm_sif_slave_init(cm_sif_id_e sif_id, cm_sif_cfg_t *config);

/**
 * @brief sif功能去初始化
 * 
 * @param [in] sif_id sif id
 * 
 * @return  
 *   = 0  - 成功 \n
 *   = 其他 - 失败
 *
 * @details
 */
int32_t cm_sif_slave_deinit(cm_sif_id_e sif_id);

#undef EXTERN
#ifdef __cplusplus
}
#endif
#endif /* __CM_SIF_H__ */