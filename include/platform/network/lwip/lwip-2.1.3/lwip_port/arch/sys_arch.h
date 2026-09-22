/**
 * @file cc.h
 * @brief 芯翼LWIP RTOS适配文件
 * @version 1.0
 * @date 2023-03-06
 * @copyright Copyright (c) 2023  芯翼信息科技有限公司
 * 
 */
#ifndef LWIP_HDR_SYS_ARCH_H
#define LWIP_HDR_SYS_ARCH_H

/* modify by cmiot1325 暂时修改通过编译 */
#if 0
#include "cmsis_os2.h"
#else
#include "cm_os.h"
#endif

#define SYS_MBOX_NULL                   (osMessageQueueId_t)0
#define SYS_SEM_NULL                    (osSemaphoreId_t)0

typedef osSemaphoreId_t sys_sem_t;
typedef osMessageQueueId_t sys_mbox_t;
typedef osThreadId_t sys_thread_t;
typedef osMutexId_t sys_mutex_t;
typedef int sys_prot_t;

#endif /* LWIP_HDR_SYS_ARCH_H */
