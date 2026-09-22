/**
 * @file        cm_lwm2m.h
 * @brief       LWM2M接口
 * @copyright   Copyright © 2023 China Mobile IOT. All rights reserved.
 * @author      
 * @date        2023/07/19
 *
 * @defgroup lwm2m lwm2m
 * @ingroup NP
 * @{
 */


#ifndef __CM_LWM2M_H__
#define __CM_LWM2M_H__

#include <stdbool.h>
#include <stdint.h>

/**LwM2M实例句柄*/
typedef void *cm_lwm2m_handle_t;

/**LwM2M错误码*/
typedef enum {
    CM_LWM2M_SUCCESS        = 0,        /*!< 成功 */
    CM_LWM2M_UNKOWN         = 100,      /*!< 未知错误 */
    CM_LWM2M_PARA_ERROR     = 601,      /*!< 语法，句法错误 */
    CM_LWM2M_STATE_ERROR    = 602,      /*!< 设备未登录或设备登录中 */
    CM_LWM2M_NOT_SUPPORT    = 651,      /*!< 操作不被允许 */
    CM_LWM2M_FLOW_CONTROL   = 652,      /*!< Uplink Busy */
    CM_LWM2M_RESOURCE_FAIL  = 653,      /*!< 资源操作错误，申请dev时已存在，其他操作资源不存在 */
}cm_lwm2m_erroc_e;

/**LwM2M连接平台*/
typedef enum {
    CM_LWM2M_ONENET         = 0,        /*!< 中国移动OneNET平台 */
    CM_LWM2M_CTWING         = 1,        /*!< 中国电信CTWing平台 */
    CM_LWM2M_DMP            = 2,        /*!< 中国联通雁飞格物DMP平台 */
    CM_LWM2M_HUAWEIYUN      = 3,        /*!< 华为云物联网平台 */
    CM_LWM2M_OTHER          = 10,       /*!< 其他平台 */
}cm_lwm2m_platform_e;

/**LwM2M 事件*/
typedef enum {
    CM_LWM2M_EVENT_BASE                     = 0,
    CM_LWM2M_EVENT_BOOTSTRAP_START          = 1,
    CM_LWM2M_EVENT_BOOTSTRAP_SUCCESS        = 2,
    CM_LWM2M_EVENT_BOOTSTRAP_FAILED         = 3,
    CM_LWM2M_EVENT_CONNECT_SUCCESS          = 4,
    CM_LWM2M_EVENT_CONNECT_FAILED           = 5,
    CM_LWM2M_EVENT_REG_SUCCESS              = 6,
    CM_LWM2M_EVENT_REG_FAILED               = 7,
    CM_LWM2M_EVENT_REG_TIMEOUT              = 8,
    CM_LWM2M_EVENT_LIFETIME_TIMEOUT         = 9,
    CM_LWM2M_EVENT_STATUS_HALT              = 10,
    CM_LWM2M_EVENT_UPDATE_SUCCESS           = 11,
    CM_LWM2M_EVENT_UPDATE_FAILED            = 12,
    CM_LWM2M_EVENT_UPDATE_TIMEOUT           = 13,
    CM_LWM2M_EVENT_UNREG_DONE               = 15,
    CM_LWM2M_EVENT_UNREG_FAILED             = 16,
    CM_LWM2M_EVENT_RESPONSE_FAILED          = 20,
    CM_LWM2M_EVENT_RESPONSE_SUCCESS         = 21,
    CM_LWM2M_EVENT_NOTIFY_FAILED            = 25,
    CM_LWM2M_EVENT_NOTIFY_SUCCESS           = 26,
    CM_LWM2M_EVENT_NO_DEVICE                = 31,
    CM_LWM2M_EVENT_DTLS_NAT                 = 32,
    CM_LWM2M_EVENT_RECV_DROP                = 33,
}cm_lwm2m_event_e;

