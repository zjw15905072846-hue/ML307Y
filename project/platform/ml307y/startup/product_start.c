/*------------------------------------------includes--------------------------------------------*/
#include "ml307y_port.h"
#include "product_build_config.h"
#include "cm_os.h"
#include "cm_sys.h"
#include <string.h>
#if PRODUCT_HAS_MQTT
#include "mbedtls/aes.h"
#include <stddef.h>
/* The shipped base ELF DWARF reports nr=0, rk_offset=8, buf=16, size=288. */
_Static_assert(sizeof(mbedtls_aes_context) == 288, "Base AES ABI size mismatch");
_Static_assert(offsetof(mbedtls_aes_context, MBEDTLS_PRIVATE(rk_offset)) == 8,
               "Base AES round key ABI mismatch");
_Static_assert(offsetof(mbedtls_aes_context, MBEDTLS_PRIVATE(buf)) == 16,
               "Base AES buffer ABI mismatch");
#endif
/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
extern bool PRODUCT_BOARD_PREPARE(product_services_t *services);
extern void PRODUCT_ENTRY(product_services_t *services);
static product_services_t s_services;
static const product_descriptor_t s_product = {PRODUCT_ID, PRODUCT_NAME, PRODUCT_STORAGE_NAMESPACE,
                                               PRODUCT_BOARD_PREPARE, PRODUCT_ENTRY};

/*-------------------------------------------function---------------------------------------------*/

/*******************************************************************************
* Function Name  : product_boot_task
* Description    : 验证底包身份并绑定唯一选中的产品与板
* Input          : argument - 保留
* Output         : 产品任务或明确启动诊断
* Return         : 初始化失败时返回
* Attention      : 不使用弱函数覆盖；未核验硬件不会开始报警业务
*******************************************************************************/
static void product_boot_task(void *argument)
{
    (void)argument;
    if (strcmp(project_base_identity(), PROJECT_BASE_ID) != 0)
    {
        cm_log_printf(0, "[project] incompatible base image");
        return;
    }
    s_services.name = s_product.name;
    s_services.product_id = s_product.id;
    s_services.storage_namespace = s_product.storage_namespace;
    if (!ml_system_create(&s_services) || !s_product.prepare_board(&s_services))
    {
        cm_log_printf(0, "[project] product/board not ready");
        return;
    }
#if PRODUCT_HAS_STORAGE
    if (!ml_storage_create(&s_services))
    {
        s_services.system.fault("storage-bind", -1);
        return;
    }
#endif
#if PRODUCT_HAS_MQTT
    if (!ml_mqtt_create(&s_services))
    {
        s_services.system.fault("mqtt-bind", -1);
        return;
    }
#endif
    s_product.start(&s_services);
}

/*******************************************************************************
* Function Name  : cm_opencpu_entry
* Description    : 唯一OpenCPU入口，创建启动任务后立即返回
* Input          : param - SDK启动参数
* Output         : 产品启动任务
* Return         : 0成功；-1任务创建失败
* Attention      : 入口内不执行存储、联网或等待
*******************************************************************************/
int cm_opencpu_entry(void *param)
{
    osThreadAttr_t attributes = {0};
    (void)param;
    attributes.name = "product-boot";
    attributes.stack_size = 8192U;
    attributes.priority = osPriorityNormal;
    return osThreadNew(product_boot_task, NULL, &attributes) ? 0 : -1;
}
