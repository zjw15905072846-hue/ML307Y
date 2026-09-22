/**
 * @file        cm_uart.h
 * @brief       UART接口
 * @copyright   Copyright © 2021 China Mobile IOT. All rights reserved.
 * @author      By cmiot1325
 * @date        2021/03/09
 *
 * @defgroup uart uart
 * @ingroup PI
 * @{
 */

#ifndef __CM_UART_H__
#define __CM_UART_H__

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/** 标准波特率 */
#define    CM_UART_BAUDRATE_1200        (1200U)    /*仅设备2支持*/
#define    CM_UART_BAUDRATE_2400        (2400U)    /*仅设备2支持*/
#define    CM_UART_BAUDRATE_4800        (4800U)
#define    CM_UART_BAUDRATE_9600        (9600U)
#define    CM_UART_BAUDRATE_14400       (14400U)
#define    CM_UART_BAUDRATE_19200       (19200U)
#define    CM_UART_BAUDRATE_28800       (28800U)
#define    CM_UART_BAUDRATE_38400       (38400U)
#define    CM_UART_BAUDRATE_57600       (57600U)
#define    CM_UART_BAUDRATE_76800       (76800U)
#define    CM_UART_BAUDRATE_115200      (115200U)
#define    CM_UART_BAUDRATE_230400      (230400U)
#define    CM_UART_BAUDRATE_460800      (460800U)
#define    CM_UART_BAUDRATE_921600      (921600U)

/****************************************************************************
 * Public Types
 ****************************************************************************/

 /** 设备ID ，详情参照资源综述*/
 /* FOTA升级期间，会有数据从主串口吐出（对应为UART0），用户无需关注该数据，也不应处理该数据 */
 /* 增加UART2，使用PIN38 PIN39 */
typedef enum{
    CM_UART_DEV_0,              /*!< 设备0*/
    CM_UART_DEV_1,              /*!< 设备1*/
    CM_UART_DEV_2,              /*!< 设备2*/
    CM_UART_DEV_NUM
} cm_uart_dev_e;

/** 数据位 */
typedef enum{
    CM_UART_BYTE_SIZE_8 = 8,
    CM_UART_BYTE_SIZE_7 = 7,
    CM_UART_BYTE_SIZE_6 = 6,
    CM_UART_BYTE_SIZE_5 = 5,    /*!< 不支持*/
} cm_uart_byte_size_e;

/** 奇偶校验 */
typedef enum{
    CM_UART_PARITY_NONE,
    CM_UART_PARITY_ODD,
    CM_UART_PARITY_EVEN,
    CM_UART_PARITY_MARK,    /*!< 不支持*/
    CM_UART_PARITY_SPACE    /*!< 不支持*/
} cm_uart_parity_e;

/** 停止位 */
typedef enum{
    CM_UART_STOP_BIT_ONE,
    CM_UART_STOP_BIT_ONE_HALF,
    CM_UART_STOP_BIT_TWO
} cm_uart_stop_bit_e;

/** 流控制 */
/* 
 * 开启流控后，当缓存区内数据超过缓存长度将拉高RTS(PIN22)，此时若注册了流控事件将上报流控事件
 * 通过读取缓存区数据直到低于缓存长度才会重新拉低RTS(PIN22)
 */
typedef enum{
    CM_UART_FLOW_CTRL_NONE,
    CM_UART_FLOW_CTRL_HW,   /*!< 使能流控，仅主串口支持*/
} cm_uart_flow_ctrl_e;

/** 结果码 */
typedef enum{
    CM_UART_RET_OK = 0,
    CM_UART_RET_INVALID_PARAM = -1,
    CM_UART_RET_CTS_HIGH = -2,
} cm_uart_ret_e;

