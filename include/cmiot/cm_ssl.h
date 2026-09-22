/**
 * @file        cm_ssl.h
 * @brief       SSL通用API接口
 * @copyright   Copyright © 2021 China Mobile IOT. All rights reserved.
 * @author      By cmiot1325
 * @date        2021/8/5
 *
 * @defgroup ssl ssl
 * @ingroup ssl
 * @{
 */

#ifndef __CM_SSL_H__
#define __CM_SSL_H__

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MSSL_CERTS_FILENAME_SIZE_MAX 64 /*!< 证书名最大长度*/   

/****************************************************************************
 * Public Types
 ****************************************************************************/

/** SSL证书文件类型 */
typedef enum
{
    CM_SSL_FILE_TYPE_CERT = 1,  /*!< 证书 */
    CM_SSL_FILE_TYPE_KEY,       /*!< 私钥 */
    CM_SSL_FILE_TYPE_PSKID,     /*!< PSK预共享密钥（不支持，请勿配置）*/
    
    CM_SSL_FILE_TYPE_MAX,
}cm_ssl_file_type;

/** SSL证书文件结构体**/
typedef struct _cm_ssl_certfile_t
{
    uint8_t file_name[MSSL_CERTS_FILENAME_SIZE_MAX + 1];    /*!< 证书名 */
    cm_ssl_file_type file_type;                             /*!< 证书类型 */
    uint16_t cert_length;                                   /*!< 证书长度 */
    uint16_t write_len;                                     /*!< 已写入长度 */
    void *fs;                                              /*!< w文件句柄 */
    void *next;                                             /*!< 下一个证书 */
} cm_ssl_certfile_t;

/** SSL配置类型 */
typedef enum
{
    CM_SSL_PARAM_VERIFY,            /*!< 设置认证等级, 参数(value): 类型uint8_t, 0无身份验证，1单向验证， 2双向验证. */
    CM_SSL_PARAM_VERSION,           /*!< 设置支持的协议版本, 参数(value): 类型uint8_t, 3 TLS1.2；255 全部. 建议使用255，当前版本不支持TLS1.2之前的协议，配置其他值可能会造成无法连接 */
    CM_SSL_PARAM_SESSION,           /*!< 设置是否开启会话恢复功能, 参数(value): 类型uint8_t, 0 关闭；1 打开. */
    CM_SSL_PARAM_IGNORE_STAMP,      /*!< 设置是否忽略证书时间, 参数(value): 类型uint8_t, 0 不忽略；1 忽略.. */
    CM_SSL_PARAM_IGNORE_VERIFY,     /*!< 设置是否忽略证书认证结果, 参数(value): 类型uint8_t, 0 不忽略；1 忽略. 仅模组端忽略 */
    CM_SSL_PARAM_NEGOTIME,          /*!< 设置协商阶段的最大超时时间, 参数(value): 类型uint16_t，10~300s. */
    CM_SSL_PARAM_SUITES,            /*!< 设置加密套件, 参数(value): 类型int32_t，0支持所有套件. */
    CM_SSL_PARAM_CA_CERT_FILENAME,  /*!< 设置CA证书文件名, 参数(value): 类型uint8_t *, 最大能保存64字节. */
    CM_SSL_PARAM_CLI_CERT_FILENAME, /*!< 设置客户端证书文件名, 参数(value): 类型uint8_t *, 最大能保存64字节. */
    CM_SSL_PARAM_CLI_KEY_FILENAME,  /*!< 设置私钥文件名, 参数(value): 类型uint8_t *, 最大能保存64字节. */
    CM_SSL_PARAM_PSKID,             /*!< （不支持，请勿配置） */
    CM_SSL_PARAM_SNI,               /*!< 设置是否使能SNI功能, 参数(value): 类型uint8_t, 0 关闭；1 打开.. */
}cm_ssl_param_type_e;


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
 * @brief 设置ssl相关参数
 *
 * @param [in] ssl_id         ssl通道号，范围为0-5
 * @param [in] type           配置项
 * @param [in] value          设定值(传入参数值)
 *
 * @return -1 失败；0 成功
 *
 * @details 设置ssl相关参数，支持项见cm_ssl_param_type_e
 */
int32_t cm_ssl_setopt(int32_t ssl_id,cm_ssl_param_type_e type,void * value);

/**
 * @brief 获取ssl相关参数
 *
 * @param [in] ssl_id         ssl通道号，范围为0-5
 * @param [in] type           配置项
 * @param [out] value         获取设定值
 *
 * @return -1 失败；0 成功
 *
 * @details 获取ssl相关参数，支持项见cm_ssl_param_type_e，获取文件名时需确保存储区大于64字节
 */
int32_t cm_ssl_getopt(int32_t ssl_id,cm_ssl_param_type_e type,void * value);

/**
 * @brief 建立SSL连接
 *
 * @param [out] cm_ssl_ctx_p ssl索引上下文指针地址
 * @param [in] ssl_id ssl配置通道，范围为0-5
 * @param [in] socket 套接字ID
 * @param [in] timeout 超时时间 ms
 *
 * @return 0:成功 其他:失败
 *
 * @details timeout为连接成功后read的超时时间，协商阶段超时时间由cm_ssl_setopt配置
 */
