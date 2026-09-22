/**
 * @file        cm_modem.h
 * @brief       Modem接口
 * @copyright   Copyright © 2026 China Mobile IOT. All rights reserved.
 * @author      By CMIOT
 * @date        2026/01/30
 *
 * @defgroup modem modem
 * @ingroup DS
 * @{
 */

#ifndef __CM_MODEM_H__
#define __CM_MODEM_H__

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define CM_SYS_MAX_APN_LEN  100
#define CM_SYS_MAX_USERID_LEN  200
#define CM_SYS_MAX_PASSWORD_LEN  200

/****************************************************************************
 * Public Types
 ****************************************************************************/

 /* 网络制式选项 */
typedef enum
{
    CM_NETWORK_FORMAT_GSM = 1,
    CM_NETWORK_FORMAT_WCDMA,
    CM_NETWORK_FORMAT_TDSCDMA,
    CM_NETWORK_FORMAT_LTE,
    CM_NETWORK_FORMAT_eMTC,
    CM_NETWORK_FORMAT_NBIOT,
    CM_NETWORK_FORMAT_CDMA,
    CM_NETWORK_FORMAT_NR,

    CM_NETWORK_FORMAT_MAX,
}cm_network_format_e;

/** 选择 PLMN */
typedef struct
{
    uint8_t mode;         /** 网络选择模式，取值范围：0-4
                             0: 自动选择
                             1: 手动选择
                             2: 手动选择并注册
                             3: 自动选择并注册
                             4: 自动选择，带PLMN列表 */
    uint8_t format;       /** 运营商信息格式，取值范围：0-2
                             0: long alphanumeric <MCC><MNC><SPC>
                             1: short alphanumeric <MCC><MNC>
                             2: numeric <MCC><MNC> */
    uint8_t oper[21];     /** 运营商信息，格式根据format字段决定 */
    uint8_t act;          /** 网络接入技术，取值范围：7,11,12
                             7:  E-UTRAN
                             11: NR connected to a 5GCN 
                             12: NG-RAN*/
} cm_cops_info_t;

/** 无线信息 */
typedef struct 
{
    uint8_t  rat;            /**无线接入模式,详见cm_network_format_e */
    uint8_t sband;           /**当前频段 */
    int32_t rsrp;            /**信号接收功率,单位0.1dBm,无效值-32768 */
    int32_t rsrq;            /**信号接收质量，单位0.1dB,无效值-32768 */
    int32_t rssi;            /**信号强度指示，单位0.1dB,无效值-32768 */
    uint16_t rxlev;          /**信号接收电平,0~63,99无效 */
    int32_t tx_power;        /**最近一次发射功率，单位0.1dBm,无效值-32768 */
    int32_t tx_time;         /**上行累计发送时长，单位ms，无效值0 */
    int32_t rx_time;         /**下行累计接收时长，单位ms，无效值0 */
    uint64_t last_cellid;    /**上一次SB1服务小区ID,NR制式有效范围：0~0xFFFFFFFFFFFFFFFF；无效值：0xFFFFFFFFFFFFFFFF；LTE制式有效值范围：0~0xFFFFFFE；无效值0xFFFFFFFF */
    uint8_t  last_ecl;       /**上一次ECL值,有效取值0、1、2，0为普通覆盖，1、2为增强覆盖；无效值255 */
    int32_t last_snr;        /**上一次信噪比，单位0.1dB，无效值-32768 */
    uint32_t last_earfcn;    /**上一次绝对射频频道编号，有效值范围1~0xFFFFFFFF，无效值为0 */
    uint16_t last_pci;       /**上一次小区物理ID,无效值为65535 */
} cm_radio_info_t;

