#pragma once

/*
 * 铠湾 MQTT 传输层公共接口。
 * 上下文绑定一个客户端，由平台适配连接、订阅、重连和异步发送。
 */

/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*-------------------------------------------define---------------------------------------------*/
#define KW_CLOUD_BROKER_HOST_SIZE 96U   /* Broker域名/IP缓存长度 */
#define KW_CLOUD_CLIENT_ID_SIZE 64U     /* MQTT Client ID缓存长度 */
#define KW_CLOUD_USERNAME_SIZE 64U      /* MQTT用户名缓存长度 */
#define KW_CLOUD_PASSWORD_SIZE 96U      /* MQTT密码缓存长度 */
#define KW_CLOUD_TOPIC_SIZE 160U        /* 单个Topic最大缓存长度 */
#define KW_CLOUD_IMEI_SIZE 16U          /* 15位IMEI加结束符 */
#define KW_CLOUD_MQTT_BUFFER_SIZE 3072U /* Paho收发缓存大小 */
#define KW_CLOUD_MAX_PAYLOAD_SIZE 2800U /* 单条MQTT负载上限 */
#define KW_CLOUD_QUEUE_DEPTH 6U         /* 云发送队列深度 */

/*-------------------------------------------typedef---------------------------------------------*/
/* 云传输层统一返回值。 */
typedef enum
{
    KW_CLOUD_OK = 0,
    KW_CLOUD_ERR_ARGUMENT = -1,
    KW_CLOUD_ERR_STATE = -2,
    KW_CLOUD_ERR_MEMORY = -3,
    KW_CLOUD_ERR_QUEUE = -4,
    KW_CLOUD_ERR_CONFIG = -5,
    KW_CLOUD_ERR_NETWORK = -6,
    KW_CLOUD_ERR_MQTT = -7,
    KW_CLOUD_ERR_OVERSIZE = -8
} kw_cloud_result_t;

/* 同一套固件支持主机和分机两种角色。 */
typedef enum
{
    KW_ROLE_HOST = 0,
    KW_ROLE_BRANCH = 1
} kw_role_t;

/* 两台 4G 模块不能假设公网直连，实际通过平台转发或获授权的私有 Topic 中转。 */
typedef enum
{
    KW_ROUTE_PLATFORM_FORWARD = 0,
    KW_ROUTE_PRIVATE_TOPIC = 1
} kw_route_mode_t;

/* MQTT连接、身份、Topic和重连参数。字符串必须以'\0'结束。 */
typedef struct
{
    kw_role_t role;
    kw_route_mode_t route_mode;

    char broker_host[KW_CLOUD_BROKER_HOST_SIZE];
    uint16_t broker_port;
    bool use_tls;
    void *tls_config;

    uint8_t mqtt_version;
    uint8_t qos;
    bool clean_session;
    uint16_t keepalive_seconds;
    uint32_t command_timeout_ms;
    uint32_t yield_ms;
    uint32_t pdp_poll_ms;
    uint32_t reconnect_min_ms;
    uint32_t reconnect_max_ms;

    char client_id[KW_CLOUD_CLIENT_ID_SIZE];
    char username[KW_CLOUD_USERNAME_SIZE];
    char password[KW_CLOUD_PASSWORD_SIZE];
    char local_imei[KW_CLOUD_IMEI_SIZE];
    char host_imei[KW_CLOUD_IMEI_SIZE];

    char platform_up_topic[KW_CLOUD_TOPIC_SIZE];
    char platform_down_topic[KW_CLOUD_TOPIC_SIZE];
    char peer_tx_topic[KW_CLOUD_TOPIC_SIZE];
    char peer_rx_topic[KW_CLOUD_TOPIC_SIZE];
} kw_cloud_config_t;

/*
 * 云层异步回调。全部运行在cloud task中，回调参数只在本次调用期间有效。
 * on_message返回失败时，云层会再调用on_receive_error暴露丢包原因。
 */
typedef struct
{
    void (*on_state_changed)(bool online, void *user);
    kw_cloud_result_t (*on_message)(const char *topic, size_t topic_len, const uint8_t *payload,
                                    size_t payload_len, void *user);
    void (*on_receive_error)(kw_cloud_result_t result, void *user);
    void (*on_publish_result)(uint32_t cookie, kw_cloud_result_t result, void *user);
    void *user;
} kw_cloud_callbacks_t;

typedef struct
{
    void *user;
    kw_cloud_result_t (*start)(void *user, const kw_cloud_config_t *config,
                               const kw_cloud_callbacks_t *callbacks);
    void (*poll)(void *user, uint32_t now);
    bool (*online)(void *user);
    bool (*stop)(void *user);
    kw_cloud_result_t (*publish)(void *user, const char *topic, const uint8_t *payload, size_t size,
                                 uint8_t qos, bool retained, uint32_t cookie);
} kw_transport_t;

typedef struct
{
    unsigned channel;
    const char *ca_file;
} kw_tls_config_t;

/*-------------------------------------------function---------------------------------------------*/
void kw_cloud_config_init(kw_cloud_config_t *config);

kw_cloud_result_t kw_cloud_make_platform_topics(kw_cloud_config_t *config,
                                                const char imei[KW_CLOUD_IMEI_SIZE]);

kw_cloud_result_t kw_cloud_validate_config(const kw_cloud_config_t *config);

#ifdef __cplusplus
}
#endif