/** 配置 */
typedef struct{
    cm_uart_byte_size_e byte_size;      /*!< 数据位，枚举*/
    cm_uart_parity_e parity;            /*!< 校验位，枚举*/
    cm_uart_stop_bit_e stop_bit;        /*!< 停止位，枚举*/
    cm_uart_flow_ctrl_e flow_ctrl;      /*!< 流控制，枚举*/
    uint32_t baudrate;                  /*!< 波特率，支持的波特率见本文件中标准波特率宏定义和特殊波特率宏定义*/
    uint32_t is_lpuart;                 /*!< 无效参数，固定dev0为低功耗串口支持115200及以下的波特率sleep唤醒不丢数据，大于115200波特率时唤醒的数据会出现乱码，唤醒后正常*/
    uint32_t rxrb_buf_size;             /*!< 环形缓存区大小，若为0则按默认配置8K*/
    uint32_t fc_high_threshold;         /*!< 不支持*/
    uint32_t fc_low_threshold;          /*!< 不支持*/
} cm_uart_cfg_t;

/** 事件类型 */
typedef enum
{
    CM_UART_EVENT_TYPE_RX_ARRIVED = (1 << 0),   /*!< 接收到新的数据*/
    CM_UART_EVENT_TYPE_RX_OVERFLOW = (1 << 1),  /*!< 接收FIFO缓存溢出*/
    CM_UART_EVENT_TYPE_TX_COMPLETE   = (1 << 2),/*!< 不支持该事件*/
    CM_UART_EVENT_TYPE_RX_FLOWCTRL = (1 << 3)   /*!< 流控事件*/
}cm_uart_event_type_e;

/** 事件 */
/* 
 * 若注册了event_entry_ex回调，将不会触发event_entry回调。
 * 若注册了event_entry_ex回调，UART接收数据时会先保存到环形缓存区，再上报事件。
 * 若不希望数据保存到环形缓存区，则不要注册event_entry_ex回调，仅注册event_entry，回调函数就会直接上报数据。
 * @attention 特别注意！！！！！！！
 * 由于XY现有的irq event软中断框架方案是异步执行中断回调，而非扩展回调使用时，其底层的数据缓冲区非环形队列机制，如果异步中断回调不能得到及时调度，有后到的数据覆盖前到数据的风险。
 * 为规避此风险，非扩展回调没有使用软中断方案，而是直接在底层中断处理函数中直接执行回调，由于中断时不挂起flash，所以要保证使用非扩展回调时，回调函数及其内部调用的函数全部放到ram上，不可置于flash上。
 */
typedef struct{
    uint32_t event_type;    /*!< 要注册的事件，数据可读/溢出/流控等，注册后才会上报*/
    void *event_param;      /*!< 事件参数*/
    void *event_entry;      /*!< 注册cm_uart_event_cb_t类型回调函数*/
    void *event_entry_ex;   /*!< 注册cm_uart_event_cb_t_ex类型回调函数*/
} cm_uart_event_t;

/**
 *  @brief 接收回调
 *  
 *  @param [out] src 数据
 *  @param [out] len 数据长度
 *  
 *  @details 请在回调中将数据保存，回调结束后数据将被清除 \n
 *           回调函数中不可输出LOG、串口打印、执行复杂任务、阻塞或消耗过多资源。
 */
typedef void (*cm_uart_event_cb_t)(char *src, uint32_t len);

/**
 *  @brief 扩展接收回调
 *  
 *  @param [out] param 用户参数
 *  @param [out] evt   事件
 *  
 *  @details 回调函数中不可输出LOG、串口打印、执行复杂任务、阻塞或消耗过多资源。\n
 *           同一路串口，若注册了本类型回调函数，则不会触发cm_uart_event_cb_t类型的回调函数
 */
typedef void (*cm_uart_event_cb_t_ex)(void *param, uint32_t evt);

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

/****************************************************************************/

/**
 *  @brief 打开串口
 *  
 *  @param [in] dev 串口设备ID
 *  @param [in] cfg 串口配置
 *  
 *  @return  
 *    = 0  - 成功 \n
 *    < 0  - 失败, 返回值为错误码
 *  
 *  @details open之前必须先对引脚复用功能进行设置。\n
 *           串口支持的波特率及模式见cm_uart_cfg_t结构体注释说明，请详细查看波特率宏定义处的注意事项
 */
int32_t cm_uart_open(cm_uart_dev_e dev, cm_uart_cfg_t *cfg);