/** 小区信息 */
typedef struct 
{
    uint8_t  rat;            /**无线接入模式,详见cm_network_format_e */
    bool     primary_cell;   /**是否为当前驻留小区 */
    uint8_t  mcc[4];         /**移动国家代码 */
    uint8_t  mnc[4];         /**移动网络代码 */
    uint32_t earfcn;         /**绝对射频频道编号，有效值范围1~0xFFFFFFFF，无效值为0 */
    uint8_t  earfcn_offset;  /**绝对射频频偏,取值范围：0~4，无效值为255 */
    uint16_t pci;            /**小区物理ID,无效值为65535 */
    int32_t rsrp;            /**信号接收功率,单位0.1dBm,无效值-32768 */
    int32_t rsrq;            /**信号接收质量,单位0.1dBm,无效值-32768 */
    int32_t rssi;            /**信号强度指示,单位0.1dBm,无效值-32768 */
    int32_t snr;             /**信噪比,单位0.1dBm,无效值-32768 */
    uint32_t bandwidth;      /**带宽 */
} cm_cell_info_t;

/* 误块率信息 */
typedef struct
{
    uint8_t rlc_ul_bler;                       /**RLC层上行误块率，单位0.01 */
    uint8_t rlc_dl_bler;                       /**RLC层下行误块率，单位0.01 */
    uint8_t mac_ul_bler;                       /**物理层上行误块率，单位0.01 */
    uint8_t mac_dl_bler;                       /**物理层下行误块率，单位0.01 */
    uint64_t total_bytes_transmitted;          /**传输的总字节数 */
    uint64_t total_bytes_received;             /**接收的总字节数 */
    uint64_t transport_blocks_sent;            /**发送的传输块 */
    uint64_t transport_blocks_received;        /**接收的传输块 */
    uint64_t transport_blocks_retransmitted;   /**重传的传输块 */
    uint64_t total_acknack_received;           /**接收的总ACK/NACK消息数 */
}cm_bler_info_t;

/* 吞吐量信息 */
typedef struct
{
    uint32_t rlc_ul;    /**RLC层上行吞吐量，单位bps */
    uint32_t rlc_dl;    /**RLC层下行吞吐量，单位bps */
    uint32_t mac_ul;    /**物理层上行吞吐量，单位bps */
    uint32_t mac_dl;    /**物理层下行吞吐量，单位bps */
}cm_thp_info_t;

/* 锁频参数 */
typedef struct
{
    uint16_t rat;     /**无线接入模式,详见cm_network_format_e */
    uint32_t earfcn;  /**要搜索的EARFCN */
    uint16_t pci;     /**物理小区ID,无效值为65535*/
}cm_modem_freq_t;

/* PDP类型选项 */
typedef enum
{
    CM_PDP_TYPE_IP,
    CM_PDP_TYPE_IPV6,
    CM_PDP_TYPE_IPV4V6,
    //CM_PDP_TYPE_PPP,   //mod by cmiot
    // CM_PDP_TYPE_NONIP, //mod by cmiot
    
    CM_PDP_TYPE_MAX,
}cm_pdptype_e;

/* apn设置 */
typedef struct
{
    uint8_t pdp_type;                  /**pdp类型,cm_pdptype_e类型 */
    uint8_t apn[CM_SYS_MAX_APN_LEN+1]; /**apn,最长100字节 */
}cm_default_apn_t;

/* 自动联网设置 */
typedef struct
{
    bool enable;  /**使能 */
    uint8_t cid;  /**cid */
}cm_auto_connect_t;

/* ims域使能 */
typedef struct
{
    bool lte_ims_enable;  /**LTE */
    bool nr_ims_enable;   /**NR */
}cm_ims_enable_t;

/* 高精度授时使能结构体 */
typedef struct
{
    bool htp_enable;         /**使能 */
    uint32_t hpt_certainty;  /**精度 */
}cm_htp_enable_t;

/* 联网上报信息 */
typedef struct
{
    uint8_t cid;      /**cid */
    uint8_t status;   /**状态，0 断开连接；1 建立连接 */
    uint8_t ip[21];   /**ipv4地址 */
    uint8_t ipv6[41]; /**ipv6地址 */
}cm_call_info_t;

/* 联网状态回调 */
typedef void (*cm_modem_call_cb)(cm_call_info_t *info);

