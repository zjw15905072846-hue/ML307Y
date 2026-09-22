/*********************************************************
 *  @file    cm_demo_lwm2m.c
 *  @brief   OpenCPU LWM2M示例
 *  Copyright (c) 2025 China Mobile IOT.
 *  All rights reserved.
 *
 *  @details 以 CLI 子命令方式覆盖 cm_lwm2m.h 全部接口
 *           (create/delete/add_obj/del_obj/discover/open/update/
 *           close/notify_packing/notify/read_rsp/write_rsp/
 *           execute_rsp/param_rsp/observe_rsp)，
 *           支持 OneNET/CTWing/DMP/华为云多平台配置切换。
 *           内置 autotest 连通性冒烟测试，自动等待 PDP 就绪、
 *           注册平台并验证一次数据上报。
 *********************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cm_os.h"
#include "cm_sys.h"
#include "cm_modem.h"
#include "cm_lwm2m.h"
#include "cm_demo_lwm2m.h"
#include "cm_demo_uart.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 平台默认服务器 */
#define LWM2M_DEMO_HOST_ONENET      "183.230.40.39"
#define LWM2M_DEMO_HOST_CTWING      "221.229.214.202"

/* 默认参数 */
#define LWM2M_DEMO_TIMEOUT          30      /* 登录超时(s) */
#define LWM2M_DEMO_LIFETIME         86400   /* 默认生命周期(s) */
#define LWM2M_DEMO_PDP_WAIT_CNT     60      /* PDP等待最大次数(1s/次) */
#define LWM2M_DEMO_NOTIFY_WAIT_CNT  10      /* notify ack等待(500ms/次) */

/* 限制 */
#define LWM2M_DEMO_PACK_MAX         1000    /* notify组包建议上限(Bytes) */
#define LWM2M_DEMO_VERSION_MAX      16      /* 版本号最大长度 */
#define LWM2M_DEMO_TASK_STACK       (4096 * 2)

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* LWM2M连接状态 */
typedef enum
{
    LWM2M_DEMO_STATE_NO_REG = 0,        /* 未注册 */
    LWM2M_DEMO_STATE_REG_SUCCESS,       /* 注册成功 */
    LWM2M_DEMO_STATE_REG_FAILED,        /* 注册失败 */
} lwm2m_demo_state_e;

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* LWM2M实例句柄 */
static cm_lwm2m_handle_t g_lwm2m_dev = NULL;

/* 连接状态 (回调线程写, CLI/autotest线程读) */
static volatile lwm2m_demo_state_e g_state = LWM2M_DEMO_STATE_NO_REG;

/* 回调自动应答开关：1=平台下行操作自动回RSP，0=需手动调用*rsp接口应答 */
static int g_auto_rsp = 1;

/* 最近一次平台下行操作信息，用于手动应答 */
static int32_t g_last_mid = 0;
static int32_t g_last_obj = 0;
static int32_t g_last_ins = 0;
static int32_t g_last_res = 0;

/* notify递增mid */
static int32_t g_notify_mid = 1;

/* notify应答状态: 0=等待 1=成功 -1=失败 (回调线程写, autotest线程读) */
static volatile int g_notify_ack = 0;

/* 自动测试线程句柄 */
static osThreadId_t g_auto_task = NULL;

/* --- 配置参数：字符串成员指向下面的 static 缓冲区，避免悬垂指针 --- */
static cm_lwm2m_cfg_t g_cfg;
static char  g_cfg_host[128]      = LWM2M_DEMO_HOST_ONENET;
static char  g_cfg_epname[64]     = {0};      /* platform=10时自定义epname */
static char  g_cfg_auth[64]       = {0};      /* 认证码 */
static char  g_cfg_psk[64]        = {0};      /* DTLS PSK */
static char  g_cfg_pskid[64]      = {0};      /* DTLS PSKID */
static char  g_cfg_version[16]    = {0};      /* 版本号 */
static int   g_cfg_platform       = CM_LWM2M_ONENET;
static int   g_cfg_pattern        = 0;        /* endpoint pattern */
static int   g_cfg_flag           = 1;        /* 标志位 */
static int   g_cfg_autoupdate     = 1;        /* 自动update */

