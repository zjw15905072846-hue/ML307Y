/*********************************************************
 *  @file    cm_demo_virt_at.h
 *  @brief   OpenCPU 虚拟AT示例
 *  Copyright (c) 2025 China Mobile IOT.
 *  All rights reserved.
 *  created by cmiot4594 2025/05/22
 ********************************************************/

#include <stdlib.h>
#include <stdio.h>
#include "cm_sys.h"
#include "cm_virt_at.h"
#include "cm_demo_common.h"
#include "embedded_cli.h"
#ifndef __CM_DEMO_VIRT_AT_H__
#define __CM_DEMO_VIRT_AT_H__


void cm_virt_at_test_sync(EmbeddedCli *cli, char *args, void *context);
void cm_virt_at_test_async(EmbeddedCli *cli, char *args, void *context);
void cm_test_urc(EmbeddedCli *cli, char *args, void *context);

#endif