/** EDRX set配置 (AT+CEDRXS命令)*/
typedef struct
{
    uint8_t mode;       /** EDRX配置模式，取值范围：0-1
                             0: Disable EDRX
                             1: Enable EDRX with requested eDRX value*/
    uint8_t act_type;   /** 网络访问技术，取值范围：0-9
                             0: Access technology is not using eDRX
                             1: EC-GSM-IoT (A/Gb mode)
                             2: GSM (A/Gb mode)
                             3: UTRAN (Iu mode)
                             4: E-UTRAN (WB-S1 mode)
                             5: E-UTRAN (NB-S1 mode)
                             6: satellite E-UTRAN (NB-S1 mode)
                             7: satellite E-UTRAN (WB-S1 mode)
                             8: NG-RAN (N1 mode)
                             9: satellite NG-RAN (N1 mode) */
    uint8_t requested_edrx_value; /** 要配置的EDRX值，具体值对应的时间周期请参考AT+CEDRXS命令 */
} cm_edrx_cfg_set_t;

/** EDRX get配置 (AT+CEDRXS命令)*/
typedef struct
{
    uint8_t act_type;   /** 网络访问技术，取值范围：0-9
                             0: Access technology is not using eDRX
                             1: EC-GSM-IoT (A/Gb mode)
                             2: GSM (A/Gb mode)
                             3: UTRAN (Iu mode)
                             4: E-UTRAN (WB-S1 mode)
                             5: E-UTRAN (NB-S1 mode)
                             6: satellite E-UTRAN (NB-S1 mode)
                             7: satellite E-UTRAN (WB-S1 mode)
                             8: NG-RAN (N1 mode)
                             9: satellite NG-RAN (N1 mode) */
    uint8_t requested_edrx_value; /** 要配置的EDRX值，具体值对应的时间周期请参考AT+CEDRXS命令 */
} cm_edrx_cfg_get_t;


/** PSM配置(AT+CPSMS命令)*/
typedef struct
{
    uint8_t mode;    /** 模式，取值范围：0-2
                        0: 禁用PSM
                        1: 启用PSM
                        2: 禁用PSM，且设置指令中其他参数值无效，如可恢复则将其他参数恢复出厂值 */
    uint8_t requested_periodic_tau; /** 请求的扩展周期性TAU值（T3412），取值范围：一个字节（8位）
                                       编码格式为GPRS Timer 3信息元素的位格式
                                       例如："01000111" 等于 70 小时
                                       具体编码和取值范围见AT+CPSMS命令 */
    uint8_t requested_active_time;  /** 请求的活动时间值（T3324），取值范围：一个字节（8位）
                                       编码格式为GPRS Timer 2信息元素的位格式
                                       例如："00100100" 等于 4 分钟
                                       具体编码和取值范围见AT+CPSMS命令 */
} cm_psm_cfg_t;