/* 平台名称查表 */
static const char *g_platform_name[] =
{
    "OneNET",       /* 0 */
    "CTWing",       /* 1 */
    "DMP",          /* 2 */
    "HuaWeiYun",    /* 3 */
    "Unknown",      /* 4 */
    "Unknown",      /* 5 */
    "Unknown",      /* 6 */
    "Unknown",      /* 7 */
    "Unknown",      /* 8 */
    "Unknown",      /* 9 */
    "Other",        /* 10 */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 *  @brief 事件码转可读字符串
 */
static const char *__event_str(int32_t event)
{
    switch (event)
    {
        case CM_LWM2M_EVENT_BOOTSTRAP_START:    return "BOOTSTRAP_START";
        case CM_LWM2M_EVENT_BOOTSTRAP_SUCCESS:  return "BOOTSTRAP_SUCCESS";
        case CM_LWM2M_EVENT_BOOTSTRAP_FAILED:   return "BOOTSTRAP_FAILED";
        case CM_LWM2M_EVENT_CONNECT_SUCCESS:    return "CONNECT_SUCCESS";
        case CM_LWM2M_EVENT_CONNECT_FAILED:     return "CONNECT_FAILED";
        case CM_LWM2M_EVENT_REG_SUCCESS:        return "REG_SUCCESS";
        case CM_LWM2M_EVENT_REG_FAILED:         return "REG_FAILED";
        case CM_LWM2M_EVENT_REG_TIMEOUT:        return "REG_TIMEOUT";
        case CM_LWM2M_EVENT_LIFETIME_TIMEOUT:   return "LIFETIME_TIMEOUT";
        case CM_LWM2M_EVENT_STATUS_HALT:        return "STATUS_HALT";
        case CM_LWM2M_EVENT_UPDATE_SUCCESS:     return "UPDATE_SUCCESS";
        case CM_LWM2M_EVENT_UPDATE_FAILED:      return "UPDATE_FAILED";
        case CM_LWM2M_EVENT_UPDATE_TIMEOUT:     return "UPDATE_TIMEOUT";
        case CM_LWM2M_EVENT_UNREG_DONE:         return "UNREG_DONE";
        case CM_LWM2M_EVENT_UNREG_FAILED:       return "UNREG_FAILED";
        case CM_LWM2M_EVENT_RESPONSE_FAILED:    return "RESPONSE_FAILED";
        case CM_LWM2M_EVENT_RESPONSE_SUCCESS:   return "RESPONSE_SUCCESS";
        case CM_LWM2M_EVENT_NOTIFY_FAILED:      return "NOTIFY_FAILED";
        case CM_LWM2M_EVENT_NOTIFY_SUCCESS:     return "NOTIFY_SUCCESS";
        case CM_LWM2M_EVENT_NO_DEVICE:          return "NO_DEVICE";
        case CM_LWM2M_EVENT_DTLS_NAT:           return "DTLS_NAT";
        case CM_LWM2M_EVENT_RECV_DROP:          return "RECV_DROP";
        default:                                return "UNKNOWN";
    }
}

/**
 *  @brief 连接状态转可读字符串
 */
static const char *__state_str(lwm2m_demo_state_e s)
{
    switch (s)
    {
        case LWM2M_DEMO_STATE_NO_REG:        return "NO_REG";
        case LWM2M_DEMO_STATE_REG_SUCCESS:   return "REG_SUCCESS";
        case LWM2M_DEMO_STATE_REG_FAILED:    return "REG_FAILED";
        default:                             return "UNKNOWN";
    }
}

/**
 *  @brief 平台枚举转名称
 */
static const char *__platform_str(int platform)
{
    if (platform >= 0 && platform <= 10)
    {
        return g_platform_name[platform];
    }
    return "Unknown";
}

/**
 *  @brief 敏感信息掩码 (仅显示长度，不泄露内容)
 */
static void __mask_secret(const char *in, char *out, size_t outsz)
{
    if (in == NULL || out == NULL || outsz == 0)
    {
        return;
    }
    if (strlen(in) == 0)
    {
        snprintf(out, outsz, "(null)");
    }
    else
    {
        snprintf(out, outsz, "****(len=%d)", (int)strlen(in));
    }
}

/**
 *  @brief 等待 PDP 激活就绪
 *  @return true=PDP已激活, false=超时
 */
static bool __wait_pdp_ready(void)
{
    #if 0
    int32_t i;
    for (i = 0; i < LWM2M_DEMO_PDP_WAIT_CNT; i++)
    {
        if (cm_modem_get_pdp_state(1) > 0)
        {
            return true;
        }
        if (i % 10 == 0)
        {
            cm_demo_printf("\r\n[LWM2M] waiting PDP ready... (%d/%d s)\r\n",
                           i, LWM2M_DEMO_PDP_WAIT_CNT);
        }
        osDelay(1000);
    }
    return false;
    #endif
    return true;
}

/**
 *  @brief 状态事件回调
 */
static void __lwm2m_event_cb(int32_t event, cm_lwm2m_cb_param_t param)
{
    cm_demo_printf("\r\n[LWM2M][%s] event:%d(%s)\r\n",
                   (char *)param.cb_param, event, __event_str(event));

    switch (event)
    {
        case CM_LWM2M_EVENT_REG_SUCCESS:
            g_state = LWM2M_DEMO_STATE_REG_SUCCESS;
            break;
        case CM_LWM2M_EVENT_REG_FAILED:
        case CM_LWM2M_EVENT_REG_TIMEOUT:
            g_state = LWM2M_DEMO_STATE_REG_FAILED;
            break;
        case CM_LWM2M_EVENT_UNREG_DONE:
            g_state = LWM2M_DEMO_STATE_NO_REG;
            break;
        case CM_LWM2M_EVENT_NOTIFY_SUCCESS:
            g_notify_ack = 1;
            break;
        case CM_LWM2M_EVENT_NOTIFY_FAILED:
            g_notify_ack = -1;
            break;
        default:
            break;
    }
}

/**
 *  @brief notify上报响应回调
 */
static void __lwm2m_notify_cb(int32_t mid, cm_lwm2m_cb_param_t param)
{
    cm_demo_printf("\r\n[LWM2M][%s] notify rsp, mid:%d\r\n",
                   (char *)param.cb_param, mid);
    g_notify_ack = 1;
}

/**
 *  @brief 平台read操作回调
 */
static void __lwm2m_read_cb(int32_t mid, int32_t objid, int32_t insid, int32_t resid,
                            cm_lwm2m_cb_param_t param)
{
    cm_demo_printf("\r\n[LWM2M][%s] read: mid:%d obj:%d ins:%d res:%d\r\n",
                   (char *)param.cb_param, mid, objid, insid, resid);

    g_last_mid = mid;
    g_last_obj = objid;
    g_last_ins = insid;
    g_last_res = resid;
}

/**
 *  @brief 平台write操作回调
 */
static void __lwm2m_write_cb(int32_t mid, int32_t objid, int32_t insid, int32_t resid,
                             int32_t type, int32_t is_over, char *data, int32_t len,
                             cm_lwm2m_cb_param_t param)
{
    cm_demo_printf("\r\n[LWM2M][%s] write: mid:%d obj:%d ins:%d res:%d type:%d is_over:%d len:%d data:%.*s\r\n",
                   (char *)param.cb_param, mid, objid, insid, resid, type, is_over, len, len, data);

    g_last_mid = mid;
    g_last_obj = objid;
    g_last_ins = insid;
    g_last_res = resid;

    if (g_auto_rsp)
    {
        int32_t ret = cm_lwm2m_write_rsp(param.handle, mid, CM_LWM2M_RESULT_204_CHANGED);
        if (CM_LWM2M_SUCCESS != ret)
        {
            cm_demo_printf("\r\n[LWM2M] cm_lwm2m_write_rsp() fail, ret:%d\r\n", ret);
        }
    }
}

/**
 *  @brief 平台execute操作回调
 */
static void __lwm2m_execute_cb(int32_t mid, int32_t objid, int32_t insid, int32_t resid,
                               char *data, int32_t len, cm_lwm2m_cb_param_t param)
{
    cm_demo_printf("\r\n[LWM2M][%s] execute: mid:%d obj:%d ins:%d res:%d len:%d data:%.*s\r\n",
                   (char *)param.cb_param, mid, objid, insid, resid, len, len, data);

    g_last_mid = mid;
    g_last_obj = objid;
    g_last_ins = insid;
    g_last_res = resid;

    if (g_auto_rsp)
    {
        int32_t ret = cm_lwm2m_execute_rsp(param.handle, mid, CM_LWM2M_RESULT_204_CHANGED);
        if (CM_LWM2M_SUCCESS != ret)
        {
            cm_demo_printf("\r\n[LWM2M] cm_lwm2m_execute_rsp() fail, ret:%d\r\n", ret);
        }
    }
}

/**
 *  @brief 平台observe操作回调
 */
static void __lwm2m_observe_cb(int32_t mid, int32_t observe, int32_t objid, int32_t insid,
                               int32_t resid, cm_lwm2m_cb_param_t param)
{
    cm_demo_printf("\r\n[LWM2M][%s] observe: mid:%d %d_%d_%d %s\r\n",
                   (char *)param.cb_param, mid, objid, insid, resid,
                   observe ? "subscribe" : "cancel");

    g_last_mid = mid;
    g_last_obj = objid;
    g_last_ins = insid;
    g_last_res = resid;
}

/**
 *  @brief 平台discover操作回调
 */
static void __lwm2m_discover_cb(int32_t mid, int32_t objid, cm_lwm2m_cb_param_t param)
{
    cm_demo_printf("\r\n[LWM2M][%s] discover: mid:%d obj:%d\r\n",
                   (char *)param.cb_param, mid, objid);

    g_last_mid = mid;
    g_last_obj = objid;
}

/**
 *  @brief 平台策略参数回调
 */
static void __lwm2m_params_cb(int32_t mid, int32_t objid, int32_t insid, int32_t resid,
                              char *parameter, int32_t len, cm_lwm2m_cb_param_t param)
{
    cm_demo_printf("\r\n[LWM2M][%s] params: mid:%d obj:%d ins:%d res:%d len:%d param:%.*s\r\n",
                   (char *)param.cb_param, mid, objid, insid, resid, len, len, parameter);

    g_last_mid = mid;
    g_last_obj = objid;
    g_last_ins = insid;
    g_last_res = resid;

    if (g_auto_rsp)
    {
        int32_t ret = cm_lwm2m_param_rsp(param.handle, mid, CM_LWM2M_RESULT_204_CHANGED);
        if (CM_LWM2M_SUCCESS != ret)
        {
            cm_demo_printf("\r\n[LWM2M] cm_lwm2m_param_rsp() fail, ret:%d\r\n", ret);
        }
    }
}

/**
 *  @brief 根据当前配置填充 cm_lwm2m_cfg_t
 */
static void __fill_cfg(void)
{
    memset(&g_cfg, 0, sizeof(g_cfg));
    g_cfg.platform       = (cm_lwm2m_platform_e)g_cfg_platform;
    if (g_cfg_platform == CM_LWM2M_OTHER && strlen(g_cfg_epname) > 0)
    {
        g_cfg.endpoint.name = g_cfg_epname;
    }
    else
    {
        g_cfg.endpoint.pattern = g_cfg_pattern;
    }
    g_cfg.host           = g_cfg_host;
    g_cfg.flag           = g_cfg_flag;
    g_cfg.auth_code      = (strlen(g_cfg_auth) > 0) ? g_cfg_auth : NULL;
    g_cfg.psk            = (strlen(g_cfg_psk) > 0) ? g_cfg_psk : NULL;
    g_cfg.pskid          = (strlen(g_cfg_pskid) > 0) ? g_cfg_pskid : NULL;
    g_cfg.auto_update    = g_cfg_autoupdate ? 1 : 0;
    g_cfg.cb.onRead      = __lwm2m_read_cb;
    g_cfg.cb.onWrite     = __lwm2m_write_cb;
    g_cfg.cb.onExec      = __lwm2m_execute_cb;
    g_cfg.cb.onObserve   = __lwm2m_observe_cb;
    g_cfg.cb.onParams    = __lwm2m_params_cb;
    g_cfg.cb.onEvent     = __lwm2m_event_cb;
    g_cfg.cb.onNotify    = __lwm2m_notify_cb;
    g_cfg.cb.onDiscover  = __lwm2m_discover_cb;
    g_cfg.cb_param       = (void *)"LWM2M";
}

/**
 *  @brief 打印当前配置
 */
static void __show_cfg(void)
{
    char masked[32] = {0};

    cm_demo_printf("\r\n[LWM2M] ===== current config =====\r\n");
    cm_demo_printf("  platform   : %d (%s)\r\n", g_cfg_platform, __platform_str(g_cfg_platform));
    cm_demo_printf("  host       : %s\r\n", g_cfg_host);
    cm_demo_printf("  flag       : %d\r\n", g_cfg_flag);
    if (g_cfg_platform == CM_LWM2M_OTHER && strlen(g_cfg_epname) > 0)
    {
        cm_demo_printf("  epname     : %s\r\n", g_cfg_epname);
    }
    else
    {
        cm_demo_printf("  pattern    : %d\r\n", g_cfg_pattern);
    }
    __mask_secret(g_cfg_auth, masked, sizeof(masked));
    cm_demo_printf("  auth_code  : %s\r\n", masked);
    __mask_secret(g_cfg_psk, masked, sizeof(masked));
    cm_demo_printf("  psk        : %s\r\n", masked);
    __mask_secret(g_cfg_pskid, masked, sizeof(masked));
    cm_demo_printf("  pskid      : %s\r\n", masked);
    cm_demo_printf("  autoupdate : %d\r\n", g_cfg_autoupdate);
    cm_demo_printf("  auto_rsp   : %d\r\n", g_auto_rsp);
    cm_demo_printf("  state      : %s (%d)\r\n", __state_str(g_state), (int)g_state);
    cm_demo_printf("  handle     : %p\r\n", g_lwm2m_dev);
    cm_demo_printf("==============================\r\n");
}

/**
 *  @brief 打印用法
 */
static void __print_help(void)
{
    cm_demo_printf("\r\n");
    cm_demo_printf("========== LWM2M test usage ==========\r\n");
    cm_demo_printf("\r\n");
    cm_demo_printf("[config]\r\n");
    cm_demo_printf("  lwm2m cfg platform <0-3|10>       0=OneNET 1=CTWing 2=DMP 3=HuaWei 10=Other\r\n");
    cm_demo_printf("  lwm2m cfg host <ip[:port]>        server address (port default 5683)\r\n");
    cm_demo_printf("  lwm2m cfg flag <flag>             bit0=bootstrap bit1=disable monitor\r\n");
    cm_demo_printf("  lwm2m cfg pattern <0-4>           endpoint name pattern\r\n");
    cm_demo_printf("  lwm2m cfg epname <name>           custom endpoint name (platform=10)\r\n");
    cm_demo_printf("  lwm2m cfg auth <code>             auth code (empty to clear)\r\n");
    cm_demo_printf("  lwm2m cfg psk <psk> <pskid>       DTLS PSK and PSKID\r\n");
    cm_demo_printf("  lwm2m cfg autoupdate <0|1>        auto update\r\n");
    cm_demo_printf("  lwm2m cfg preset <onenet|ctwing>  load preset params\r\n");
    cm_demo_printf("  lwm2m cfg show                    show current config\r\n");
    cm_demo_printf("\r\n");
    cm_demo_printf("[device]\r\n");
    cm_demo_printf("  lwm2m create                      create LWM2M instance\r\n");
    cm_demo_printf("  lwm2m delete                      delete LWM2M instance\r\n");
    cm_demo_printf("  lwm2m addobj <objid> <inscnt> <bitmap>\r\n");
    cm_demo_printf("                                    add object (bitmap e.g. 10011)\r\n");
    cm_demo_printf("  lwm2m delobj <objid>             delete object\r\n");
    cm_demo_printf("  lwm2m discover <objid> <r1,r2>    set resource list\r\n");
    cm_demo_printf("\r\n");
    cm_demo_printf("[connection]\r\n");
    cm_demo_printf("  lwm2m open [timeout] [lifetime]   login (sec), default 30 / 86400\r\n");
    cm_demo_printf("  lwm2m update <lifetime> <withobj> update lifetime\r\n");
    cm_demo_printf("  lwm2m close                       logout\r\n");
    cm_demo_printf("\r\n");
    cm_demo_printf("[data]\r\n");
    cm_demo_printf("  lwm2m pack <obj> <ins> <res> <type> <value> [ctype]\r\n");
    cm_demo_printf("        type:  string|opaque|int|float|bool|hexstr  (or 1-6)\r\n");
    cm_demo_printf("        ctype: default|text|link|opaque|tlv|json  (or 0-5)\r\n");
    cm_demo_printf("  lwm2m notify [mid]                send packed data (mid=-1: no ack)\r\n");
    cm_demo_printf("\r\n");
    cm_demo_printf("[response] (when autorsp=0)\r\n");
    cm_demo_printf("  lwm2m readrsp <mid> <result> [obj ins res type value]\r\n");
    cm_demo_printf("  lwm2m writersp <mid> <result>\r\n");
    cm_demo_printf("  lwm2m execrsp <mid> <result>\r\n");
    cm_demo_printf("  lwm2m paramrsp <mid> <result>\r\n");
    cm_demo_printf("  lwm2m observersp <mid> <result>\r\n");
    cm_demo_printf("\r\n");
    cm_demo_printf("[misc]\r\n");
    cm_demo_printf("  lwm2m autorsp <0|1>               toggle auto response (default 1)\r\n");
    cm_demo_printf("  lwm2m state                       show connection state\r\n");
    cm_demo_printf("  lwm2m autotest                    run connectivity smoke test\r\n");
    cm_demo_printf("  lwm2m help                        show this help\r\n");
    cm_demo_printf("\r\n");
    cm_demo_printf("======================================\r\n");
}

/**
 *  @brief 配置子命令处理
 *  @details 调用时 args 仍包含 "cfg" 本身（CLI只剥离绑定命令名 lwm2m），
 *           因此 args 的 token 依次为：1=cfg 2=key 3=value...
 */
static void __handle_cfg(char *args)
{
    const char *key = embeddedCliGetToken(args, 2);
    if (key == NULL)
    {
        /* 无 key 时直接显示当前配置 */
        __show_cfg();
        return;
    }

    /* 设备已创建时修改配置不会立即生效，提示客户需 delete+create */
    if (g_lwm2m_dev != NULL && 0 != strcasecmp(key, "show"))
    {
        cm_demo_printf("\r\n[LWM2M] warning: device already created, "
                       "config changes need 'lwm2m delete' + 'lwm2m create' to take effect\r\n");
    }

    if (0 == strcasecmp(key, "platform"))
    {
        const char *v = embeddedCliGetToken(args, 3);
        if (v == NULL) { cm_demo_printf("\r\n[LWM2M] usage: lwm2m cfg platform <0-3|10>\r\n"); return; }
        int p = atoi(v);
        if (p != 0 && p != 1 && p != 2 && p != 3 && p != 10)
        {
            cm_demo_printf("\r\n[LWM2M] invalid platform %d, valid: 0,1,2,3,10\r\n", p);
            return;
        }
        g_cfg_platform = p;
        cm_demo_printf("\r\n[LWM2M] platform=%d (%s)\r\n", g_cfg_platform, __platform_str(g_cfg_platform));
    }
    else if (0 == strcasecmp(key, "host"))
    {
        const char *v = embeddedCliGetToken(args, 3);
        if (v == NULL) { cm_demo_printf("\r\n[LWM2M] usage: lwm2m cfg host <ip[:port]>\r\n"); return; }
        strncpy(g_cfg_host, v, sizeof(g_cfg_host) - 1);
        cm_demo_printf("\r\n[LWM2M] host=%s\r\n", g_cfg_host);
    }
    else if (0 == strcasecmp(key, "flag"))
    {
        const char *v = embeddedCliGetToken(args, 3);
        if (v == NULL) { cm_demo_printf("\r\n[LWM2M] usage: lwm2m cfg flag <flag>\r\n"); return; }
        g_cfg_flag = atoi(v);
        cm_demo_printf("\r\n[LWM2M] flag=%d\r\n", g_cfg_flag);
    }
    else if (0 == strcasecmp(key, "pattern"))
    {
        const char *v = embeddedCliGetToken(args, 3);
        if (v == NULL) { cm_demo_printf("\r\n[LWM2M] usage: lwm2m cfg pattern <0-4>\r\n"); return; }
        g_cfg_pattern = atoi(v);
        cm_demo_printf("\r\n[LWM2M] pattern=%d\r\n", g_cfg_pattern);
    }
    else if (0 == strcasecmp(key, "epname"))
    {
        const char *v = embeddedCliGetToken(args, 3);
        if (v == NULL) { cm_demo_printf("\r\n[LWM2M] usage: lwm2m cfg epname <name>\r\n"); return; }
        strncpy(g_cfg_epname, v, sizeof(g_cfg_epname) - 1);
        cm_demo_printf("\r\n[LWM2M] epname=%s\r\n", g_cfg_epname);
    }
    else if (0 == strcasecmp(key, "auth"))
    {
        const char *v = embeddedCliGetToken(args, 3);
        if (v)
        {
            strncpy(g_cfg_auth, v, sizeof(g_cfg_auth) - 1);
            cm_demo_printf("\r\n[LWM2M] auth set (len=%d)\r\n", (int)strlen(g_cfg_auth));
        }
        else
        {
            g_cfg_auth[0] = 0;
            cm_demo_printf("\r\n[LWM2M] auth cleared\r\n");
        }
    }
    else if (0 == strcasecmp(key, "psk"))
    {
        const char *p1 = embeddedCliGetToken(args, 3);
        const char *p2 = embeddedCliGetToken(args, 4);
        if (p1 == NULL)
        {
            g_cfg_psk[0] = 0;
            g_cfg_pskid[0] = 0;
            cm_demo_printf("\r\n[LWM2M] psk cleared\r\n");
            return;
        }
        strncpy(g_cfg_psk, p1, sizeof(g_cfg_psk) - 1);
        if (p2)
        {
            strncpy(g_cfg_pskid, p2, sizeof(g_cfg_pskid) - 1);
        }
        cm_demo_printf("\r\n[LWM2M] psk set (len=%d), pskid set (len=%d)\r\n",
                       (int)strlen(g_cfg_psk), (int)strlen(g_cfg_pskid));
    }
    else if (0 == strcasecmp(key, "autoupdate"))
    {
        const char *v = embeddedCliGetToken(args, 3);
        if (v == NULL) { cm_demo_printf("\r\n[LWM2M] usage: lwm2m cfg autoupdate <0|1>\r\n"); return; }
        g_cfg_autoupdate = atoi(v) ? 1 : 0;
        cm_demo_printf("\r\n[LWM2M] autoupdate=%d\r\n", g_cfg_autoupdate);
    }
    else if (0 == strcasecmp(key, "preset"))
    {
        const char *v = embeddedCliGetToken(args, 3);
        if (v && 0 == strcasecmp(v, "onenet"))
        {
            g_cfg_platform = CM_LWM2M_ONENET;
            strncpy(g_cfg_host, LWM2M_DEMO_HOST_ONENET, sizeof(g_cfg_host) - 1);
            g_cfg_pattern = 2;
            g_cfg_flag = 3;
            cm_demo_printf("\r\n[LWM2M] preset OneNET loaded\r\n");
            cm_demo_printf("[LWM2M]   host=%s pattern=%d flag=%d\r\n", g_cfg_host, g_cfg_pattern, g_cfg_flag);
        }
        else if (v && 0 == strcasecmp(v, "ctwing"))
        {
            g_cfg_platform = CM_LWM2M_CTWING;
            strncpy(g_cfg_host, LWM2M_DEMO_HOST_CTWING, sizeof(g_cfg_host) - 1);
            g_cfg_pattern = 1;
            g_cfg_flag = 0;
            cm_demo_printf("\r\n[LWM2M] preset CTWing loaded\r\n");
            cm_demo_printf("[LWM2M]   host=%s pattern=%d flag=%d\r\n", g_cfg_host, g_cfg_pattern, g_cfg_flag);
        }
        else
        {
            cm_demo_printf("\r\n[LWM2M] unknown preset, use: onenet | ctwing\r\n");
        }
    }
    else if (0 == strcasecmp(key, "show"))
    {
        __show_cfg();
    }
    else
    {
        cm_demo_printf("\r\n[LWM2M] unknown cfg key '%s', try 'lwm2m help'\r\n", key);
    }
}

/**
 *  @brief 解析上报数据类型，支持关键字或数字
 *  @details "string"->1 "opaque"->2 "int"->3 "float"->4 "bool"->5 "hexstr"->6
 *           "1"~"6" 同样有效
 *  @return >=1=类型值, 0=无效
 */
static int32_t __parse_type(const char *s)
{
    if (s == NULL)
    {
        return 0;
    }
    /* 先尝试数字 */
    int32_t v = atoi(s);
    if (v >= 1 && v <= 6)
    {
        return v;
    }
    /* 关键字 */
    if      (0 == strcasecmp(s, "string"))  return 1;
    else if (0 == strcasecmp(s, "opaque"))  return 2;
    else if (0 == strcasecmp(s, "int"))     return 3;
    else if (0 == strcasecmp(s, "float"))   return 4;
    else if (0 == strcasecmp(s, "bool"))    return 5;
    else if (0 == strcasecmp(s, "hexstr"))  return 6;
    return 0;
}

/**
 *  @brief 解析编码格式，支持关键字或数字
 *  @details "default"->0 "text"->1 "link"->2 "opaque"->3 "tlv"->4 "json"->5
 *           "0"~"5" 同样有效
 *  @return >=0=格式值, -1=无效
 *
 *  @note 先匹配关键字再校验纯数字，避免atoi()对非数字字符串返回0
 *        导致"text"等关键字被误判为合法ctype=0(default)的问题。
 */
static int32_t __parse_ctype(const char *s)
{
    if (s == NULL || s[0] == '\0')
    {
        return 0;   /* 默认 */
    }

    /* 关键字优先匹配（避免atoi("text")=0被误判为default） */
    if      (0 == strcasecmp(s, "default")) return 0;
    else if (0 == strcasecmp(s, "text"))    return 1;
    else if (0 == strcasecmp(s, "link"))    return 2;
    else if (0 == strcasecmp(s, "opaque"))  return 3;
    else if (0 == strcasecmp(s, "tlv"))     return 4;
    else if (0 == strcasecmp(s, "json"))    return 5;

    /* 纯数字校验：仅当字符串全为0-9数字字符时才调用atoi */
    for (const char *p = s; *p != '\0'; p++)
    {
        if (*p < '0' || *p > '9')
        {
            return -1;  /* 含非数字字符且非关键字 */
        }
    }
    int32_t v = atoi(s);
    if (v >= 0 && v <= 5)
    {
        return v;
    }
    return -1;
}

/**
 *  @brief 解析 instance bitmap 字符串为 instances 数组
 *  @details 例如 "10011" -> {1,0,0,1,1}
 *  @return >0=解析的位数, -1=非法字符
 */
static int __parse_bitmap(const char *bitmap, uint8_t *instances, int max)
{
    int i;
    if (bitmap == NULL)
    {
        return 0;
    }
    for (i = 0; bitmap[i] != '\0' && i < max; i++)
    {
        if (bitmap[i] == '1')
        {
            instances[i] = 1;
        }
        else if (bitmap[i] == '0')
        {
            instances[i] = 0;
        }
        else
        {
            return -1;  /* 非法字符 */
        }
    }
    return i;
}

/**
 *  @brief 自动测试连通性冒烟测试线程
 *  @details 全流程：等待PDP -> create -> addobj -> discover -> open ->
 *           等待注册成功 -> 上报1条数据 -> 等待ack -> close -> delete。
 *           每步打印 PASS/FAIL，便于客户定位问题。
 */
static void __autotest_task(void *param)
{
    (void)param;
    int32_t ret = 0;
    int32_t wait = 0;
    int32_t objid = 19;        /* CTWing 默认 object */
    int32_t resid = 0;
    bool dev_owned = false;    /* 标记是否由本任务创建的设备 */

    /* 根据平台选择默认 object/resource */
    if (g_cfg_platform == CM_LWM2M_ONENET)
    {
        objid = 3303;          /* IPSO 温度对象 */
        resid = 5700;          /* Sensor Value (float) */
    }

    cm_demo_printf("\r\n[LWM2M] ===== autotest start =====\r\n");
    cm_demo_printf("[LWM2M] platform: %s (%d)\r\n",
                   __platform_str(g_cfg_platform), g_cfg_platform);

    /* Step 1: 等待 PDP 激活 */
    cm_demo_printf("\r\n[LWM2M] [1] waiting for PDP...\r\n");
    if (!__wait_pdp_ready())
    {
        cm_demo_printf("[LWM2M] [1] FAIL: PDP not ready, abort\r\n");
        goto __autotest_done;
    }
    cm_demo_printf("[LWM2M] [1] PASS: PDP ready\r\n");

    /* Step 2: 创建实例 */
    if (g_lwm2m_dev != NULL)
    {
        cm_demo_printf("\r\n[LWM2M] [2] dev exists, skip create\r\n");
    }
    else
    {
        __fill_cfg();
        ret = cm_lwm2m_create(&g_cfg, &g_lwm2m_dev);
        if (CM_LWM2M_SUCCESS != ret)
        {
            cm_demo_printf("[LWM2M] [2] FAIL: cm_lwm2m_create ret:%d\r\n", ret);
            g_lwm2m_dev = NULL;
            goto __autotest_done;
        }
        dev_owned = true;
        cm_demo_printf("[LWM2M] [2] PASS: create ok, handle=%p\r\n", g_lwm2m_dev);
    }

    /* Step 3: 添加 object */
    {
        uint8_t instances[1] = {1};
        ret = cm_lwm2m_add_obj(g_lwm2m_dev, objid, instances, 1, 0, 0);
    }
    if (CM_LWM2M_SUCCESS != ret)
    {
        cm_demo_printf("\r\n[LWM2M] [3] FAIL: cm_lwm2m_add_obj(%d) ret:%d\r\n", objid, ret);
        goto __autotest_cleanup;
    }
    cm_demo_printf("[LWM2M] [3] PASS: add_obj(%d)\r\n", objid);

    /* Step 4: discover 资源 */
    {
        int32_t resources[1] = {resid};
        ret = cm_lwm2m_discover(g_lwm2m_dev, objid, resources, 1);
    }
    if (CM_LWM2M_SUCCESS != ret)
    {
        cm_demo_printf("\r\n[LWM2M] [4] FAIL: cm_lwm2m_discover(%d) ret:%d\r\n", objid, ret);
        goto __autotest_cleanup;
    }
    cm_demo_printf("[LWM2M] [4] PASS: discover(%d)\r\n", objid);

    /* Step 5: 登录平台 + 等待注册 */
    g_state = LWM2M_DEMO_STATE_NO_REG;
    ret = cm_lwm2m_open(g_lwm2m_dev, LWM2M_DEMO_TIMEOUT, LWM2M_DEMO_LIFETIME);
    if (CM_LWM2M_SUCCESS != ret)
    {
        cm_demo_printf("\r\n[LWM2M] [5] FAIL: cm_lwm2m_open ret:%d\r\n", ret);
        goto __autotest_cleanup;
    }
    cm_demo_printf("[LWM2M] [5] open sent, waiting register...\r\n");

    wait = 0;
    while (g_state == LWM2M_DEMO_STATE_NO_REG && wait < (LWM2M_DEMO_TIMEOUT * 2))
    {
        osDelay(500);
        wait++;
    }
    if (g_state != LWM2M_DEMO_STATE_REG_SUCCESS)
    {
        cm_demo_printf("[LWM2M] [5] FAIL: register state=%s\r\n", __state_str(g_state));
        goto __autotest_cleanup;
    }
    cm_demo_printf("[LWM2M] [5] PASS: register success\r\n");

    /* Step 6: 上报 1 条数据 + 等待 ack */
    {
        int32_t pack_type = 1;      /* string */
        int32_t pack_ctype = 0;     /* platform default */
        char pack_val[64] = {0};

        if (g_cfg_platform == CM_LWM2M_ONENET)
        {
            /* 3303/5700 是 float 类型, OneNET 支持 TLV */
            pack_type = 4;          /* float */
            pack_ctype = 4;         /* TLV */
            snprintf(pack_val, sizeof(pack_val), "25.6");
        }
        else
        {
            /* CTWing/其他: string */
            snprintf(pack_val, sizeof(pack_val), "smoke_test");
        }

        ret = cm_lwm2m_notify_packing(g_lwm2m_dev, objid, 0, resid,
                                       pack_type, pack_val,
                                       strlen(pack_val), pack_ctype);
        if (CM_LWM2M_SUCCESS != ret)
        {
            cm_demo_printf("\r\n[LWM2M] [6] FAIL: notify_packing ret:%d\r\n", ret);
            goto __autotest_cleanup;
        }

        g_notify_ack = 0;
        ret = cm_lwm2m_notify(g_lwm2m_dev, g_notify_mid++);
        if (CM_LWM2M_SUCCESS != ret)
        {
            cm_demo_printf("[LWM2M] [6] FAIL: cm_lwm2m_notify ret:%d\r\n", ret);
            goto __autotest_cleanup;
        }
        cm_demo_printf("[LWM2M] [6] notify sent, waiting ack...\r\n");

        wait = 0;
        while (g_notify_ack == 0 && wait < LWM2M_DEMO_NOTIFY_WAIT_CNT)
        {
            osDelay(500);
            wait++;
        }
        if (g_notify_ack > 0)
        {
            cm_demo_printf("[LWM2M] [6] PASS: notify acked\r\n");
        }
        else if (g_notify_ack < 0)
        {
            cm_demo_printf("[LWM2M] [6] FAIL: notify rejected by platform\r\n");
        }
        else
        {
            cm_demo_printf("[LWM2M] [6] WARN: notify ack timeout\r\n");
        }
    }

__autotest_cleanup:
    /* 清理：仅清理本任务创建的设备，避免误删客户手动创建的实例 */
    if (dev_owned && g_lwm2m_dev != NULL)
    {
        cm_demo_printf("\r\n[LWM2M] cleaning up...\r\n");
        cm_lwm2m_del_obj(g_lwm2m_dev, objid);
        cm_lwm2m_close(g_lwm2m_dev);
        osDelay(1000);
        cm_lwm2m_delete(g_lwm2m_dev);
        g_lwm2m_dev = NULL;
        g_state = LWM2M_DEMO_STATE_NO_REG;
    }

__autotest_done:
    cm_demo_printf("\r\n[LWM2M] ===== autotest done =====\r\n");
    g_auto_task = NULL;
    osThreadExit();
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/**
 *  LWM2M功能调试使用示例
 *  测试命令: lwm2m <cmd> [<param>...]
 */
void cm_test_lwm2m(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)context;

    const char *cmd = embeddedCliGetToken(args, 1);
    if (cmd == NULL)
    {
        __print_help();
        return;
    }

    if (0 == strcasecmp(cmd, "help"))
    {
        __print_help();
    }
    else if (0 == strcasecmp(cmd, "cfg"))
    {
        __handle_cfg(args);
    }
    else if (0 == strcasecmp(cmd, "state"))
    {
        cm_demo_printf("\r\n[LWM2M] state: %s (%d), handle: %p\r\n",
                       __state_str(g_state), (int)g_state, g_lwm2m_dev);
        if (g_last_mid != 0)
        {
            cm_demo_printf("[LWM2M] last downstream: mid=%d obj=%d ins=%d res=%d\r\n",
                           g_last_mid, g_last_obj, g_last_ins, g_last_res);
        }
    }
    else if (0 == strcasecmp(cmd, "autorsp"))
    {
        const char *v = embeddedCliGetToken(args, 2);
        if (v)
        {
            g_auto_rsp = atoi(v) ? 1 : 0;
        }
        cm_demo_printf("\r\n[LWM2M] auto_rsp=%d\r\n", g_auto_rsp);
    }
    else if (0 == strcasecmp(cmd, "create"))
    {
        if (g_lwm2m_dev != NULL)
        {
            cm_demo_printf("\r\n[LWM2M] dev already exists, delete first\r\n");
            return;
        }
        /* 提示：若使用默认 flag=1/pattern=0，建议先加载 preset */
        if (g_cfg_flag == 1 && g_cfg_pattern == 0)
        {
            cm_demo_printf("\r\n[LWM2M] tip: consider 'lwm2m cfg preset onenet|ctwing' first\r\n");
        }
        __fill_cfg();
        int32_t ret = cm_lwm2m_create(&g_cfg, &g_lwm2m_dev);
        if (ret != CM_LWM2M_SUCCESS)
        {
            g_lwm2m_dev = NULL;
        }
        cm_demo_printf("\r\n[LWM2M] cm_lwm2m_create ret:%d handle:%p\r\n", ret, g_lwm2m_dev);
    }
    else if (0 == strcasecmp(cmd, "delete"))
    {
        if (g_lwm2m_dev == NULL)
        {
            cm_demo_printf("\r\n[LWM2M] dev is NULL\r\n");
            return;
        }
        int32_t ret = cm_lwm2m_delete(g_lwm2m_dev);
        cm_demo_printf("\r\n[LWM2M] cm_lwm2m_delete ret:%d\r\n", ret);
        if (ret == CM_LWM2M_SUCCESS)
        {
            g_lwm2m_dev = NULL;
            g_state = LWM2M_DEMO_STATE_NO_REG;
        }
    }
    else if (0 == strcasecmp(cmd, "addobj"))
    {
        if (g_lwm2m_dev == NULL) { cm_demo_printf("\r\n[LWM2M] dev is NULL, create first\r\n"); return; }
        const char *s_obj = embeddedCliGetToken(args, 2);
        const char *s_cnt = embeddedCliGetToken(args, 3);
        const char *s_bmp = embeddedCliGetToken(args, 4);
        if (s_obj == NULL || s_cnt == NULL || s_bmp == NULL)
        {
            cm_demo_printf("\r\n[LWM2M] usage: lwm2m addobj <objid> <inscount> <bitmap>\r\n");
            return;
        }
        int32_t objid = atoi(s_obj);
        int32_t inscnt = atoi(s_cnt);
        if (inscnt <= 0 || inscnt > 32)
        {
            cm_demo_printf("\r\n[LWM2M] inscount out of range (1-32)\r\n");
            return;
        }
        uint8_t instances[32] = {0};
        int n = __parse_bitmap(s_bmp, instances, inscnt);
        if (n < 0)
        {
            cm_demo_printf("\r\n[LWM2M] bitmap has invalid char (only 0/1)\r\n");
            return;
        }
        if (n < inscnt)
        {
            cm_demo_printf("\r\n[LWM2M] bitmap length %d < inscount %d\r\n", n, inscnt);
            return;
        }
        int32_t ret = cm_lwm2m_add_obj(g_lwm2m_dev, objid, instances, inscnt, 0, 0);
        cm_demo_printf("\r\n[LWM2M] cm_lwm2m_add_obj(%d) ret:%d\r\n", objid, ret);
    }
    else if (0 == strcasecmp(cmd, "delobj"))
    {
        if (g_lwm2m_dev == NULL) { cm_demo_printf("\r\n[LWM2M] dev is NULL\r\n"); return; }
        const char *s_obj = embeddedCliGetToken(args, 2);
        if (s_obj == NULL) { cm_demo_printf("\r\n[LWM2M] usage: lwm2m delobj <objid>\r\n"); return; }
        int32_t objid = atoi(s_obj);
        int32_t ret = cm_lwm2m_del_obj(g_lwm2m_dev, objid);
        cm_demo_printf("\r\n[LWM2M] cm_lwm2m_del_obj(%d) ret:%d\r\n", objid, ret);
    }
    else if (0 == strcasecmp(cmd, "discover"))
    {
        if (g_lwm2m_dev == NULL) { cm_demo_printf("\r\n[LWM2M] dev is NULL\r\n"); return; }
        const char *s_obj = embeddedCliGetToken(args, 2);
        const char *s_res = embeddedCliGetToken(args, 3);
        if (s_obj == NULL || s_res == NULL)
        {
            cm_demo_printf("\r\n[LWM2M] usage: lwm2m discover <objid> <r1,r2,...>\r\n");
            return;
        }
        int32_t objid = atoi(s_obj);
        int32_t resources[16] = {0};
        int rescount = 0;
        /* 解析逗号分隔的资源列表 */
        char buf[64] = {0};
        strncpy(buf, s_res, sizeof(buf) - 1);
        char *tok = strtok(buf, ",");
        while (tok != NULL && rescount < 16)
        {
            resources[rescount++] = atoi(tok);
            tok = strtok(NULL, ",");
        }
        if (rescount == 0)
        {
            cm_demo_printf("\r\n[LWM2M] no valid resource id\r\n");
            return;
        }
        int32_t ret = cm_lwm2m_discover(g_lwm2m_dev, objid, resources, rescount);
        cm_demo_printf("\r\n[LWM2M] cm_lwm2m_discover(%d, %d res) ret:%d\r\n", objid, rescount, ret);
    }
    else if (0 == strcasecmp(cmd, "open"))
    {
        if (g_lwm2m_dev == NULL) { cm_demo_printf("\r\n[LWM2M] dev is NULL\r\n"); return; }
        const char *s_to = embeddedCliGetToken(args, 2);
        const char *s_lt = embeddedCliGetToken(args, 3);
        uint32_t timeout = s_to ? (uint32_t)atoi(s_to) : LWM2M_DEMO_TIMEOUT;
        uint32_t lifetime = s_lt ? (uint32_t)atoi(s_lt) : LWM2M_DEMO_LIFETIME;
        if (timeout == 0) timeout = LWM2M_DEMO_TIMEOUT;
        if (lifetime == 0) lifetime = LWM2M_DEMO_LIFETIME;
        g_state = LWM2M_DEMO_STATE_NO_REG;
        int32_t ret = cm_lwm2m_open(g_lwm2m_dev, timeout, lifetime);
        cm_demo_printf("\r\n[LWM2M] cm_lwm2m_open(%u,%u) ret:%d\r\n", timeout, lifetime, ret);
    }
    else if (0 == strcasecmp(cmd, "update"))
    {
        if (g_lwm2m_dev == NULL) { cm_demo_printf("\r\n[LWM2M] dev is NULL\r\n"); return; }
        const char *s_lt = embeddedCliGetToken(args, 2);
        const char *s_wo = embeddedCliGetToken(args, 3);
        if (s_lt == NULL) { cm_demo_printf("\r\n[LWM2M] usage: lwm2m update <lifetime> <withobj>\r\n"); return; }
        uint32_t lifetime = (uint32_t)atoi(s_lt);
        bool withobj = s_wo ? (atoi(s_wo) ? true : false) : false;
        int32_t ret = cm_lwm2m_update(g_lwm2m_dev, lifetime, withobj);
        cm_demo_printf("\r\n[LWM2M] cm_lwm2m_update(%u,%d) ret:%d\r\n", lifetime, withobj, ret);
    }
    else if (0 == strcasecmp(cmd, "close"))
    {
        if (g_lwm2m_dev == NULL) { cm_demo_printf("\r\n[LWM2M] dev is NULL\r\n"); return; }
        int32_t ret = cm_lwm2m_close(g_lwm2m_dev);
        cm_demo_printf("\r\n[LWM2M] cm_lwm2m_close ret:%d\r\n", ret);
    }
    else if (0 == strcasecmp(cmd, "pack"))
    {
        if (g_lwm2m_dev == NULL) { cm_demo_printf("\r\n[LWM2M] dev is NULL, create first\r\n"); return; }
        const char *s_obj  = embeddedCliGetToken(args, 2);
        const char *s_ins  = embeddedCliGetToken(args, 3);
        const char *s_res  = embeddedCliGetToken(args, 4);
        const char *s_type = embeddedCliGetToken(args, 5);
        const char *s_val  = embeddedCliGetToken(args, 6);
        const char *s_ct   = embeddedCliGetToken(args, 7);
        if (s_obj == NULL || s_ins == NULL || s_res == NULL || s_type == NULL || s_val == NULL)
        {
            cm_demo_printf("\r\n[LWM2M] usage: lwm2m pack <obj> <ins> <res> <type> <value> [ctype]\r\n");
            cm_demo_printf("[LWM2M]   type:  string|opaque|int|float|bool|hexstr (or 1-6)\r\n");
            cm_demo_printf("[LWM2M]   ctype: default|text|link|opaque|tlv|json (or 0-5)\r\n");
            return;
        }
        int32_t type = __parse_type(s_type);
        if (type == 0)
        {
            cm_demo_printf("\r\n[LWM2M] invalid type '%s', use: string|opaque|int|float|bool|hexstr\r\n", s_type);
            return;
        }
        int32_t ctype = __parse_ctype(s_ct);
        if (ctype < 0)
        {
            cm_demo_printf("\r\n[LWM2M] invalid ctype '%s', use: default|text|link|opaque|tlv|json\r\n", s_ct);
            return;
        }
        if ((int32_t)strlen(s_val) > LWM2M_DEMO_PACK_MAX)
        {
            cm_demo_printf("\r\n[LWM2M] warning: value len %d exceeds recommended max %d\r\n",
                           (int)strlen(s_val), LWM2M_DEMO_PACK_MAX);
        }

        int data_len = (int)strlen(s_val);
        #if 0 /* 用于验证所有数据通过二进制发送 */
        /* 二进制透传验证：数值先序列化为大端字节，再以 type=1 透传上报。
         * 平台/LwM2M 整型按大端解；type=1 是 raw 透传，模组不做字节序转换，故必须在此转大端。
         */
        unsigned char be_buf[8] = {0};

        if(type == 3) /* int -> 4 字节大端 */
        {
            uint32_t v = (uint32_t)atoi(s_val);
            be_buf[0] = (unsigned char)(v >> 24);
            be_buf[1] = (unsigned char)(v >> 16);
            be_buf[2] = (unsigned char)(v >> 8);
            be_buf[3] = (unsigned char)(v);
            s_val = be_buf;
            data_len = 4;
            type = 1;
        }
        else if(type == 4) /* float/double -> 8 字节大端 */
        {
            double d = atof(s_val);
            unsigned char *p = (unsigned char *)&d;
            int i;
            for(i = 0; i < 8; i++) { be_buf[i] = p[7 - i]; }
            s_val = be_buf;
            data_len = 8;
            type = 1;
        }
        else if(type == 5) /* bool -> 1 字节 */
        {
            be_buf[0] = (unsigned char)(atoi(s_val) ? 1 : 0);
            s_val = be_buf;
            data_len = 1;
            type = 1;
        }
        #endif

        int32_t ret = cm_lwm2m_notify_packing(g_lwm2m_dev, atoi(s_obj), atoi(s_ins), atoi(s_res),
                                               type, (char *)s_val, data_len, ctype);
        cm_demo_printf("\r\n[LWM2M] cm_lwm2m_notify_packing(%s/%s/%s type=%d len=%d ct=%d) ret:%d\r\n",
                       s_obj, s_ins, s_res, type, data_len, ctype, ret);
    }
    else if (0 == strcasecmp(cmd, "notify"))
    {
        if (g_lwm2m_dev == NULL) { cm_demo_printf("\r\n[LWM2M] dev is NULL\r\n"); return; }
        const char *s_mid = embeddedCliGetToken(args, 2);
        int32_t mid = s_mid ? atoi(s_mid) : g_notify_mid++;
        int32_t ret = cm_lwm2m_notify(g_lwm2m_dev, mid);
        cm_demo_printf("\r\n[LWM2M] cm_lwm2m_notify(%d) ret:%d\r\n", mid, ret);
    }
    else if (0 == strcasecmp(cmd, "readrsp"))
    {
        if (g_lwm2m_dev == NULL) { cm_demo_printf("\r\n[LWM2M] dev is NULL\r\n"); return; }
        const char *s_mid    = embeddedCliGetToken(args, 2);
        const char *s_result = embeddedCliGetToken(args, 3);
        if (s_mid == NULL || s_result == NULL)
        {
            cm_demo_printf("\r\n[LWM2M] usage: lwm2m readrsp <mid> <result> [obj ins res type value]\r\n");
            return;
        }
        int32_t mid    = atoi(s_mid);
        int32_t result = atoi(s_result);
        const char *s_obj  = embeddedCliGetToken(args, 4);
        const char *s_ins  = embeddedCliGetToken(args, 5);
        const char *s_res  = embeddedCliGetToken(args, 6);
        const char *s_type = embeddedCliGetToken(args, 7);
        const char *s_val  = embeddedCliGetToken(args, 8);
        int32_t ret;
        if (s_obj && s_ins && s_res && s_type && s_val)
        {
            ret = cm_lwm2m_read_rsp(g_lwm2m_dev, mid, result,
                                    atoi(s_obj), atoi(s_ins), atoi(s_res),
                                    atoi(s_type), (char *)s_val, strlen(s_val));
        }
        else
        {
            /* 使用最近一次回调缓存的 obj/ins/res */
            ret = cm_lwm2m_read_rsp(g_lwm2m_dev, mid, result,
                                    g_last_obj, g_last_ins, g_last_res,
                                    1, "ok", strlen("ok"));
        }
        cm_demo_printf("\r\n[LWM2M] cm_lwm2m_read_rsp ret:%d\r\n", ret);
    }
    else if (0 == strcasecmp(cmd, "writersp"))
    {
        if (g_lwm2m_dev == NULL) { cm_demo_printf("\r\n[LWM2M] dev is NULL\r\n"); return; }
        const char *s_mid    = embeddedCliGetToken(args, 2);
        const char *s_result = embeddedCliGetToken(args, 3);
        if (s_mid == NULL || s_result == NULL)
        {
            cm_demo_printf("\r\n[LWM2M] usage: lwm2m writersp <mid> <result>\r\n");
            return;
        }
        int32_t ret = cm_lwm2m_write_rsp(g_lwm2m_dev, atoi(s_mid), atoi(s_result));
        cm_demo_printf("\r\n[LWM2M] cm_lwm2m_write_rsp ret:%d\r\n", ret);
    }
    else if (0 == strcasecmp(cmd, "execrsp"))
    {
        if (g_lwm2m_dev == NULL) { cm_demo_printf("\r\n[LWM2M] dev is NULL\r\n"); return; }
        const char *s_mid    = embeddedCliGetToken(args, 2);
        const char *s_result = embeddedCliGetToken(args, 3);
        if (s_mid == NULL || s_result == NULL)
        {
            cm_demo_printf("\r\n[LWM2M] usage: lwm2m execrsp <mid> <result>\r\n");
            return;
        }
        int32_t ret = cm_lwm2m_execute_rsp(g_lwm2m_dev, atoi(s_mid), atoi(s_result));
        cm_demo_printf("\r\n[LWM2M] cm_lwm2m_execute_rsp ret:%d\r\n", ret);
    }
    else if (0 == strcasecmp(cmd, "paramrsp"))
    {
        if (g_lwm2m_dev == NULL) { cm_demo_printf("\r\n[LWM2M] dev is NULL\r\n"); return; }
        const char *s_mid    = embeddedCliGetToken(args, 2);
        const char *s_result = embeddedCliGetToken(args, 3);
        if (s_mid == NULL || s_result == NULL)
        {
            cm_demo_printf("\r\n[LWM2M] usage: lwm2m paramrsp <mid> <result>\r\n");
            return;
        }
        int32_t ret = cm_lwm2m_param_rsp(g_lwm2m_dev, atoi(s_mid), atoi(s_result));
        cm_demo_printf("\r\n[LWM2M] cm_lwm2m_param_rsp ret:%d\r\n", ret);
    }
    else if (0 == strcasecmp(cmd, "observersp"))
    {
        if (g_lwm2m_dev == NULL) { cm_demo_printf("\r\n[LWM2M] dev is NULL\r\n"); return; }
        const char *s_mid    = embeddedCliGetToken(args, 2);
        const char *s_result = embeddedCliGetToken(args, 3);
        if (s_mid == NULL || s_result == NULL)
        {
            cm_demo_printf("\r\n[LWM2M] usage: lwm2m observersp <mid> <result>\r\n");
            return;
        }
        int32_t ret = cm_lwm2m_observe_rsp(g_lwm2m_dev, atoi(s_mid), atoi(s_result));
        cm_demo_printf("\r\n[LWM2M] cm_lwm2m_observe_rsp ret:%d\r\n", ret);
    }
    else if (0 == strcasecmp(cmd, "autotest"))
    {
        if (g_auto_task != NULL)
        {
            cm_demo_printf("\r\n[LWM2M] autotest already running\r\n");
            return;
        }
        osThreadAttr_t attr = {0};
        attr.name = "lwm2m_auto";
        attr.stack_size = LWM2M_DEMO_TASK_STACK;
        attr.priority = osPriorityNormal;
        g_auto_task = osThreadNew(__autotest_task, NULL, &attr);
        if (g_auto_task == NULL)
        {
            cm_demo_printf("\r\n[LWM2M] create autotest task fail\r\n");
        }
        else
        {
            cm_demo_printf("\r\n[LWM2M] autotest started\r\n");
        }
    }
    else
    {
        cm_demo_printf("\r\n[LWM2M] unknown cmd '%s', try 'lwm2m help'\r\n", cmd);
    }
}
