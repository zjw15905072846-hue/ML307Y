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
    /* 由前台唯一所有者管理；只有业务静止且板唤醒已验证才能解除。 */
    void (*power_hold)(void *user, bool hold);
} system_interface_t;

/*-------------------------------------------function---------------------------------------------*/