/** EPS 域注册状态 (AT+CEREG命令) */
typedef struct
{
    uint8_t n;            /** 模式，取值范围：0-5
                             0: 禁用网络注册非请求结果代码
                             1: 启用网络注册非请求结果代码 +CEREG: <stat>
                             2: 启用网络注册和位置信息非请求结果代码 +CEREG: <stat>[,[<tac>],[<ci>],[<AcT>]]
                             3: 启用网络注册、位置信息和EMM原因值非请求结果代码 +CEREG: <stat>[,[<tac>],[<ci>],[<AcT>][,<cause_type>,<reject_cause>]]
                             4: 对需要应用PSM的UE，启用网络注册和位置信息非请求结果代码 +CEREG: <stat>[,[<tac>],[<ci>],[<AcT>][,<Active_Time>,<Periodic_TAU>]]
                             5: 对需要应用PSM的UE，启用网络注册、位置信息和EMM原因值信息非请求结果代码 +CEREG: <stat>[,[<tac>],[<ci>],[<AcT>][,<cause_type>,<reject_cause>][,<Active_Time>,<Periodic_TAU>]] */
    uint8_t state;        /** 网络注册状态，取值范围：0-11
                             0: 未注册，设备未搜索网络
                             1: 已注册，主网络
                             2: 未注册，但设备正在尝试附着或搜索网络以注册
                             3: 注册被拒绝
                             4: 未知（例如，不在E-UTRAN覆盖范围内）
                             5: 已注册，漫游
                             6: 已注册仅用于“SMS only”，主网络（不适用）
                             7: 已注册仅用于“SMS only”，漫游（不适用）
                             8: 仅用于紧急承载服务的附着（参见NOTE 2）
                             9: 已注册且“CSFB不首选”，主网络（不适用）
                             10: 已注册且“CSFB不首选”，漫游（不适用）
                             11: 仅用于RLOS访问的附着（参见NOTE 2a）（仅当<AcT>指示E-UTRAN时适用） */
    uint16_t lac;         /** 位置区码 (Location Area Code)，取值范围：0-65535*/
    uint32_t ci;          /** 小区识别码 (Cell Identity)，取值范围：0-268435455，无效值：4294967295 */
    uint8_t act;          /** 网络访问技术，取值范围：7，11
                             7: E-UTRAN
                             11: NR-5GC
                             无效值：255 */
    uint8_t rac;          /** 路由区域编码 (Routing Area Code)，取值范围：0-255
                             无效值：255 */
    uint8_t cause_type;   /** reject_cause类型，取值范围：0-1
                             0: <reject_cause> 包含EMM原因值
                             1: <reject_cause> 包含制造商特定的原因
                             无效值：255 */
    uint8_t reject_cause; /** 注册失败原因，取值范围：0-255，无效值：255 */
    uint8_t active_time;  /** 请求的扩展周期性TAU值（T3412），取值范围：一个字节（8位）
                                       编码格式为GPRS Timer 3信息元素的位格式
                                       例如："01000111" 等于 70 小时
                                       具体编码和取值范围见AT+CPSMS命令 */
    uint8_t periodic_tau; /** 请求的活动时间值（T3324），取值范围：一个字节（8位）
                                       编码格式为GPRS Timer 2信息元素的位格式
                                       例如："00100100" 等于 4 分钟
                                       具体编码和取值范围见AT+CPSMS命令 */
} cm_cereg_state_t;


/** PS网络注册状态 (AT+CREG) */
typedef struct
{
    uint8_t n;            /** 模式，取值范围：0-3
                             0: 禁用网络注册非请求结果代码
                             1: 启用网络注册非请求结果代码 +CREG: <stat>
                             2: 启用网络注册和位置信息非请求结果代码 +CREG: <stat>[,[<lac>],[<ci>],[<AcT>]]
                             3: 启用网络注册、位置信息和原因值信息非请求结果代码 +CREG: <stat>[,[<lac>],[<ci>],[<AcT>][,<cause_type>,<reject_cause>]] */
    uint8_t state;        /** 网络注册状态，取值范围：0-11
                             0: 未注册，设备未搜索网络
                             1: 已注册，主网络
                             2: 未注册，但设备正在搜索新网络以注册
                             3: 注册被拒绝
                             4: 未知（例如，不在GERAN/UTRAN/E-UTRAN覆盖范围内）
                             5: 已注册，漫游
                             6: 已注册仅用于“SMS only”，主网络（仅当<AcT>指示E-UTRAN时适用）
                             7: 已注册仅用于“SMS only”，漫游（仅当<AcT>指示E-UTRAN时适用）
                             8: 仅用于紧急承载服务的附着（参见NOTE 2）（不适用）
                             9: 已注册且“CSFB不首选”，主网络（仅当<AcT>指示E-UTRAN时适用）
                             10: 已注册且“CSFB不首选”，漫游（仅当<AcT>指示E-UTRAN时适用）
                             11: 仅用于RLOS访问的附着（参见NOTE 2a）（仅当<AcT>指示E-UTRAN时适用） */
    uint16_t lac;         /** 位置区码 (Location Area Code)，取值范围：0-65535*/
    uint32_t ci;          /** 小区识别码 (Cell Identity)，取值范围：0-268435455，无效值：4294967295 */
    uint8_t act;          /** 网络访问技术，取值范围：7，11
                             7: E-UTRAN
                             11: NR-5GC
                             无效值：255 */
} cm_creg_state_t;