/**
 *  @brief 注册串口事件
 *  
 *  @param [in] dev 串口设备ID
 *  @param [in] event 串口事件
 *  
 *  @return  
 *    = 0  - 成功 \n
 *    < 0  - 失败, 返回值为错误码
 *  
 *  @details 事件包括串口数据可读/溢出/流控等。需在open之前注册。\n
 *           内置默认8K缓存区(缓存区大小可配置)保存未读出的数据，若注册了溢出事件，缓存区满后将上报溢出，缓存区溢出状态下将丢弃新接收的数据。\n
 *           回调函数中不可输出LOG、串口打印、执行复杂任务或消耗过多资源。
 */
int32_t cm_uart_register_event(cm_uart_dev_e dev, void *event);

/**
 *  @brief 关闭串口
 *  
 *  @param [in] dev 串口设备ID
 *  
 *  @return
 *    = 0  - 成功 \n
 *    < 0  - 失败, 返回值为错误码
 *  
 *  @details 
 */
int32_t cm_uart_close(cm_uart_dev_e dev);

/**
 *  @brief 串口写数据
 *  
 *  @param [in] dev 串口设备ID
 *  @param [in] data 待写入数据
 *  @param [in] len 长度
 *  @param [in] timeout 超时时间(ms)(无效参数)
 *  
 *  @return 
 *    = 实际写入长度 - 成功 \n
 *    < 0 - 失败, 返回值为错误码
 *  
 *  @details 当主串口开启流控，发送前若检测CTS(PIN23)为高则返回失败，若发送过程中检测CTS(PIN23)为高则最终返回实际发送的长度(包含已经发送的长度和缓存区中待发送的长度)
 */
int32_t cm_uart_write(cm_uart_dev_e dev, const void *data, int32_t len, int32_t timeout);

/**
 *  @brief 串口读数据
 *  
 *  @param [in] dev 串口设备ID
 *  @param [out] data 待读数据
 *  @param [in] len 长度
 *  @param [in] timeout 超时时间(ms)(无效参数)
 *  
 *  @return 
 *    = 实际读出长度 - 成功 \n
 *    < 0 - 失败, 返回值为错误码
 *  
 *  @details 
 */
int32_t cm_uart_read(cm_uart_dev_e dev, void* data, int32_t len, int32_t timeout);

/**
 *  @brief 获取串口配置
 *  
 *  @param [in]  dev 串口设备ID
 *  @param [out] cfg 串口配置
 *
 *  @return 
 *    = 0 - 成功 \n
 *    < 0 - 失败, 返回值为错误码
 *  
 *  @details 用户自行维护cfg内存空间
 */
int32_t cm_uart_get_cfg(cm_uart_dev_e dev, cm_uart_cfg_t *cfg);

/**
 *  @brief 获取串口缓存区待读数据长度
 *  
 *  @param [in] dev 串口设备ID
 *
 *  @return 
 *   >= 0 - 待读数据长度 \n
 *    < 0 - 失败, 返回值为错误码
 *  
 *  @details 
 */
int32_t cm_uart_get_rxrb_data_len(cm_uart_dev_e dev);

/**
 *  @brief 清空串口接收缓存区数据
 *  
 *  @param [in] dev 串口设备ID
 *
 *  @return 
 *    = 0 - 成功 \n
 *    < 0 - 失败, 返回值为错误码
 *  
 *  @details 
 */
int32_t cm_uart_clean(cm_uart_dev_e dev);

/**
 *  @brief 查询串口发送状态
 *  
 *  @param [in] dev 串口设备ID
 *
 *  @return 
 *    = 0 - 未在发送 \n
 *    = 1 - 正在发送
 *  
 *  @details 在cm_uart_write返回之后使用，实际为查询tx fifo状态，若为空则说明数据已从fifo全部取出，接口返回0 \n
 *           若用于开启流控的流控串口，cm_uart_write返回后此时fifo中的数据可能受流控控制而无法被取出发送，此时接口将返回1
 */
int32_t cm_uart_is_sending(cm_uart_dev_e dev);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __CM_UART_H__ */

/** @}*/
