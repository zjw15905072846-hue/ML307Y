#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "system_if.h"
#include "storage_if.h"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/* 板级输入输出由同一个产品实例持有；业务层不直接操作 CM GPIO。 */
typedef struct
{
    void *user; /* 板级实例上下文，在产品任务存活期间保持有效。 */
    /* false表示读失败，不能当作松开；pressed是经过有效电平转换的原始输入。 */
    bool (*read_key)(void *user, bool *pressed);
    /* 唯一声光入口；相同状态可由板端口去重，失败返回false。 */
    bool (*outputs)(void *user, bool led, bool buzzer);
    /* 返回真实毫伏值；不推算未经标定的电量百分比。 */
    bool (*vbat)(void *user, uint16_t *millivolts);
    /* notify只能投递零等待消息；argument必须保持有效至任务结束。 */
    void (*set_wakeup)(void *user, void (*notify)(void *), void *argument);
    bool ready; /* 初始化完成且输出已置安全状态。 */
    bool wake_verified; /* 独立于普通GPIO读取的休眠唤醒核验。 */
} board_if_t;

typedef struct
{
    system_if_t system; /* 平台任务、队列、时钟和设备信息服务。 */
    storage_if_t storage; /* 当前产品独占的持久化端口。 */
    board_if_t board; /* 本次构建所选板的接口。 */
    void *transport; /* 所选组件的独立传输实例；MQTT产品转换为kw_transport_t。 */
    uint32_t product_id; /* 与持久镜像身份对应的产品 ID。 */
    const char *name; /* 本次构建选择的产品名。 */
    const char *storage_namespace; /* 文件目录隔离所用的产品命名空间。 */
} product_services_t;

/* 每次构建仅实例化一个描述表；函数指针有唯一且显式的实现。 */
typedef struct
{
    uint32_t id;
    const char *name;
    const char *storage_namespace;
    /* 板资源未核验或初始化失败必须返回 false，启动流程随即停止。 */
    bool (*prepare_board)(product_services_t *services);
    /* 仅在板、系统及所需传输/存储端口准备好后调用。 */
    void (*start)(product_services_t *services);
} product_descriptor_t;

/*-------------------------------------------function---------------------------------------------*/
/* 启动代码通过生成的描述表只绑定一个产品和一块板。 */