/** PS网络注册状态 (AT+CGREG) */
typedef struct
{
    uint8_t n;            /** 模式，取值范围：0-3
                             0: 禁用网络注册非请求结果代码
                             1: 启用网络注册非请求结果代码 +CGREG: <stat>
                             2: 启用网络注册和位置信息非请求结果代码 +CGREG: <stat>[,[<lac>],[<ci>],[<AcT>],[<rac>]]
                             3: 启用网络注册、位置信息和GMM原因值信息非请求结果代码 +CGREG: <stat>[,[<lac>],[<ci>],[<AcT>],[<rac>][,<cause_type>,<reject_cause>]] */
    uint8_t state;        /** 网络注册状态，取值范围：0-11
                             0: 未注册，设备未搜索网络
                             1: 已注册，主网络
                             2: 未注册，但设备正在尝试附着或搜索网络以注册
                             3: 注册被拒绝
                             4: 未知（例如，不在GERAN/UTRAN覆盖范围内）
                             5: 已注册，漫游
                             6: 已注册仅用于“SMS only”，主网络（不适用）
                             7: 已注册仅用于“SMS only”，漫游（不适用）
                             8: 仅用于紧急承载服务的附着（参见NOTE 2）（仅当<AcT>指示2,4,5,6时适用）
                             9: 已注册且“CSFB不首选”，主网络（不适用）
                             10: 已注册且“CSFB不首选”，漫游（不适用）
                             11: 仅用于RLOS访问的附着（参见NOTE 2a）（不适用） */
    uint16_t lac;         /** 位置区码 (Location Area Code)，取值范围：0-65535*/
    uint32_t ci;          /** 小区识别码 (Cell Identity)，取值范围：0-268435455，无效值：4294967295 */
    uint8_t act;          /** 网络访问技术，取值范围：7，11
                             7: E-UTRAN
                             11: NR-5GC
                             无效值：255 */
    uint8_t rac;          /** 路由区域编码 (Routing Area Code)，无效值：255 */
    uint8_t cause_type;   /** reject_cause类型，取值范围：0-1
                             0: <reject_cause> 包含GMM原因值，见3GPP TS 24.008 Annex G
                             1: <reject_cause> 包含制造商特定的原因
                             无效值：255 */
    uint8_t reject_cause; /** 注册失败原因，取值范围：0-255，无效值：255 */
} cm_cgreg_state_t;


/**5G PS网络注册状态 (AT+C5GREG)*/
typedef struct
{
    uint8_t n;                      /**模式*/
    uint8_t state;                  /**网络注册状态*/
    uint32_t lac;                   /**位置区码,取值范围：0~16777215, 无效值4294967295‌*/
    uint64_t ci;                    /**小区识别码,取值范围0~68719476735, 无效值1099511627775‌*/
    uint8_t act;                    /**网络访问技术,无效值255*/
    uint16_t allowed_nssai_len;     /**allowed_nssai字节数*/
    char allowed_nssai[200];        /**网络允许的切片信息,每个S-NSSAI使用":"区分,16进制字符串类型*/
    uint8_t cause_type;             /**reject_cause类型,无效值255*/
    uint8_t reject_cause;           /**注册失败原因,无效值255*/
} cm_c5greg_state_t;

/* PDP上下文的身份验证协议 */
typedef enum
{
    CM_AUTH_PROTOCOL_NONE,  //清除PDP上下文的身份验证协议
    CM_AUTH_PROTOCOL_PAP,   //PAP 密码验证协议
    CM_AUTH_PROTOCOL_CHAP,  //CHAP 询问握手认证协议
    CM_AUTH_PROTOCOL_MAX,   //不配置PDP上下文的身份验证协议
}cm_auth_protocol_e;

