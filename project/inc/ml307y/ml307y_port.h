#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "product_interface.h"
/*-------------------------------------------define---------------------------------------------*/
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
