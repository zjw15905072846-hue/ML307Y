/*********************************************************
 *  @file    cm_demo_lwm2m.h
 *  @brief   OpenCPU LWM2M示例
 *  Copyright (c) 2025 China Mobile IOT.
 *  All rights reserved.
 ********************************************************/

#ifndef __CM_DEMO_LWM2M_H__
#define __CM_DEMO_LWM2M_H__

#include <stdint.h>
#include <stddef.h>
#include "embedded_cli.h"

/**
 *  LWM2M功能调试使用示例
 *  测试命令:lwm2m <cmd> [<param>...]
 */
void cm_test_lwm2m(EmbeddedCli *cli, char *args, void *context);

#endif /* __CM_DEMO_LWM2M_H__ */
