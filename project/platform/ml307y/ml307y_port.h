#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include "product_if.h"
/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------function---------------------------------------------*/
bool ml_system_create(product_services_t *services);
bool ml_storage_create(product_services_t *services);
int ml_storage_warning(const product_services_t *services);
bool ml_mqtt_create(product_services_t *services);
const char *project_base_identity(void);