/* pdp上下文配置 */
typedef struct
{
    uint8_t pdp_type;                  /**pdp类型,cm_pdptype_e类型 */
    uint8_t apn[CM_SYS_MAX_APN_LEN+1]; /**apn,最长100字节 */
    uint8_t auth_prot;                 /**PDP上下文的身份验证协议,cm_auth_protocol_e类型  ,如无需配置auth_prot\userid\password,可将auth_prot配置为CM_AUTH_PROTOCOL_MAX*/
    uint8_t userid[16+1]; /**用于访问网络所需身份验证的用户名,最长16字节 */
    uint8_t password[16+1]; /**用于访问网络所需身份验证的密码,最长16字节 */
}cm_pdp_context_t;

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

/**
 * @brief 获取产品型号
 * 
 * @param [out] cgmm 产品型号，长度64字节 
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 * 
 * @details More details
 */
int32_t cm_modem_get_cgmm(char *cgmm);

/**
 * @brief 获取生产商信息
 * 
 * @param [out] cgmi 生产商信息，长度64字节 
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 * 
 * @details More details
 */
int32_t cm_modem_get_cgmi(char *cgmi);

/**
 * @brief 获取运营商信息
 * 
 * @param [out] cops 运营商PLMN信息
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 * 
 * @details More details
 */
int32_t cm_modem_get_cops(cm_cops_info_t *cops);

/**
 * @brief 获取模组信号强度
 *
 * @param [out] rssi 接收信号强度指示，范围：0~31,99
 * @param [out] ber  误码率，范围：0~7,99，比特误码率百分比，99：未知或不可测
 *
 * @return  
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *
 * @details 
 */
int32_t cm_modem_get_csq(char *rssi,char *ber);

/**
 * @brief 获取模组无线信息
 * 
 * @param [out] radio_info 无线信息
 * 
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
int32_t cm_modem_get_radio_info(cm_radio_info_t *radio_info);

/**
 * @brief 获取模组小区信息
 * 
 * @param [out] cell_info[] 小区信息数组
 * @param [in] cell_info_num 欲获取小区信息个数，参考值最大为16
 * 
 * @return 
 *   >= 0  - 实际获取到的小区信息个数 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
int32_t cm_modem_get_cell_info(cm_cell_info_t cell_info[], uint16_t cell_info_num);

/**
 * @brief 获取模组误码率信息
 * 
 * @param [out] bler_info 误码率信息
 * 
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
int32_t cm_modem_get_bler_info(cm_bler_info_t *bler_info);

/**
 * @brief 获取模组吞吐量信息
 * 
 * @param [out] thp_info 吞吐量信息
 * 
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
int32_t cm_modem_get_thp_info(cm_thp_info_t *thp_info);

/**
 * @brief 锁定/解除锁定搜索频率
 * 
 * @param [in] mode 模式
 *                  0 解除锁频
 *                  1 锁定指定频点
 *                  2 设置优先搜索频点
 *                  其他 保留
 * @param [in] size 锁定频点个数，mode=0时，设置为0，mode=1时,仅支持锁定1个频点及小区
 * @param [in] freq 频点，mode=0时，设置为NULL
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
int32_t cm_modem_set_lock_freq(uint8_t mode, uint32_t size, cm_modem_freq_t *freq);

/**
 * @brief 获取当前锁定频率
 * 
 * @param [out] mode 模式
 *                  0 未锁频
 *                  1 已锁定指定频点
 *                  2 已设置优先搜索频点
 *                  其他 保留
 * @param [out] size 锁定频点个数，mode=0时，为0
 * @param [out] freq 频点，mode=0时，为NULL
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details freq需传入空指针，内部分配，外部调用后需主动释放
 */
int32_t cm_modem_get_lock_freq(uint8_t *mode, uint32_t *size, cm_modem_freq_t **freq);

/**
 * @brief 清除存储频点
 * 
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
int32_t cm_modem_clean_earfcn(void);

/**
 * @brief 设置band列表
 * 
 * @param [in] format 制式，详见cm_network_format_e
 * @param [in] size band个数
 * @param [in] band band列表
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details 设置需要支持的band列表，设置值需在模组支持范围内
 */
int32_t cm_modem_set_band(uint8_t format, uint8_t size, uint8_t *band);

