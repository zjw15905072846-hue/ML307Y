#include "cm_os.h"
#include "cm_mem.h"
#include "cm_sys.h"
#include "cm_demo_uart.h"
#include "cm_demo_modem.h"
#include "cm_modem.h"
#if 0
#include "cm_dialup.h"
#endif
#include "string.h"
#include "stdio.h"
#include <stdlib.h>

void __modem_call_cb(cm_call_info_t *info)
{
    if(info)
    {
        cm_demo_printf("__cm_mipcall_cb status:%d %d\n", info->cid, info->status);

        if(info->status)
        {
            if(info->ip[0] != '\0')
            {
                cm_demo_printf("__modem_call_cb ip:%s\n",  info->ip);
            }
            if(info->ipv6[0] != '\0')
            {
                cm_demo_printf("__modem_call_cb ipv6:%s\n",  info->ipv6);
            }
        }
    }
    else
    {
        cm_demo_printf("info NULL\n");
    }
}
#if 0
void __cm_dialup_cb(cm_dialup_info_t *info)
{
    cm_demo_printf("__cm_dialup_cb...");
    if (info)
    {
        cm_demo_printf("__cm_dialup_cb cid:%d status:%d type:%d\n", 
                        info->cid, info->status, info->type);
        
        if (info->type == 0) // IPv4
        {
            cm_demo_printf("IPv4: IP=%s GW=%s DNS1=%s DNS2=%s\n",
                            info->info.ipv4info.ip,
                            info->info.ipv4info.ip_gw,
                            info->info.ipv4info.ip_dns1,
                            info->info.ipv4info.ip_dns2);
        }
        else // IPv6
        {
            cm_demo_printf("IPv6: IP=%s GW=%s DNS1=%s DNS2=%s\n",
                            info->info.ipv6info.ipv6,
                            info->info.ipv6info.ipv6_gw,
                            info->info.ipv6info.ipv6_dns1,
                            info->info.ipv6info.ipv6_dns2);
        }
    }
    else
    {
        cm_demo_printf("Dialup callback info is NULL\n");
    }
}
#endif
/**
*  modem功能功能调试使用示例
*  modem info                           //获取常用modem信息
*  modem pdp_get                        //获取PDP1与PDP2信息
*  modem pdp_set                        //设置PDP2信息
*  modem call_get                       //获取第1路与第2路拨号连接信息
*  modem call                           //第2路拨号连接
*  modem discall                        //第2路断开拨号连接
*  modem dial                           //上位机拨号联网建立连接
*  modem hangup                         //上位机拨号联网断开连接
*  modem dial_get                       //获取上位机拨号联网信息
*/