/**LwM2M操作结果码*/
typedef enum {
    CM_LWM2M_RESULT_000_ERROR               = 0,
    CM_LWM2M_RESULT_205_CONTENT             = 1,
    CM_LWM2M_RESULT_204_CHANGED             = 2,
    CM_LWM2M_RESULT_400_BADREQUEST          = 11,
    CM_LWM2M_RESULT_401_UNAUTHORIZED        = 12,
    CM_LWM2M_RESULT_404_NOTFOUND            = 13,
    CM_LWM2M_RESULT_405_METHODNOTALLOWED    = 14,
    CM_LWM2M_RESULT_406_NOTACCEPTABLE       = 15,
    CM_LWM2M_RESULT_231_CONTINUE            = 16,
    CM_LWM2M_RESULT_408_REQUESTINCOMPLTETE  = 17,
    CM_LWM2M_RESULT_413_REQUESTTOOLARGE     = 18,
    CM_LWM2M_RESULT_415_FORMATUNSUPPORTED   = 19,
}cm_lwm2m_result_e;

/** LwM2M回调函数参数结构体 */
typedef struct
{
    cm_lwm2m_handle_t handle;   /*!< LwM2M实例句柄 */
    void *cb_param;             /*!< 用户调用cm_lwm2m_create()接口时传入LwM2M回调函数用户参数（即cm_lwm2m_cfg_t结构体中的cb_param） */
} cm_lwm2m_cb_param_t;

/**
 *  @brief 状态事件上报回调函数，需用户实现
 *
 *  @param [out] event 状态事件类型，参照状态事件类型定义
 *  @param [out] param LwM2M回调函数参数结构体
 *
 *  @return 无
 *
 */
typedef void (*cm_lwm2m_event_cb_t)(int32_t event, cm_lwm2m_cb_param_t param);

/**
 *  @brief notify上报响应回调函数，需用户实现
 *
 *  @param [out] mid message id，应与cm_lwm2m_notify()调用时传入的mid一致
 *  @param [out] param LwM2M回调函数参数结构体
 *
 *  @return 无
 *
 */
typedef void (*cm_lwm2m_notify_cb_t)(int32_t mid, cm_lwm2m_cb_param_t param);

/**
 *  @brief 平台read操作回调函数，需用户实现
 *
 *  @param [out] mid 操作消息的ID
 *  @param [out] objid 被操作的object ID
 *  @param [out] insid 被操作的instance ID
 *  @param [out] resid 被操作的resource ID
 *  @param [out] param LwM2M回调函数参数结构体
 *
 *  @return 无
 *
 */
typedef void (*cm_lwm2m_read_cb_t)(int32_t mid, int32_t objid, int32_t insid, int32_t resid, cm_lwm2m_cb_param_t param);

/**
 *  @brief 平台write操作回调函数，需用户实现
 *
 *  @param [out] mid      操作消息的ID
 *  @param [out] objid    被操作的object ID
 *  @param [out] insid    被操作的instance ID
 *  @param [out] resid    被操作的resource ID
 *  @param [out] type     写资源的类型，等于该资源首次上报(notify)时定型的type；资源从未上报时为2(opaque)
 *  @param [out] is_over  多条写指令上报时消息标识 1=第一条 2=中间 0=最后一条
 *  @param [out] data     被写入的数据，格式随type（见@details"数据格式"）
 *  @param [out] len      数据长度：数值类型为字符串长度(strlen)，二进制类型为字节数；务必结合type判断
 *  @param [out] param    LwM2M回调函数参数结构体
 *
 *  @return 无
 *
 *  @details 
 *  data格式（随该资源定型type）：
 *    type=1 raw / type=2 opaque / 资源从未上报 -> 原始二进制字节(可含0x00，按len读取、勿用strlen) 
 *    type=3 integer  -> 十进制整数字符串(如"300") 
 *    type=4 float    -> 浮点字符串(如"3.140000") 
 *    type=5 bool     -> "0"/"1"字符串 
 *  data指针仅在回调期间有效，返回后即被释放，如需保留请在回调内拷贝。
 */
