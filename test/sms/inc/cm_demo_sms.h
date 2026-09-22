/*********************************************************
 *  @file    cm_demo_sms.h
 *  @brief   OpenCPU 短信及URC示例
 *  Copyright (c) 2025 China Mobile IOT.
 *  All rights reserved.
 *  created by cmiot4594 2025/05/22
 ********************************************************/

#include <stdlib.h>
#include <stdio.h>
#include "cm_sys.h"
#include "cm_sms.h"
#include "cm_demo_common.h"
#include "embedded_cli.h"
#ifndef __CM_DEMO_SMS_H__
#define __CM_DEMO_SMS_H__


void cm_test_sendmsg(EmbeddedCli *cli, char *args, void *context);

#endif
