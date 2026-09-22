/**
 *  @file    cm_demo_main.h
 *  @brief   demo main
 *  @copyright copyright © 2025 China Mobile IOT. All rights reserved.
 *  @author by zhangxw
 *  @date 2025/02/25
 *  
 */


#ifndef __CM_DEMO_MAIN_H__
#define __CM_DEMO_MAIN_H__


/**
 *  @brief OpenCPU程序入口
 *  
 *  @param [in] param 固定传入NULL
 *  
 *  @return 成功返回0，失败返回-1
 *  
 *  @details 禁止阻塞
 */
int cm_opencpu_entry(void *param);

#endif