typedef void (*cm_lwm2m_write_cb_t)(int32_t mid, int32_t objid, int32_t insid, int32_t resid, int32_t type, int32_t is_over, char *data, int32_t len, cm_lwm2m_cb_param_t param);

/**
 *  @brief 平台excute操作回调函数，需用户实现
 *
 *  @param [out] mid   操作消息的ID
 *  @param [out] objid 被操作的object ID
 *  @param [out] insid 被操作的instance ID
 *  @param [out] resid 被操作的resource ID
 *  @param [out] data  执行的参数
 *  @param [out] len   数据长度
 *  @param [out] param LwM2M回调函数参数结构体
 *
 *  @return 无
 *
 */
typedef void (*cm_lwm2m_execute_cb_t)(int32_t mid, int32_t objid, int32_t insid, int32_t resid, char *data, int32_t len, cm_lwm2m_cb_param_t param);

/**
 *  @brief 平台observe操作回调函数，需用户实现
 *
 *  @param [out] mid 操作消息的ID
 *  @param [out] observe 订阅标志 1：订阅；0：取消订阅 
 *  @param [out] objid 被操作的object ID
 *  @param [out] insid 被操作的instance ID
 *  @param [out] resid 被操作的resource ID
 *  @param [out] param LwM2M回调函数参数结构体
 *
 *  @return 无
 *
 */
typedef void (*cm_lwm2m_observe_cb_t)(int32_t mid, int32_t observe, int32_t objid, int32_t insid, int32_t resid, cm_lwm2m_cb_param_t param);

/**
 *  @brief 平台discover操作回调函数，需用户实现
 *
 *  @param [out] mid 操作消息的ID
 *  @param [out] objid 被操作的object ID
 *  @param [out] param LwM2M回调函数参数结构体
 *
 *  @return 无
 *
 */
typedef void (*cm_lwm2m_discover_cb_t)(int32_t mid, int32_t objid, cm_lwm2m_cb_param_t param);

/**
 *  @brief 平台设置策略参数请求回调函数，需用户实现
 *
 *  @param [out] mid   操作消息的ID
 *  @param [out] objid 被操作的object ID
 *  @param [out] insid 被操作的instance ID
 *  @param [out] resid 被操作的resource ID
 *  @param [out] parameter   策略参数，格式如:pmin=xxx; pmax=xxx; gt=xxx; lt=xxx; stp=xxx\n
 *                  pmin: int类型，上传数据的最小时间间隔 \n
 *                  pmax: int类型，上传数据的最大时间间隔 \n
 *                  t:    double类型，当数据大于该值上传 \n
 *                  lt:   double类型，当数据小于该值上传 \n
 *                  stp:  double类型，当两个数据点相差大于或者等于该值上传
 *  @param [out] len   数据长度
 *  @param [out] param LwM2M回调函数参数结构体
 *
 *  @return 无
 *
 */
typedef void (*cm_lwm2m_params_cb_t)(int32_t mid, int32_t objid, int32_t insid, int32_t resid, char *parameter, int32_t len, cm_lwm2m_cb_param_t param);


/** LwM2M事件回调*/
typedef struct cm_lwm2m_cb
{
    cm_lwm2m_read_cb_t       onRead;         /*!< 读回调，平台下发Read请求时触发*/
    cm_lwm2m_write_cb_t      onWrite;        /*!< 写回调，平台下发Write请求时触发*/
    cm_lwm2m_execute_cb_t    onExec;         /*!< 执行回调，平台下发Exec请求时触发*/
    cm_lwm2m_observe_cb_t    onObserve;      /*!< 订阅回调，平台下发订阅请求时触发*/
    cm_lwm2m_params_cb_t     onParams;       /*!< 策略参数回调，平台下发策略时触发*/
    cm_lwm2m_event_cb_t      onEvent;        /*!< 事件状态通知*/
    cm_lwm2m_notify_cb_t     onNotify;       /*!< Notify通知*/
    cm_lwm2m_discover_cb_t   onDiscover;     /*!< 平台下发Discover请求时触发*/
}cm_lwm2m_cb_t;