/**
 * @brief 获取band列表
 * 
 * @param [in] format 制式，详见cm_network_format_e
 * @param [out] size band个数
 * @param [out] band band列表
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details band需传入空指针，内部分配，外部调用后需主动释放
 */
int32_t cm_modem_get_band(uint8_t format, uint8_t *size, uint8_t **band);

/**
 * @brief 设置制式及优先级
 * 
 * @param [in] size 制式个数
 * @param [in] format 制式列表，详见cm_network_format_e
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details 设置需要支持的制式列表，设置值需在模组支持范围内，format取值范围：CM_NETWORK_FORMAT_LTE,CM_NETWORK_FORMAT_NR
 */
int32_t cm_modem_set_network_format(uint8_t size, uint8_t *format);

/**
 * @brief 获取制式及优先级
 * 
 * @param [out] size 制式个数
 * @param [out] format 制式列表，详见cm_network_format_e
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details format传入缓存长度需不小于CM_NETWORK_FORMAT_MAX
 */
int32_t cm_modem_get_network_format(uint8_t *size, uint8_t *format);

/**
 * @brief 设置默认APN
 * 
 * @param [in] apn 
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
int32_t cm_modem_set_default_apn(cm_default_apn_t *apn);

/**
 * @brief 获取默认APN
 * 
 * @param [out] apn 长度不超过38
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
int32_t cm_modem_get_default_apn(cm_default_apn_t *apn);

/**
 * @brief 设置自动联网功能
 * 
 * @param [in] mode 配置参数
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
int32_t cm_modem_set_auto_connect(cm_auto_connect_t *mode);

/**
 * @brief 获取自动联网配置
 * 
 * @param [out] mode 配置参数
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
int32_t cm_modem_get_auto_connect(cm_auto_connect_t *mode);

/**
 * @brief 设置IMS域使能
 * 
 * @param [in] mode 配置参数
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
int32_t cm_modem_set_ims_enable(cm_ims_enable_t mode);

/**
 * @brief 获取IMS域使能配置
 * 
 * @param [out] mode 配置参数
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
int32_t cm_modem_get_ims_enable(cm_ims_enable_t *mode);

/**
 * @brief 设置高精度授时使能
 * 
 * @param [in] mode 配置参数
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
// int32_t cm_modem_set_htp_enable(cm_htp_enable_t mode);

/**
 * @brief 获取高精度授时使能配置
 * 
 * @param [out] mode 配置参数
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
// int32_t cm_modem_get_htp_enable(cm_htp_enable_t *mode);

/**
 * @brief 设置协议版本
 * 
 * @param [in] ver 协议版本
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
int32_t cm_modem_set_release_ver(uint32_t ver);

/**
 * @brief 获取协议版本
 * 
 * @param [out] ver 协议版本
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
int32_t cm_modem_get_release_ver(uint32_t *ver);

/**
 * @brief 设置联网状态回调
 * 
 * @param [in] callback 回调函数
 *
 * @return 
 *  
 * @details 手动、自动联网均上报至该回调
 */
void cm_modem_set_call_callback(cm_modem_call_cb callback);

/**
 * @brief 建立模组数据连接
 * 
 * @param [in] mode 操作类型
 *                  0 断开连接
 *                  1 建立连接
 * @param [in] cid cid
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details 需保留一路为激活状态
 */
int32_t cm_modem_call(uint8_t mode, uint8_t cid);

/**
 * @brief 获取模组数据连接信息
 * 
 * @param [in] cid cid
 * @param [out] info 连接信息指针
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
int32_t cm_modem_get_call_status(uint8_t cid, cm_call_info_t *info);

/**
 * @brief 设置模组功能模式
 * 
 * @param [in] fun 功能模式代码，参考AT指令文档AT+CFUN说明
 * 
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 * 
 * @details More details
 */
int32_t cm_modem_set_cfun(uint16_t fun);

/**
 * @brief 获取模组功能模式
 * 
 * @return 
 *   >= 0  - 功能模式代码，参考AT指令文档AT+CFUN说明 \n
 *   < 0  - 失败, 返回值为错误码
 * 
 * @details More details
 */
int32_t cm_modem_get_cfun(void);

