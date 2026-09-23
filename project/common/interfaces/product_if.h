#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "system_if.h"
#include "storage_if.h"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
typedef struct
{
    void *user;
    /* false表示读失败，不能当作松开；pressed是经过有效电平转换的原始输入。 */
    bool (*read_key)(void *user, bool *pressed);
    /* 唯一声光入口；相同状态可由板端口去重，失败返回false。 */
    bool (*outputs)(void *user, bool led, bool buzzer);
    /* 返回真实毫伏值；不推算未经标定的电量百分比。 */
    bool (*vbat)(void *user, uint16_t *millivolts);
    /* notify只能投递零等待消息；argument必须保持有效至任务结束。 */
    void (*set_wakeup)(void *user, void (*notify)(void *), void *argument);
    bool ready;
    bool wake_verified; /* 独立于普通GPIO读取的休眠唤醒核验。 */
} board_if_t;

typedef struct
{
    system_if_t system;
    storage_if_t storage;
    board_if_t board;
    void *transport; /* 所选组件的独立传输实例；MQTT产品转换为kw_transport_t。 */
    uint32_t product_id;
    const char *name;
    const char *storage_namespace;
} product_services_t;

/* 每次构建仅实例化一个描述表；函数指针有唯一且显式的实现。 */
typedef struct
{
    uint32_t id;
    const char *name;
    const char *storage_namespace;
    bool (*prepare_board)(product_services_t *services);
    void (*start)(product_services_t *services);
} product_descriptor_t;

/*-------------------------------------------function---------------------------------------------*/
/* Startup binds a single product and board via its generated descriptor. */
