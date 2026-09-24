/*------------------------------------------includes--------------------------------------------*/
#include "ml307y/ml307y_port.h"
#include "ml307y/diag_uart.h"
#include "product_build_config.h"
#include "cm_os.h"
#include "cm_sys.h"
#include <string.h>
#if PRODUCT_HAS_MQTT
#include "mbedtls/aes.h"
#include <stddef.h>
/* 随附底包 ELF 的 DWARF 显示 nr=0、rk_offset=8、buffer=16、总长=288。 */
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
extern bool PRODUCT_ENTRY(product_services_t *services);
/* 当前构建只选择一个产品，服务容器在产品任务整个生命周期内有效。 */
static product_services_t selected_product_services;
static const product_descriptor_t selected_product = {PRODUCT_ID, PRODUCT_NAME, PRODUCT_STORAGE_NAMESPACE,
                                               PRODUCT_BOARD_PREPARE, PRODUCT_ENTRY};

/*-------------------------------------------function---------------------------------------------*/

/*******************************************************************************
* Function Name  : product_boot_initialize
* Description    : 验证底包并依次绑定系统、板、存储、网络与产品入口
* Input          : 无
* Output         : 产品服务与业务任务
* Return         : true - 启动完成；false - 初始化失败
* Attention      : 失败时保留诊断，不继续启动后续模块
*******************************************************************************/
static bool product_boot_initialize(void)
{

    if (ml307y_uart_diag_init() == 0)
    {
        ml307y_uart_diag_printf("[project] UART0 ready");
    }
    /* 先核对编译所用底包身份，避免错误接口表进入产品初始化。 */
    if (strcmp(project_base_identity(), PROJECT_BASE_ID) != 0)
    {
        ml307y_uart_diag_printf("[project] incompatible base image");
        return false;
    }
    selected_product_services.name = selected_product.name;
    selected_product_services.product_id = selected_product.id;
    selected_product_services.storage_namespace = selected_product.storage_namespace;
    if (!ml307y_system_create(&selected_product_services))
    {
        ml307y_uart_diag_printf("[project] system not ready");
        return false;
    }
    if (!selected_product.prepare_board(&selected_product_services))
    {
        ml307y_uart_diag_printf("[project] board not ready");
        return false;
    }
#if PRODUCT_HAS_STORAGE
    if (!ml307y_storage_create(&selected_product_services))
    {
        selected_product_services.system.fault("storage-bind", -1);
        return false;
    }
#endif
#if PRODUCT_HAS_MQTT
    if (!ml307y_mqtt_create(&selected_product_services))
    {
        selected_product_services.system.fault("mqtt-bind", -1);
        return false;
    }
#endif
    /* 板、存储和传输端口就绪后，最后启动产品业务。 */
    if (!selected_product.start(&selected_product_services))
    {
        selected_product_services.system.fault("product-start", -1);
        return false;
    }
    return true;
}

/*******************************************************************************
* Function Name  : product_boot_task
* Description    : 执行一次产品初始化，随后在带延时的循环中保持任务存活
* Input          : argument - 保留
* Output         : 产品启动结果
* Return         : 不返回
* Attention      : 循环必须让出 CPU；产品前后台各自运行独立任务
*******************************************************************************/
static void product_boot_task(void *argument)
{
    (void)argument;
    (void)product_boot_initialize();
    while (1)
    {
        osDelay(60U * osKernelGetTickFreq());
    }
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