void cm_test_modem(EmbeddedCli *cli, char *args, void *context)
{
    const char *cmd = embeddedCliGetToken(args, 1);
    const char *param = embeddedCliGetToken(args, 2);

    if (cmd == NULL)
    {
        cm_demo_printf("invalid param\n");
        return;
    }

    if (0 == strcasecmp((const char *)cmd, "info"))
    {
        char cgmm[10] = {0};
        cm_modem_get_cgmm(cgmm);
        cm_demo_printf("cgmm:%s\n",cgmm);

        char cgmi[10] = {0};
        cm_modem_get_cgmi(cgmi);
        cm_demo_printf("cgmi:%s\n",cgmi);

        cm_cops_info_t *cops = NULL;
        cops = cm_malloc(sizeof(cm_cops_info_t));
        memset(cops,0,sizeof(cm_cops_info_t));
        cm_modem_get_cops(cops);
        cm_demo_printf("cops->mode:%d\n", cops->mode);
        if(cops->mode)
        {
            cm_demo_printf("cops->act:%d cops->format:%d, cops->oper:%s\n",cops->act, cops->format, cops->oper);
        } 
        cm_free(cops);

        char rssi;
        char ber;
        cm_modem_get_csq(&rssi, &ber);
        cm_demo_printf("rssi:%d,ber:%d\n",rssi, ber);


        cm_radio_info_t *radio_info = NULL;
        radio_info = cm_malloc(sizeof(cm_radio_info_t));
        memset(radio_info,0,sizeof(cm_radio_info_t));
        cm_modem_get_radio_info(radio_info);
        cm_demo_printf("radio_info->last_cellid:%d,\n\
            radio_info->last_earfcn:%d,\n\
            radio_info->last_ecl:%d,\n\
            radio_info->last_pci:%d,\n\
            radio_info->last_snr:%d,\n\
            radio_info->rat:%d,\n\
            radio_info->rsrp:%d,\n\
            radio_info->rsrq:%d,\n\
            radio_info->rssi:%d,\n\
            radio_info->rx_time:%d,\n\
            radio_info->rxlev:%d,\n\
            radio_info->tx_power:%d\n",
                    radio_info->last_cellid,   
                    radio_info->last_earfcn,   
                    radio_info->last_ecl,
                    radio_info->last_pci,
                    radio_info->last_snr,
                    radio_info->rat,
                    radio_info->rsrp,
                    radio_info->rsrq,
                    radio_info->rssi,
                    radio_info->rx_time,
                    radio_info->rxlev,
                    radio_info->tx_power);
        cm_free(radio_info);


        cm_cell_info_t cell_info[3];
        int count = cm_modem_get_cell_info(cell_info,3);
        cm_demo_printf("find cell count:%d\n",count);
        cm_demo_printf("cell_info[0].bandwidth:%d,\n\
            cell_info[0].earfcn:%d,\n\
            cell_info[0].earfcn_offset:%d,\n\
            cell_info[0].mcc:%s,\n\
            cell_info[0].mnc:%s,\n\
            cell_info[0].pci:%d,\n\
            cell_info[0].primary_cell:%d,\n\
            cell_info[0].rsrp:%d,\n\
            cell_info[0].rsrq:%d,\n\
            cell_info[0].rssi:%d,\n\
            cell_info[0].snr:%d\n",
                    cell_info[0].bandwidth,   
                    cell_info[0].earfcn,   
                    cell_info[0].earfcn_offset,   
                    cell_info[0].mcc,   
                    cell_info[0].mnc,   
                    cell_info[0].pci,   
                    cell_info[0].primary_cell,   
                    cell_info[0].rsrp,   
                    cell_info[0].rsrq,   
                    cell_info[0].rssi,   
                    cell_info[0].snr);

        int get_fun = cm_modem_get_cfun();
        cm_demo_printf("get_fun:%d\n",get_fun);
#if 0
        cm_edrx_cfg_get_t *edrx_cfg;
        edrx_cfg = cm_malloc(sizeof(cm_edrx_cfg_get_t));
        memset(edrx_cfg,0,sizeof(cm_edrx_cfg_get_t));
        cm_modem_get_edrx_cfg(edrx_cfg);
        cm_demo_printf("edrx_cfg->act_type:%d,edrx_cfg->requested_edrx_value:%d\n",edrx_cfg->act_type, edrx_cfg->requested_edrx_value);
        cm_free(edrx_cfg);
#endif
        cm_psm_cfg_t *psm_cfg = NULL;
        psm_cfg = cm_malloc(sizeof(cm_psm_cfg_t));
        memset(psm_cfg,0,sizeof(cm_psm_cfg_t));
        cm_modem_get_psm_cfg(psm_cfg);
        cm_demo_printf("psm_cfg->mode:%d, psm_cfg->requested_active_time:%d, psm_cfg->requested_periodic_tau:%d\n",psm_cfg->mode, psm_cfg->requested_active_time, psm_cfg->requested_periodic_tau);
        cm_free(psm_cfg);


        cm_cereg_state_t *cereg;
        cereg = cm_malloc(sizeof(cm_cereg_state_t));
        memset(cereg,0,sizeof(cm_cereg_state_t));
        cm_modem_get_cereg_state(cereg);
        cm_demo_printf("cereg->act:%d, cereg->active_time:%d, cereg->cause_type:%d,cereg->ci:%.8x, cereg->lac:%d,cereg->n:%d,cereg->periodic_tau:%d,cereg->rac:%d,cereg->reject_cause:%d,cereg->state:%d\n",
                        cereg->act,    cereg->active_time,    cereg->cause_type,   cereg->ci,    cereg->lac,cereg->n,      cereg->periodic_tau,   cereg->rac,   cereg->reject_cause,   cereg->state);
        cm_free(cereg);

        int get_cscon = cm_modem_get_cscon();
        cm_demo_printf("get_cscon:%d\n",get_cscon);

    }  
    else if (0 == strcasecmp((const char *)cmd, "pdp_get"))
    {
        cm_pdp_context_t pdp_context = {};

        memset(&pdp_context, 0, sizeof(pdp_context));
        cm_modem_get_pdp_context(1, &pdp_context);
        cm_demo_printf("pdp1_context: %d %s %s %s\n",
            pdp_context.pdp_type,
            pdp_context.apn,
            pdp_context.userid,
            pdp_context.password);

        memset(&pdp_context, 0, sizeof(pdp_context));
        cm_modem_get_pdp_context(2, &pdp_context);
        cm_demo_printf("pdp2_context: %d %s %s %s\n",
            pdp_context.pdp_type,
            pdp_context.apn,
            pdp_context.userid,
            pdp_context.password);
    }
    else if (0 == strcasecmp((const char *)cmd, "pdp_set"))
    {
        cm_pdp_context_t pdp_context = {};

        memset(&pdp_context, 0, sizeof(pdp_context));
        pdp_context.pdp_type = CM_PDP_TYPE_IPV4V6;
        sprintf((void *)&pdp_context.apn, "cmiot");
        cm_modem_set_pdp_context(2, &pdp_context);

        cm_modem_get_pdp_context(2, &pdp_context);
        cm_demo_printf("pdp2_context: %d %s %s %s\n",
            pdp_context.pdp_type,
            pdp_context.apn,
            pdp_context.userid,
            pdp_context.password);
    }
    else if (0 == strcasecmp((const char *)cmd, "call_get"))
    {
        cm_call_info_t call_info = {};
        int ret = 0;

        memset(&call_info, 0, sizeof(call_info));
        ret = cm_modem_get_call_status(1, &call_info);
        cm_demo_printf("call_status:%d %d\n", ret, call_info.status);
        if((ret == 0) && (call_info.status == 1))
        {
            cm_demo_printf("ip:%s %s\n", call_info.ip, call_info.ipv6);
        }
        
        memset(&call_info, 0, sizeof(call_info));
        ret = cm_modem_get_call_status(2, &call_info);
        cm_demo_printf("call_status:%d %d\n", ret, call_info.status);
        if((ret == 0) && (call_info.status == 1))
        {
            cm_demo_printf("ip:%s %s\n", call_info.ip, call_info.ipv6);
        }
    }
    else if (0 == strcasecmp((const char *)cmd, "call"))
    {
        cm_modem_set_call_callback(__modem_call_cb);
        cm_modem_call(1, 2);
    }
    else if (0 == strcasecmp((const char *)cmd, "discall"))
    {
        cm_modem_set_call_callback(__modem_call_cb);
        cm_modem_call(0, 2);
    }
   
    else if (0 == strcmp((const char *)cmd, "get_auto_connect"))
    {
        cm_auto_connect_t get_conn;
        if(cm_modem_get_auto_connect(&get_conn) == 0)
        {
            cm_demo_printf("AutoConnect: enable=%d\n", get_conn.enable);
        }
    }
    else if (0 == strcmp((const char *)cmd, "set_lock_freq"))
    {
        // 测试频率锁定
        cm_modem_freq_t freq = {CM_NETWORK_FORMAT_LTE, 38930, 70};
        int ret = cm_modem_set_lock_freq(1, 1, &freq);
        cm_demo_printf("SetLockFreq: ret=%d\n", ret);
    }
    else if (0 == strcmp((const char *)cmd, "clr_lock_freq"))
    {
        // 测试频率锁定
        int ret = cm_modem_set_lock_freq(0, 0, NULL);
        cm_demo_printf("SetLockFreq: ret=%d\n", ret);
    }
    else if (0 == strcmp((const char *)cmd, "get_lock_freq"))
    {
        uint8_t mode = 0;
        uint32_t size = 0;
        cm_modem_freq_t *lock_freq = NULL;
        int ret = cm_modem_get_lock_freq(&mode, &size, &lock_freq);
        cm_demo_printf("GetLockFreq: ret=%d\n", ret);
        if((ret == 0) && lock_freq)
        {
            cm_demo_printf("LockFreq: mode=%d, rat=%d, earfcn=%d, pai=%d\n", 
                            mode, lock_freq->rat, lock_freq->earfcn, lock_freq->pci);
            cm_free(lock_freq);
        }
    }
    else if (0 == strcmp((const char *)cmd, "get_band"))
    {
        uint8_t size = 0;
        uint8_t *get_bands = NULL;
        int ret = cm_modem_get_band(CM_NETWORK_FORMAT_LTE, &size, &get_bands);
        cm_demo_printf("get_band: ret=%d\n", ret);
        if((ret == 0) && get_bands)
        {
            cm_demo_printf("Bands: %d bands configured\n", size);
            for(int i=0; i<size; i++)
            {
                cm_demo_printf("%d,", get_bands[i]);
            }
            cm_free(get_bands);
        }
    }
    else if (0 == strcmp((const char *)cmd, "set_band"))
    {
        // 测试Band设置
        uint8_t bands[] = {1, 3};
        int ret = cm_modem_set_band(CM_NETWORK_FORMAT_LTE, 2, bands);
        cm_demo_printf("set_band: ret=%d\n", ret);
    }
    else if (0 == strcmp((const char *)cmd, "clr_band"))
    {
        // 测试Band设置
        uint8_t bands = 0;
        int ret = cm_modem_set_band(CM_NETWORK_FORMAT_LTE, 1, &bands);
        cm_demo_printf("set_band: ret=%d\n", ret);
    }
    else if (0 == strcmp((const char *)cmd, "pdp_activate"))
    {
        uint8_t cid = 0;
        if (param)
        {
            cid = (uint8_t)atoi(param);
        }

        int ret = cm_modem_activate_pdp(cid);
        cm_demo_printf("PDP%d activated %d\n",cid,ret);    
    }
    else if (0 == strcmp((const char *)cmd, "pdp_deactivate"))
    {
        uint8_t cid = 0;
        if (param)
        {
            cid = (uint8_t)atoi(param);
        }

        int ret = cm_modem_deactivate_pdp(cid);
        cm_demo_printf("PDP%d deactivate %d\n", cid, ret);    
    }
    else if (0 == strcmp((const char *)cmd, "pdp_get_state"))
    {
        uint8_t cid = 0;
        if (param)
        {
            cid = (uint8_t)atoi(param);
        }

        int state = cm_modem_get_pdp_state(cid);
        cm_demo_printf("PDP%d State: %d\n", cid, state);
    }
    else if (0 == strcmp((const char *)cmd, "psm_set"))
    {
        // 设置PSM配置
        cm_psm_cfg_t psm_cfg = {1, 10, 20};
        if(cm_modem_set_psm_cfg(&psm_cfg) == 0)
        {
            cm_demo_printf("PSM set success\n");
        }
        else
        {
            cm_demo_printf("PSM set fail\n");
        }
    }
    else if (0 == strcmp((const char *)cmd, "edrx_set"))
    {
        // 设置EDRX配置
        cm_edrx_cfg_set_t edrx_cfg = {1, 4, 4};
        /*
            mode = 1                // 启用eDRX
            act_type = 5            // LTE-M: 4, NB-IoT: 5（常见）
            requested_edrx_value = 4 // eDRX值（具体见3GPP表）
        */

        if(cm_modem_set_edrx_cfg(&edrx_cfg) == 0)
        {
            cm_demo_printf("EDRX set success\n");
        }
        else
        {
            cm_demo_printf("EDRX set failed\n");
        }
    }
#if 0
    else if (0 == strcasecmp((const char *)cmd, "dial"))
    {
        cm_dialup_t dialup = {0};
        dialup.interface = CM_DIALUP_INTERFACE_USB;
        
        cm_dialup_set_callback(__cm_dialup_cb);
        int32_t ret = cm_dialup(1, 1, &dialup); // 默认CID=1
        cm_demo_printf("cm_dialup(connect):%d\n", ret);
    }
    else if (0 == strcasecmp((const char *)cmd, "hangup"))
    {
        cm_dialup_t dialup = {0};
        dialup.interface = CM_DIALUP_INTERFACE_USB;
        
        int32_t ret = cm_dialup(0, 1, &dialup); // 默认CID=1
        cm_demo_printf("cm_dialup(disconnect):%d\n", ret);
    }
    else if (0 == strcasecmp((const char *)cmd, "dial_get"))
    {
        cm_dialup_info_t info = {0};
        int32_t ret = cm_dialup_get_status(1, 0, &info); // 默认CID=1 IPv4״
        cm_demo_printf("cm_dialup_get_status:%d cid:%d status:%d\n", 
                      ret, info.cid, info.status);
        
        if (ret == 0 && info.status == 1)
        {
            if (info.type == 0)
            {
                cm_demo_printf("IP: %s\n", info.info.ipv4info.ip);
            }
        }
    }
#endif
    else
    {
        cm_demo_printf("Unknown command: %s\n", cmd);
    }
}
