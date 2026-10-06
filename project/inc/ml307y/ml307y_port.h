#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "product_interface.h"
/*-------------------------------------------define---------------------------------------------*/
#ifndef ML307Y_SLEEP_MODE
#define ML307Y_SLEEP_MODE CM_PM_SLEEP_MODE_LIGHT /* 默认兼容；构建参数显式选择 SDK DEEP 验证档位。 */
#endif
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
/* 绑定平台时钟、队列、任务、身份和电源管理端口。 */
bool ml307y_system_create(product_services_t *services);
/* 按产品命名空间绑定双快照文件端口，不立即读取介质。 */
bool ml307y_storage_create(product_services_t *services);
/* 返回存储恢复告警；没有有效端口时返回未就绪错误。 */
int ml307y_storage_warning(const product_services_t *services);
/* 创建当前产品独占的异步 MQTT 传输实例。 */
bool ml307y_mqtt_create(product_services_t *services);
/* 底包导出的构建身份，用于阻止应用与错误底包混装。 */
const char *project_base_identity(void);
/* 成功 CONNACK 回调中查询实际源 IP；缓冲至少 46 字节，返回 4/6 或负错误码。 */
/* 当前 SDK 私有 socket 布局由配套底包核验；失败清空文本，不改变地址选择。 */
int project_mqtt_local_address(void *client, char *address, size_t capacity);
