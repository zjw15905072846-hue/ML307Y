#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/*-------------------------------------------define---------------------------------------------*/
#define SYSTEM_WAIT_FOREVER UINT32_MAX /* 仅后台允许无限等待队列消息。 */

/*-------------------------------------------typedef---------------------------------------------*/
/* 设备身份与遥测快照；有效性由采集结果和产品协议共同判断。 */
typedef struct
{
    char imei[16]; /* 15 位 IMEI 加字符串终止符。 */
    char imsi[16]; /* 15 位 IMSI 加字符串终止符。 */
    char iccid[21]; /* 20 位 ICCID 加字符串终止符。 */
    uint32_t utc_seconds; /* RTC候选值，产品验证来源和时区后才能标记有效。 */
    uint8_t csq; /* 模组信号质量原值，不自行换算为百分比。 */
    bool csq_valid; /* false 时不得把 csq 当成真实采样值。 */
} device_info_t;

/* 射频请求与实测验证分开；READY 表示功能模式及 PDP 已就绪。 */
typedef enum
{
    SYSTEM_RADIO_OFF,       /* CFUN 关闭且读回匹配。 */
    SYSTEM_RADIO_RESTORING, /* 恢复模式或等待 PDP。 */
    SYSTEM_RADIO_READY,     /* 可以启动 MQTT。 */
    SYSTEM_RADIO_STOPPING,  /* 等待射频关闭确认。 */
    SYSTEM_RADIO_ERROR      /* 保持工作锁，按期限重试。 */
} system_radio_state_t;

/* RTOS、设备信息和电源管理端口；回调归属由平台实现。 */
typedef struct
{
    void *user; /* 平台实例上下文，由启动适配创建并持有。 */
    /* 单调毫秒，允许uint32回绕；不得使用墙上时间计算提示时序。 */
    uint32_t (*millis)(void *user);
    void (*delay)(uint32_t milliseconds); /* 阻塞当前任务，前台按键循环慎用。 */
    /* 队列按固定消息大小复制数据；失败返回NULL。 */
    void *(*queue_create)(unsigned count, unsigned bytes);
    /* timeout_ms=0不等待，可用于前台/中断；FOREVER仅用于后台。 */
    bool (*queue_put)(void *queue, const void *message, uint32_t timeout_ms);
    /* 成功才写出完整消息；超时或失败返回false。 */
    bool (*queue_get)(void *queue, void *message, uint32_t timeout_ms);
    /* entry使用argument上下文；foreground指定较高优先级；失败明确返回false。 */
    bool (*thread_start)(const char *name, void (*entry)(void *), void *argument,
                         unsigned stack_bytes, bool foreground);
    void *(*allocate)(size_t size); /* 分配失败返回 NULL。 */
    void (*release)(void *memory); /* 只释放本端口分配的内存。 */
    /* 后台获取身份与遥测，可能阻塞；不在声光任务调用。 */
    bool (*identity)(device_info_t *info);
    /* 来自匹配底包的随机源，失败不得以固定IV代替。 */
    bool (*random)(uint8_t *output, size_t size);
    /* module只传固定诊断名，禁止包含账户、密钥或报文。 */
    void (*fault)(const char *module, int error);
    /* 前台工作锁需求；平台合并前后台需求后才允许解锁。 */
    void (*power_hold)(void *user, bool hold);
    /* 后台工作锁需求；网络、存储或业务确认未完成时保持。 */
    void (*power_background_hold)(void *user, bool hold);
    /* 后台普通任务按需输出 SDK 休眠状态，不在回调内打印。 */
    void (*power_diagnostic)(void *user);
    /* 射频接口仅由后台调用；request 不等待，poll 在任务中推进并核对模式。 */
    void (*radio_request)(void *user, bool enabled, uint32_t now);
    void (*radio_poll)(void *user, uint32_t now);
    system_radio_state_t (*radio_state)(void *user, int *error);
    uint32_t (*radio_next_wait)(void *user, uint32_t now);
    void (*radio_set_notify)(void *user, void (*notify)(void *), void *argument);
    /* 可选云流程诊断；stage 只允许固定名称，value 仅为序号或事件 ID，禁止传凭据。 */
    void (*diagnostic)(const char *stage, uint32_t value, int result);
    /* 可选发送日志；payload 为实际 MQTT 文本，回调同步消费且不得保留指针。 */
    /* result 仅表示传输入队结果，不能作为平台确认；不传登录凭据或 AES 密钥。 */
    void (*packet_log)(const char *kind, uint16_t sequence, const char *topic,
                       const uint8_t *payload, size_t size, int result);
} system_interface_t;

/*-------------------------------------------function---------------------------------------------*/