int cm_ssl_conn(void **cm_ssl_ctx_p, int ssl_id, int socket, int timeout);

/**
 * @brief 建立SSL连接(带服务器域名设置，SNI功能必须使用该接口)
 *
 * @param [out] cm_ssl_ctx_p ssl索引上下文指针地址
 * @param [in] ssl_id ssl配置通道，范围为0-5
 * @param [in] socket 套接字ID
 * @param [in] timeout 超时时间 ms
 * @param [in] host 服务器域名,可设置为NULL（sni和session功能会使用）
 *
 * @return 0:成功 其他:失败
 *
 * @details host设置为NULL时，SNI功能将无法使用。
 */
int cm_ssl_conn_ex(void **cm_ssl_ctx_p, int ssl_id, int socket, int timeout, char *host);

/**
 * @brief 关闭SSL连接
 *
 * @param [in] cm_ssl_ctx_p ssl索引上下文指针地址
 *
 * @details
 */
void cm_ssl_close(void **cm_ssl_ctx_p);

/**
 * @brief SSL发送数据
 *
 * @param [in] cm_ssl_ctx ssl索引上下文指针
 * @param [in] data 发送的数据
 * @param [in] data_len 发送的数据长度
 *
 * @return < 0:失败 其他:实际发送的数据长度
 *
 * @details
 */
int cm_ssl_write(void *cm_ssl_ctx, void *data, int data_len);

/**
 * @brief SSL接收数据
 *
 * @param [in] cm_ssl_ctx ssl索引上下文指针
 * @param [out] data 接收数据缓存区
 * @param [in] data_len 接收数据的长度
 *
 * @return < 0:失败 其他:实际接收的数据长度
 *
 * @details
 */
int cm_ssl_read(void *cm_ssl_ctx, void *data, int data_len);

/**
 * @brief 检查 SSL/TLS 上下文是否存在 “未完成的挂起操作”
 *
 * @param [in] cm_ssl_ctx ssl索引上下文指针
 *
 * @return 0:没有数据缓存 1:有数据缓存
 *
 * @details 检查是否有已从协议栈读取至SSL缓存区但尚未处理的数据
 */
int cm_ssl_check_pending(void *cm_ssl_ctx);

/**
 * @brief 获取SSL缓存数据长度
 *
 * @param [in] cm_ssl_ctx ssl索引上下文指针
 *
 * @return 缓存数据长度
 *
 * @details 获取SSL缓存数据长度
 */
int cm_ssl_get_bytes_avail(void *cm_ssl_ctx);

/**
 * @brief 证书是否已创建
 *
 * @param [in] file_name       证书名
 *
 * @return 0:未创建； 1:已创建
 *
 * @details
 */
bool cm_ssl_check_cert_file_exists(uint8_t *file_name);

/**
 * @brief 写入证书
 *
 * @param [in] file_name       证书名
 * @param [in] length          写入长度
 * @param [in] remain_length   剩余长度
 * @param [in] data            数据
 * @param [in] file_type       证书类型
 *
 * @return 0:成功；其他:失败
 *
 * @details
 */
int cm_ssl_write_cert_file(uint8_t *file_name, uint16_t length, uint16_t remain_length, uint8_t *data, cm_ssl_file_type file_type);

/**
 * @brief 获取证书类型
 *
 * @param [in] file_name       证书名
 *
 * @return -1:失败；其他:文件类型
 *
 * @details 获取证书类型。
 */
int cm_ssl_get_cert_file_type(uint8_t *file_name);

/**
 * @brief 读取证书
 *
 * @param [in] file_name       证书名
 * @param [out] data           数据
 * @param [out] length         读取长度
 *
 * @return 0:成功；其他:失败
 *
 * @details data外部传入空指针，内部分配空间，外部使用后，需释放空间
 */
int cm_ssl_read_cert_file(uint8_t *file_name, uint8_t **data, uint16_t *length);

/**
 * @brief 删除证书
 *
 * @param [in] file_name       证书名
 *
 * @return 0:成功；其他:失败
 *
 * @details 
 */
int cm_ssl_rm_cert_file(uint8_t *file_name);

/**
 * @brief 列举证书
 *
 * @return 链表指针
 *
 * @details 返回证书保存链表
 */
cm_ssl_certfile_t* cm_ssl_list_cert_file(void);

/**
 * @brief 校验证书
 *
 * @param [in] file_name       证书名
 * @param [in] verify_alg      校验类型（当前只支持0：MD5校验）
 * @param [out] out            校验码
 *
 * @return 0:成功；其他:失败
 *
 * @details out外部传入空指针，接口内部将分配内存，外部使用后需在外部释放。
 */
int cm_ssl_check_cert_file(uint8_t *file_name, uint8_t verify_alg, uint8_t **out);

/**
 * @brief 列举支持的加密套件
 *
 * @return 加密套件列表
 *
 * @details 返回加密套件列表数组，数据为0时为数组结尾
 */
int *cm_ssl_lis_cipher(void);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __CM_SSL_H__ */

/** @}*/
