#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "system_interface.h"
#include "storage_interface.h"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/* 各器件分别持有接口；业务层不直接操作 CM GPIO。 */
typedef struct
{
    void *user;
    /* false 表示读取失败，不能当作按键松开。 */
    bool (*read)(void *user, bool *pressed);
    /* 中断通知只能投递零等待消息。 */
    void (*set_wakeup)(void *user, void (*notify)(void *), void *argument);
    bool ready;
    bool wake_verified;
} alarm_key_interface_t;

typedef struct
{
    void *user;
    bool (*set)(void *user, bool on);
    bool ready; /* 禁用 LED 时也提供不触碰引脚的接口。 */
} alarm_led_interface_t;

typedef struct
{
    void *user;
    bool (*set)(void *user, bool on);
    bool ready;
} alarm_buzzer_interface_t;

typedef struct
{
    void *user;
    /* 返回真实 VBAT 毫伏值，不推算未经标定的电量百分比。 */
    bool (*read_voltage)(void *user, uint16_t *millivolts);
    bool ready;
} alarm_battery_interface_t;

typedef struct
{
    system_interface_t system; /* 平台任务、队列、时钟和设备信息服务。 */
    storage_interface_t storage; /* 当前产品独占的持久化端口。 */
    alarm_key_interface_t key;
    alarm_led_interface_t led;
    alarm_buzzer_interface_t buzzer;
    alarm_battery_interface_t battery;
    void *transport; /* 所选组件的独立传输实例；MQTT产品转换为kaiwan_transport_t。 */
    uint32_t product_id; /* 与持久镜像身份对应的产品 ID。 */
    const char *name; /* 本次构建选择的产品名。 */
    const char *storage_namespace; /* 文件目录隔离所用的产品命名空间。 */
} product_services_t;

/* 每次构建仅实例化一个描述表；产品入口有唯一且显式的实现。 */
typedef struct
{
    uint32_t id;
    const char *name;
    const char *storage_namespace;
    /* 仅在器件、系统及所需传输/存储端口准备好后调用；任务未全部创建则返回 false。 */
    bool (*start)(product_services_t *services);
} product_descriptor_t;

/*-------------------------------------------function---------------------------------------------*/
/* 启动代码通过生成的描述表只绑定一个产品。 */
