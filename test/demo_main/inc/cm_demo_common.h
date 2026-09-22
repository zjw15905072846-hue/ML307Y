/**
 * @file        cm_demo_common.h
 * @brief       opencpu 通用头文件
 * @copyright   Copyright © 2025 China Mobile IOT. All rights reserved.
 * @author      By cmiot1325
 * @date        2025/05/20
 */

#ifndef __CM_COMMON_H__
#define __CM_COMMON_H__

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cm_os.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef FALSE
#define FALSE (0U)
#endif

#ifndef TRUE
#define TRUE (1U)
#endif

#define UART_TASK_PRIORITY osPriorityNormal

/**
 *  测试串口宏定义，只需修改此处即可变更测试串口
 *  此处请勿使用cm_uart_dev_e枚举值，否则影响本文件后续条件判断
 */
#define OPENCPU_MAIN_URAT 0

/****************************************************************************
 * Public Types
 ****************************************************************************/
 
typedef enum{
    RET_BUSY = -2,
    RET_ERROR =-1,
    RET_SUCCESS = 0
}CM_RET_E;

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

#if (OPENCPU_MAIN_URAT == 0)
#define OPENCPU_TEST_UARTTX_IOMUX CM_IOMUX_PIN_17, CM_IOMUX_FUNC_FUNCTION1
#define OPENCPU_TEST_UARTRX_IOMUX CM_IOMUX_PIN_18, CM_IOMUX_FUNC_FUNCTION1
#define OPENCPU_TEST_UARTCTS_IOMUX CM_IOMUX_PIN_22, CM_IOMUX_FUNC_FUNCTION1
#define OPENCPU_TEST_UARTRTS_IOMUX CM_IOMUX_PIN_23, CM_IOMUX_FUNC_FUNCTION1
#endif

//uart1
#if (OPENCPU_MAIN_URAT == 1)
#define OPENCPU_TEST_UARTTX_IOMUX CM_IOMUX_PIN_29, CM_IOMUX_FUNC_FUNCTION1
#define OPENCPU_TEST_UARTRX_IOMUX CM_IOMUX_PIN_28, CM_IOMUX_FUNC_FUNCTION1
#endif

#undef EXTERN
#ifdef __cplusplus
}
#endif

/** @}*/
/** @}*/

#endif /* __CM_COMMON_H__ */