/** cm_lwm2m_cfg_t.flag 位标志(可按位或组合) */
#define CM_LWM2M_FLAG_BOOTSTRAP        (1 << 0)  /**< bit0: 1=bootstrap服务器; 0=lwm2m ip直连服务器 */
#define CM_LWM2M_FLAG_DISABLE_MONITOR  (1 << 1)  /**< bit1: 1=禁止moniter功能; 0=使能moniter功能 */
#define CM_LWM2M_FLAG_PSK_HEX          (1 << 4)  /**< bit4: 1=psk为HEX编码字符串(每字节2个hex字符)，由模组解码为二进制密钥; 适用于二进制PSK(可能含0x00)以HEX串形式传入; 0=psk为普通字符串 */

/**LwM2M可配参数*/
typedef struct {
    cm_lwm2m_platform_e platform;    /*!< 使用的目标IoT平台 */
    union
    {
        int32_t pattern;                     /*!< 登录平台所使用的的endpoint name模式
                                            0：不指定，各个平台采用默认值 \n
                                            1：IMEI，CTWing平台、华为云物联网平台默认方式，库中自行填充IMEI数据 \n
                                            2：IMEI;IMSI，OneNET平台默认方式，库中自行填充IMEI、IMSI数据 \n
                                            3：urn:imei:###########; CTWing平台可选方式，库中自行填充IMEI数据 \n
                                            4：urn:imei-imsi:###########-###########; CTWing平台可选方式，库中自行填充IMEI、IMSI数据 */
        char* name;                          /*!< 登录平台的epname由客户指定，platform = 10时有效 */
    } endpoint;    /*!< 登录平台所使用的的endpoint（platform = 10时使用endpoint.name，其余情况使用endpoint.pattern） */
    char *host;                     /*!< 字符串，服务器地址，格式如ip:port。port不设置时默认5683 */
    int32_t flag;                   /*!< 整型，标志位 \n
                                        bit0： 1，bootstrap服务器；0，lwm2m ip直连服务器 \n
                                        bit1： 1，禁止moniter功能；0，使能moniter功能。\n
                                        bit4： 1，psk为HEX编码字符串(每字节2个hex字符)，由模组解码为二进制密钥；适用于二进制PSK(可能含0x00)以HEX串形式传入。0，psk为普通字符串。*/
    char *auth_code;                /*!< 字符串，登录服务器认证参数，不设值时平台无认证参数 */
    char *psk;                      /*!< 字符串，配置与平台建立DTLS连接的psk秘钥 */
    char *pskid;                    /*!< 字符串，配置与平台建立DTLS连接的pskID。为空时，采用平台默认的PSKID */
    bool auto_update;               /*!< 是否开启自动更新 \n
                                        0：关闭自动更新 \n
                                        1：开启自动更新 \n
                                        自动update功能开启后，将在lifetime计时器执行之前30s自动向平台发起update请求 \n
                                        模组在发送时网络异常导致update请求不成功，将在lifetime计时器到来之时退出平台 \n
                                        lifetime大于等于60s时，自动update功能方可生效 \n
                                        若调用cm_lwm2m_update()，自动update时间将更新 */
    cm_lwm2m_cb_t cb;               /*!< LwM2M回调函数设置 */
    void *cb_param;                 /*!< LwM2M回调函数用户参数（参见cm_lwm2m_event_cb_t等回调函数中param参数描述） \n
                                        指针类型，指向的内存空间需要用户自行管理其开辟和释放 */
}cm_lwm2m_cfg_t;