/**
 * @brief 设置模组EDRX
 * 
 * @param [in] cfg EDRX配置
 * 
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 * 
 * @details More details
 */
int32_t cm_modem_set_edrx_cfg(const cm_edrx_cfg_set_t *cfg);

/**
 * @brief 获取模组EDRX设置
 * 
 * @param [out] cfg EDRX配置
 * 
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 * 
 * @details More details
 */
int32_t cm_modem_get_edrx_cfg(cm_edrx_cfg_get_t *cfg);

/**
 * @brief 配置模组PSM
 * 
 * @param [in] cfg PSM配置
 * 
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 * 
 * @details More details
 */
int32_t cm_modem_set_psm_cfg(const cm_psm_cfg_t* cfg);

/**
 * @brief 获取模组PSM配置
 * 
 * @param [out] cfg PSM配置
 * 
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 * 
 * @details More details
 */
int32_t cm_modem_get_psm_cfg(cm_psm_cfg_t *cfg);

/**
 * @brief 获取PS网络注册状态
 * 
 * @param [out] creg CREG状态
 * 
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 * 
 * @details More details
 */
int32_t cm_modem_get_creg_state(cm_creg_state_t *creg);

/**
 * @brief 获取PS网络注册状态
 * 
 * @param [out] cereg CEREG状态
 * 
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 * 
 * @details More details
 */
int32_t cm_modem_get_cereg_state(cm_cereg_state_t *cereg);

/**
 * @brief 获取PS网络注册状态
 * 
 * @param [out] cgreg CGREG状态
 * 
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 * 
 * @details More details
 */
int32_t cm_modem_get_cgreg_state(cm_cgreg_state_t *cgreg);



/**
 * @brief 获取RRC连接状态
 * 
 * @return 
 *   >= 0  - RRC连接状态，参考AT文档AT+CSCON说明 \n
 *   < 0  - 失败, 返回值为错误码 
 * 
 * @details More details
 */
int32_t cm_modem_get_cscon(void);

/**
 * @brief 设置PDP上下文
 * 
 * @param [in] cid PDP上下文编号
 *
 * @param [in] pdp_context PDP上下文配置
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
int32_t cm_modem_set_pdp_context(uint16_t cid, cm_pdp_context_t *pdp_context);

/**
 * @brief 获取PDP上下文
 * 
 * @param [in] cid PDP上下文编号
 *
 * @param [out] pdp_context PDP上下文配置
 *
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 *  
 * @details More details
 */
int32_t cm_modem_get_pdp_context(uint16_t cid, cm_pdp_context_t *pdp_context);

/**
 * @brief 激活PDP
 * 
 * @param [in] cid PDP上下文编号,取值范围1~15,当IMS配置为激活时,将占用cid 8,若需去激活cid 8需先调用cm_modem_set_ims_enable去激活，再执行cm_modem_activate_pdp去激活; 激活IMS请使用cm_modem_activate_pdp。
 * 
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码 
 * 
 * @details 仅激活PDP上下文，cm_modem_activate_pdp不能与cm_modem_call、cm_dialup混用，建立模组数据连接建议使用cm_modem_call，上位机拨号建议使用cm_dialup
 */
int32_t cm_modem_activate_pdp(uint16_t cid);

/**
 * @brief 关闭PDP
 * 
 * @param [in] cid PDP上下文编号
 * 
 * @return 
 *   = 0  - 成功 \n
 *   < 0  - 失败, 返回值为错误码
 * 
 * @details 
 *   当仅剩一路已激活的PDP上下文时，不执行去激活操作
 */
int32_t cm_modem_deactivate_pdp(uint16_t cid);

/**
 * @brief 查询PDP状态
 * 
 * @param [in] cid PDP上下文编号
 * 
 * @return 
 *   >= 0  - PDP状态，参考AT指令文档AT+CGACT说明 \n
 *   < 0  - 失败, 返回值为错误码
 * 
 * @details More details
 */
int32_t cm_modem_get_pdp_state(uint16_t cid);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __CM_MODEM_H__ */

/** @}*/
