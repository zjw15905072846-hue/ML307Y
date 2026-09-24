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
#define KAIWAN_CLOUD_BROKER_HOST_SIZE 96U   /* Broker域名/IP缓存长度 */
#define KAIWAN_CLOUD_CLIENT_ID_SIZE 64U     /* MQTT Client ID缓存长度 */
#define KAIWAN_CLOUD_USERNAME_SIZE 64U      /* MQTT用户名缓存长度 */
#define KAIWAN_CLOUD_PASSWORD_SIZE 96U      /* MQTT密码缓存长度 */
#define KAIWAN_CLOUD_TOPIC_SIZE 160U        /* 单个Topic最大缓存长度 */
#define KAIWAN_CLOUD_IMEI_SIZE 16U          /* 15位IMEI加结束符 */
#define KAIWAN_CLOUD_MQTT_BUFFER_SIZE 3072U /* Paho收发缓存大小 */
#define KAIWAN_CLOUD_MAXIMUM_PAYLOAD_SIZE 2800U /* 单条MQTT负载上限 */
#define KAIWAN_CLOUD_QUEUE_DEPTH 6U         /* 云发送队列深度 */

/*-------------------------------------------typedef---------------------------------------------*/
/* 云传输层统一返回值。 */
typedef enum
{
    KAIWAN_CLOUD_OK = 0,
    KAIWAN_CLOUD_ERROR_ARGUMENT = -1,
    KAIWAN_CLOUD_ERROR_STATE = -2,
    KAIWAN_CLOUD_ERROR_MEMORY = -3,
    KAIWAN_CLOUD_ERROR_QUEUE = -4,
    KAIWAN_CLOUD_ERROR_CONFIG = -5,
    KAIWAN_CLOUD_ERROR_NETWORK = -6,
    KAIWAN_CLOUD_ERROR_MQTT = -7,
    KAIWAN_CLOUD_ERROR_OVERSIZE = -8
} kaiwan_cloud_result_t;

/* 同一套固件支持主机和分机两种角色。 */
typedef enum
{
    KAIWAN_ROLE_HOST = 0,
    KAIWAN_ROLE_BRANCH = 1
} kaiwan_role_t;

/* 两台 4G 模块不能假设公网直连，实际通过平台转发或获授权的私有 Topic 中转。 */
typedef enum
{
    KAIWAN_ROUTE_PLATFORM_FORWARD = 0,
    KAIWAN_ROUTE_PRIVATE_TOPIC = 1
} kaiwan_route_mode_t;

/* MQTT连接、身份、Topic和重连参数。字符串必须以'\0'结束。 */
typedef struct
{
    kaiwan_role_t role;             /* 主机或分机。 */
    kaiwan_route_mode_t route_mode; /* 平台转发或私有 Topic 中转。 */

    /* Broker 连接参数；tls_config 由平台层持有并保证连接期有效。 */
    char broker_host[KAIWAN_CLOUD_BROKER_HOST_SIZE];
    uint16_t broker_port;
    bool use_tls;
    void *tls_config;

    /* MQTT 会话和轮询/重连时间，时间字段单位均为毫秒。 */
    uint8_t mqtt_version;
    uint8_t qos;
    bool clean_session;
    uint16_t keepalive_seconds;
    uint32_t command_timeout_ms;
    uint32_t yield_ms;
    uint32_t pdp_poll_ms;
    uint32_t reconnect_minimum_ms;
    uint32_t reconnect_maximum_ms;

    /* 身份字段；local_imei 用于本机 Topic，host_imei 用于分机路由。 */
    char client_id[KAIWAN_CLOUD_CLIENT_ID_SIZE];
    char username[KAIWAN_CLOUD_USERNAME_SIZE];
    char password[KAIWAN_CLOUD_PASSWORD_SIZE];
    char local_imei[KAIWAN_CLOUD_IMEI_SIZE];
    char host_imei[KAIWAN_CLOUD_IMEI_SIZE];

    /* 上下行 Topic 缓冲区由 make_platform_topics 生成或由产品配置填写。 */
    char platform_up_topic[KAIWAN_CLOUD_TOPIC_SIZE];
    char platform_down_topic[KAIWAN_CLOUD_TOPIC_SIZE];
    char peer_transmit_topic[KAIWAN_CLOUD_TOPIC_SIZE];
    char peer_receive_topic[KAIWAN_CLOUD_TOPIC_SIZE];
} kaiwan_cloud_config_t;

/*
 * 云层异步回调。全部运行在cloud task中，回调参数只在本次调用期间有效。
 * on_message返回失败时，云层会再调用on_receive_error暴露丢包原因。
 */
typedef struct
{
    void (*on_state_changed)(bool online, void *user); /* 连接状态切换通知。 */
    kaiwan_cloud_result_t (*on_message)(const char *topic, size_t topic_length, const uint8_t *payload,
                                    size_t payload_length, void *user); /* 收到消息，失败时触发错误回调。 */
    void (*on_receive_error)(kaiwan_cloud_result_t result, void *user); /* 接收处理失败。 */
    void (*on_publish_result)(uint32_t cookie, kaiwan_cloud_result_t result, void *user); /* 传输发送结果，不代表平台业务确认。 */
    void *user; /* 原样传给各回调。 */
} kaiwan_cloud_callbacks_t;

/* 平台传输实现；所有函数使用同一个 user，publish 的 cookie 对应异步结果。 */
typedef struct
{
    void *user; /* 平台适配器上下文。 */
    kaiwan_cloud_result_t (*start)(void *user, const kaiwan_cloud_config_t *config,
                               const kaiwan_cloud_callbacks_t *callbacks); /* 保存配置和回调，发起异步连接。 */
    void (*poll)(void *user, uint32_t now); /* 推进连接、收发和重连。 */
    bool (*online)(void *user);            /* 当前是否具备发送条件。 */
    bool (*stop)(void *user);              /* 停止连接并释放传输状态。 */
    kaiwan_cloud_result_t (*publish)(void *user, const char *topic, const uint8_t *payload, size_t size,
                                 uint8_t qos, bool retained, uint32_t cookie); /* 排队发送，不等于业务确认。 */
} kaiwan_transport_t;

/* TLS 通道号和 CA 文件路径由模组适配层解释。 */
typedef struct
{
    unsigned channel;    /* 模组安全连接通道。 */
    const char *ca_file; /* CA 文件路径，连接期间应保持有效。 */
} kaiwan_tls_config_t;

/*-------------------------------------------function---------------------------------------------*/
/* 填写默认 MQTT 参数；身份、Broker 和 Topic 仍由产品配置完成。 */
void kaiwan_cloud_config_init(kaiwan_cloud_config_t *config);

/* 根据 IMEI 生成平台上下行 Topic；空间不足或 IMEI 不合法返回错误。 */
kaiwan_cloud_result_t kaiwan_cloud_make_platform_topics(kaiwan_cloud_config_t *config,
                                                const char imei[KAIWAN_CLOUD_IMEI_SIZE]);

/* 启动前检查身份、Broker、Topic、时间与路由参数。 */
kaiwan_cloud_result_t kaiwan_cloud_validate_config(const kaiwan_cloud_config_t *config);

#ifdef __cplusplus
}
#endif
