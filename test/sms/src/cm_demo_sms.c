/*********************************************************
 *  @file    cm_demo_sms.c
 *  @brief   OpenCPU 短信及URC示例
 *  Copyright (c) 2025 China Mobile IOT.
 *  All rights reserved.
 *  created by cmiot4594 2025/05/22
 ********************************************************/

#include "cm_os.h"
#include "cm_mem.h"
#include "cm_sms.h"
#include "cm_sys.h"
#include "cm_demo_common.h"
#include "cm_demo_uart.h"
#include "cm_demo_sms.h"



void cm_test_sendmsg(EmbeddedCli *cli, char *args, void *context)
{
    const char *cmd = embeddedCliGetToken(args, 1);
    if (cmd == NULL)
    {
        cm_demo_printf("invalid param\n");
        return;
    }
    int ret = -1;
    if(0 == strcasecmp((const char *)cmd, "text") || 0 == strcasecmp((const char *)cmd, "TEXT"))
    {
        //发送txt短信，+86后面替换为自己接收短信的号码
        ret = cm_sms_send_txt("MR380M", "+8615295704510", CM_MSG_MODE_GSM_7, CM_SIM_ID_0);
        if(ret == 0)
        {
            cm_demo_printf("send txt sms success\n");
        }
        else
        {
            cm_demo_printf("send txt sms fail\n");
        }
    }
    else if(0 == strcasecmp((const char *)cmd, "pdu") || 0 == strcasecmp((const char *)cmd, "PDU"))
    {
        //发送PDU短信，具体短信内容需要自行转换，以下示例仅做参考，长度为TPDU长度，
        ret = cm_sms_send_pdu("0011000D91685192754015F00000FF07F4F29C9E769F01", "22", CM_MSG_MODE_GSM_7, CM_SIM_ID_0);
        if(ret == 0)
        {
            cm_demo_printf("send pdu sms success\n");
        }
        else
        {
            cm_demo_printf("send pdu sms fail\n");
        }
    }
    else
    {
        cm_demo_printf("invalid param\n");
    }
}

// 0891683108200305F011000B813195640225F10000000FF4329EDE2ECFE7E173995E9ED301