#ifdef __cplusplus
extern "C"
{
#endif


/**
 * @brief 创建LwM2M设备
 * 
 * @param [in]  cfg    LwM2M配置结构体
 * @param [out] handle LwM2M实例句柄
 * 
 * @return 
 *   = 0  - 成功 \n
 *   > 0  - 失败, 返回值为错误码，参见cm_lwm2m_erroc_e
 */
int32_t cm_lwm2m_create(cm_lwm2m_cfg_t *cfg, cm_lwm2m_handle_t* handle);

/**
 * @brief 删除LwM2M设备
 * 
 * @param [in]  handle LwM2M实例句柄
 * 
 * @return 
 *   = 0  - 成功 \n
 *   > 0  - 失败, 返回值为错误码，参见cm_lwm2m_erroc_e
 */
int32_t cm_lwm2m_delete(cm_lwm2m_handle_t handle);

/**
 *  @brief 为指定设备示例添加一个object及其所属的instance
 *
 *  @param [in]  handle    LwM2M实例句柄
 *  @param [in]  objid     object id,本次需添加的object，如3200
 *  @param [in]  instances  实例数组（数组大小为inscount），每一个数组元素表示为一个实例，其中1表示可用，0表示不可用 \n
 *                             例如当前添加的object有5个实例，其中0,3可用，则实例数组为insarray[5] = {1, 0, 0, 1, 0};
 *  @param [in]  inscount  设置该object对象的instance个数，若设置本次添加的object有5个实例，则inscount为5
 *  @param [in]  attcount  属性个数，目前无意义，置0即可
 *  @param [in]  actcount  操作个数，目前无意义，置0即可
 *
 *  @return 
 *   = 0  - 成功 \n
 *   > 0  - 失败, 返回值为错误码，参见cm_lwm2m_erroc_e
 *
 */
int32_t cm_lwm2m_add_obj(cm_lwm2m_handle_t handle, int32_t objid, uint8_t instances[], int32_t inscount, int32_t attcount, int32_t actcount);

/**
 *  @brief 删除指定object及其所属的instance和resource
 *
 *  @param [in]  handle    lwm2m实例句柄
 *  @param [in]  objid     object id,本次需删除的object，如3200
 *
 *  @return 
 *   = 0  - 成功 \n
 *   > 0  - 失败, 返回值为错误码，参见cm_lwm2m_erroc_e
 *  
 */
int32_t cm_lwm2m_del_obj(cm_lwm2m_handle_t handle, int32_t objid);

/**
 *  @brief 设置指定object的所需资源列表
 *
 *  @param [in]  handle  LwM2M实例句柄
 *  @param [in]  objid   Object ID
 *  @param [in]  resoures object的资源列表数组，例如resoure[3] = {5500, 5501, 5750}
 *  @param [in]  rescount  object的资源数量，即resoure数组的大小
 *
 *  @return 
 *   = 0  - 成功 \n
 *   > 0  - 失败, 返回值为错误码，参见cm_lwm2m_erroc_e
 *
 */
int32_t cm_lwm2m_discover(cm_lwm2m_handle_t handle, int32_t objid, int32_t resoures[], int32_t rescount);

/**
 *  @brief 向平台发起登录请求或查询是否登录平台
 *
 *  @param [in]  handle    LwM2M实例句柄
 *  @param [in]  timeout   登录的超时时长，单位：s。（不可设置为0，建议设置值大于等于30）
 *  @param [in]  lifetime  本次登录平台的生命周期，单位：s。（不可设置为0，建议设置值86400）
 *
 *  @return 
 *   = 0  - 操作发起成功，操作是否成功依据模组cm_lwm2m_event_cb_t回调结果 \n
 *   > 0  - 失败, 返回值为错误码，参见cm_lwm2m_erroc_e
 */
int32_t cm_lwm2m_open(cm_lwm2m_handle_t handle, uint32_t timeout, uint32_t lifetime);

/**
 *  @brief 向平台发起更新请求
 *  
 *  @param [in]  lifetime  本次更新的生命周期，单位：s
 *  @param [in]  update_object    是否需要同时更新登录的Object对象，false:不需要更新 true:需要更新
 *
 *  @return 
 *   = 0  - 操作发起成功，操作是否成功依据模组cm_lwm2m_event_cb_t回调结果 \n
 *   > 0  - 失败, 返回值为错误码，参见cm_lwm2m_erroc_e
 *
 *  @details lifetime值小于10时，lifetime将默认被设置为86400s，即一天 \n
 *           若在open成功之后更新object，可将update_object置1
 */
int32_t cm_lwm2m_update(cm_lwm2m_handle_t handle, uint32_t lifetime, bool update_object);

/**
 * @brief 向平台发起退出登录请求
 * 
 * @param [in]  handle LwM2M实例句柄
 * 
 * @return 
 *   = 0  - 操作发起成功，操作是否成功依据模组cm_lwm2m_event_cb_t回调结果 \n
 *   > 0  - 失败, 返回值为错误码，参见cm_lwm2m_erroc_e
 */
int32_t cm_lwm2m_close(cm_lwm2m_handle_t handle);

/**
 *  @brief （单次或多次）对待上传数据进行组包
 *
 *  @param [in]  handle       LwM2M实例句柄
 *  @param [in]  objid        object id
 *  @param [in]  insid        instance id
 *  @param [in]  resid        resource id
 *  @param [in]  type         上报资源数据类型，取值1~6（含义见@details"数据类型表"） 
 *  @param [in]  value        上报数据值，格式随type（见数据类型表） 
 *  @param [in]  len          上报数据长度
 *  @param [in]  content_type 报文编码格式：0=平台默认 1=TEXT 2=LINK 3=OPAQUE 4=TLV 5=JSON
 *
 *  @return 
 *   = 0  - 成功 
 *   > 0  - 失败, 返回值为错误码，参见cm_lwm2m_erroc_e
 *
 *  @details 
 *  数据类型表（客户输入value -> 模组处理后上线的payload）： 
 *    1 raw      原始字节(value,len)，可含0x00；模组原样memcpy，二进制安全、不解析。OC推荐用于传文本或任意二进制。 
 *    2 opaque   十六进制字符串(偶数长,仅0-9a-fA-F,不含嵌入0x00)；模组转成二进制字节后上报。源于AT模式(文本串输入)，OC通常无需使用。 
 *    3 integer  十进制整数字符串(如"300")；模组atoi后转为1/2/4字节大端。仅32位有符号，不支持64位。 
 *    4 float    十进制浮点字符串(如"3.14")；模组atof后转为8字节大端double。 
 *    5 bool     "0"或"1"；模组转为1字节。 
 *    6 hexstr   十六进制字符串(输入同opaque)；模组转成二进制后以类型1(raw)上报。与类型2仅最终标签不同，OC一般无需使用。
 *
 *  使用要点：
 *    1) OC(opencpu)用户推荐用type=1传任意二进制或文本；
 *    2) 字节序：type=1是raw透传，模组不做字节序转换，平台/LwM2M整型按大端解。若以type=1透传数值原始二进制，须自行序列化为大端(网络字节序)，否则主机小端被平台按大端读会错位。
 *    3) 资源类型首次notify定型后不可变更，后续上报须与首次type一致否则失败； 
 *    4) content_type说明
 *       content_type=0时，根据cm_lwm2m_create()中设置的平台类型自动选择：
 *       - OneNET/CTWing/DMP/其他 → TLV(11542)
 *       - 华为云(HUAWEI) → OPAQUE(42)
 *       非TLV格式(1=TEXT,2=LINK,3=OPAQUE,5=JSON)下，数值类型(int/float/bool)将以二进制编码发送
 *       （如int "300"发送2字节大端0x012C）。
 *    5) 组包规则（多次调用cm_lwm2m_notify_packing后再调用cm_lwm2m_notify发送）：
 *       - TLV格式(content_type=4，或0在TLV平台)：支持多次组包(<=32次)，可在一次notify中将多个resource
 *         合并为单个TLV报文发出。同obj同ins不同res可合并；同obj不同ins也可合并(以obj/ins/*方式
 *         组包)；不同obj的resource无法合并到同一报文，仅最后一个pack所属obj的数据会被发出，
 *         其余obj的数据虽已组包但不会被发送（数据丢失）。组包总量建议<=1000字节。
 *       - 非TLV格式(content_type=1/2/3/5，或0在OPAQUE平台)：仅支持单包发送。若多次pack后调用notify，
 *         第一条非末包数据即被拒绝(返回错误)，整个notify失败，不会分包发送任何数据。
 *       - 组包失败会清空已组包数据；组包后须调用cm_lwm2m_notify()才实际发出。
 *    6) 下发write回调里data格式随该资源定型type不同，详见cm_lwm2m_write_cb_t。
 */
int32_t cm_lwm2m_notify_packing(cm_lwm2m_handle_t handle, int32_t objid, int32_t insid, int32_t resid, int32_t type, char *value, int32_t len, int32_t content_type);

/**
 *  @brief 向平台上报已组包数据
 *
 *  @param [in]  handle  LwM2M实例句柄
 *  @param [in]  mid     指定该消息是否以响应形式上报  -1:非响应模式  >=0:响应模式，终端message id \n
 *                       响应模式下，该条Notify消息成功时将被cm_lwm2m_notify_cb_t回调，回调函数中的mid应与此处一致 \n
 *                       相应模式下，一定时间内，每次调用本接口时，建议使用不同的mid值
 *
 *  @return 
 *   = 0  - 成功 \n
 *   > 0  - 失败, 返回值为错误码，参见cm_lwm2m_erroc_e
 *
 *  @details 该接口调用前应使用cm_lwm2m_notify_packing()完成上传数据的组包。
 *           报文编码格式(content_type)已在cm_lwm2m_notify_packing()中指定，本接口不再单独设置。
 *           平台推荐content_type：OneNET/CTWing/DMP用0(平台默认→TLV)；华为云用0(平台默认→OPAQUE)。
 *           content_type=0会根据cm_lwm2m_create()中设置的平台类型自动匹配，也可显式指定4(TLV)或3(OPAQUE)。
 */
int32_t cm_lwm2m_notify(cm_lwm2m_handle_t handle, int32_t mid);

/**
 *  @brief 用于响应读操作请求
 *
 *  @param [in]  handle  LwM2M实例句柄
 *  @param [in]  mid     操作结果
 *                       1      Content OK
 *                       11     Bad Request
 *                       12     Unauthorized
 *                       13     Not Found
 *                       14     Method Not Allowed
 *                       15     Not Acceptable
 *  @param [in]  result  该resource所属的instance observe操作时下发的mid
 *  @param [in]  objid   object id
 *  @param [in]  insid   instance id
 *  @param [in]  resid   resource id
 *  @param [in]  type    上报资源数据类型，取值1~6（含义见@details"数据类型表"）
 *  @param [in]  value   上报数据值，格式随type（见数据类型表） 
 *  @param [in]  len     上报数据长度
 *
 *  @return 
 *   = 0  - 成功 \n
 *   > 0  - 失败, 返回值为错误码，参见cm_lwm2m_erroc_e
 *
 *  @details 
 *  数据类型表（客户输入value -> 模组处理后上线的payload）： 
 *    1 raw      原始字节(value,len)，可含0x00；模组原样memcpy，二进制安全、不解析。OC推荐用于传文本或任意二进制。 
 *    2 opaque   十六进制字符串(偶数长,仅0-9a-fA-F,不含嵌入0x00)；模组转成二进制字节后上报。源于AT模式(文本串输入)，OC通常无需使用。 
 *    3 integer  十进制整数字符串(如"300")；模组atoi后转为1/2/4字节大端。仅32位有符号，不支持64位。 
 *    4 float    十进制浮点字符串(如"3.14")；模组atof后转为8字节大端double。 
 *    5 bool     "0"或"1"；模组转为1字节。 
 *    6 hexstr   十六进制字符串(输入同opaque)；模组转成二进制后以类型1(raw)上报。与类型2仅最终标签不同，OC一般无需使用。
 *
 *  使用要点：
 *    1) OC(opencpu)用户推荐用type=1传任意二进制或文本；
 *    2) 字节序：type=1是raw透传，模组不做字节序转换，平台/LwM2M整型按大端解。若以type=1透传数值原始二进制，须自行序列化为大端(网络字节序)，否则主机小端被平台按大端读会错位。
 */
int32_t cm_lwm2m_read_rsp(cm_lwm2m_handle_t handle, int32_t mid, int32_t result, int32_t objid, int32_t insid, int32_t resid, int32_t type, char *value, int32_t len);

/**
 *  @brief 用于响应写操作请求
 *
 *  @param [in]  handle  LwM2M实例句柄
 *  @param [in]  mid     该resource所属的instance observe操作时下发的mid
 *  @param [in]  result  操作结果参见cm_lwm2m_result_e，例如 \n
 *                       2      Changed
 *                       11     Bad Request
 *                       12     Unauthorized
 *                       13     Not Found
 *                       14     Method Not Allowed
 *
 *  @return 
 *   = 0  - 成功 \n
 *   > 0  - 失败, 返回值为错误码，参见cm_lwm2m_erroc_e
 *
 */
int32_t cm_lwm2m_write_rsp(cm_lwm2m_handle_t handle, int32_t mid, int32_t result);

/**
 *  @brief 用于响应命令操作请求
 *
 *  @param [in]  handle  LwM2M实例句柄
 *  @param [in]  mid     该resource所属的instance observe操作时下发的mid
 *  @param [in]  result  操作结果参见cm_lwm2m_result_e，例如 \n
 *                       2      Changed
 *                       11     Bad Request
 *                       12     Unauthorized
 *                       13     Not Found
 *                       14     Method Not Allowed
 *
 *  @return 
 *   = 0  - 成功 \n
 *   > 0  - 失败, 返回值为错误码，参见cm_lwm2m_erroc_e
 *
 */
int32_t cm_lwm2m_execute_rsp(cm_lwm2m_handle_t handle, int32_t mid, int32_t result);

/**
 *  @brief 用于响应策略参数操作请求
 *
 *  @param [in]  handle  LwM2M实例句柄
 *  @param [in]  mid     该resource所属的instance observe操作时下发的mid
 *  @param [in]  result  操作结果参见cm_lwm2m_result_e，例如 \n
 *                       2      Changed
 *                       11     Bad Request
 *                       12     Unauthorized
 *                       13     Not Found
 *                       14     Method Not Allowed
 *
 *  @return 
 *   = 0  - 成功 \n
 *   > 0  - 失败, 返回值为错误码，参见cm_lwm2m_erroc_e
 *
 */
int32_t cm_lwm2m_param_rsp(cm_lwm2m_handle_t handle, int32_t mid, int32_t result);

/**
 *  @brief 用于响应观测请求操作请求
 *
 *  @param [in]  handle  LwM2M实例句柄
 *  @param [in]  mid     该resource所属的instance observe操作时下发的mid
 *  @param [in]  result  操作结果参见cm_lwm2m_result_e，例如 \n
 *                       2      Changed
 *                       11     Bad Request
 *                       12     Unauthorized
 *                       13     Not Found
 *                       14     Method Not Allowed
 *
 *  @return 
 *   = 0  - 成功 \n
 *   > 0  - 失败, 返回值为错误码，参见cm_lwm2m_erroc_e
 *
 */
int32_t cm_lwm2m_observe_rsp(cm_lwm2m_handle_t handle, int32_t mid, int32_t result);

/**
 *  @brief 设置版本号
 *
 *  @param [in]  version  版本号字符串指针（最大设置16字节）
 *
 *  @return 
 *   = 0  - 成功 \n
 *   > 0  - 失败, 返回值为错误码，参见cm_lwm2m_erroc_e
 *  @details 若不设置版本号，则上报SDK版本号
 */
int32_t cm_lwm2m_set_version(char *version);
/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __CM_LWM2M_H__ */
