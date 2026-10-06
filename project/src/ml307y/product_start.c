/*------------------------------------------includes--------------------------------------------*/
#include "ml307y/ml307y_port.h"
#include "ml307y/diag_uart.h"
#include "product_build_config.h"
#if PRODUCT_HAS_KEY
#include "ml307y/alarm_key.h"
#endif
#if PRODUCT_HAS_LED
#include "ml307y/alarm_led.h"
#endif
#if PRODUCT_HAS_BUZZER
#include "ml307y/alarm_buzzer.h"
#endif
#if PRODUCT_HAS_BATTERY
#include "ml307y/alarm_battery.h"
#endif
#include "cm_os.h"
#include "cm_sys.h"
#include "cm_pm.h"
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
extern bool PRODUCT_ENTRY(product_services_t *services);
/* 当前构建只选择一个产品，服务容器在产品任务整个生命周期内有效。 */
static product_services_t selected_product_services;
/* 同一进程内每创建一个启动任务递增；整机复位后从零开始。 */
static unsigned product_boot_attempts;
static const product_descriptor_t selected_product = {PRODUCT_ID, PRODUCT_NAME, PRODUCT_STORAGE_NAMESPACE,
                                               PRODUCT_ENTRY};

/*-------------------------------------------function---------------------------------------------*/

/*******************************************************************************
* Function Name  : product_boot_initialize
* Description    : 验证底包并按清单依次初始化系统、器件、存储、网络与产品入口
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
        ml307y_uart_diag_printf("[project] boot attempt=%u tick=%u", product_boot_attempts,
                                (unsigned)osKernelGetTickCount());
        ml307y_uart_diag_printf("[project] firmware=%s %s product=%s", __DATE__, __TIME__, PRODUCT_NAME);
        ml307y_uart_diag_printf("[project] diagnostic=mqtt-vendor4872-20260928");
        ml307y_uart_diag_printf("[project] power-on-reason=%d", (int)cm_pm_get_power_on_reason());
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
#if PRODUCT_HAS_KEY
    if (!ml307y_alarm_key_init(&selected_product_services.key, ALARM_BUTTON_WAKE_VERIFIED))
    {
        selected_product_services.system.fault("alarm-key-init", -1);
        return false;
    }
    ml307y_uart_diag_printf("[project] alarm key ready");
#endif
#if PRODUCT_HAS_LED
    if (!ml307y_alarm_led_init(&selected_product_services.led, ALARM_BUTTON_LED_SDK_PIN))
    {
        selected_product_services.system.fault("alarm-led-init", -1);
        return false;
    }
    ml307y_uart_diag_printf("[project] alarm LED ready");
#endif
#if PRODUCT_HAS_BUZZER
    if (!ml307y_alarm_buzzer_init(&selected_product_services.buzzer, ALARM_BUTTON_BUZZER_SDK_PIN,
                                  ALARM_BUZZER_FREQUENCY_HZ))
    {
        selected_product_services.system.fault("alarm-buzzer-init", -1);
        return false;
    }
    ml307y_uart_diag_printf("[project] alarm buzzer ready hz=%u duty=%u%%",
                            (unsigned)ALARM_BUZZER_FREQUENCY_HZ,
                            (unsigned)ALARM_BUZZER_DUTY_PERCENT);
#endif
#if PRODUCT_HAS_BATTERY
    ml307y_alarm_battery_bind(&selected_product_services.battery);
    if (!selected_product_services.battery.ready)
    {
        selected_product_services.system.fault("alarm-battery-init", -1);
        return false;
    }
#endif
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
    /* 器件、存储和传输端口就绪后，最后启动产品业务。 */
    if (!selected_product.start(&selected_product_services))
    {
        selected_product_services.system.fault("product-start", -1);
        return false;
    }
    return true;
}

/*******************************************************************************
* Function Name  : product_boot_task
* Description    : 执行一次产品初始化，随后保持常驻并输出本轮限次存活诊断
* Input          : argument - 保留
* Output         : 产品启动结果
* Return         : 不返回
* Attention      : 循环必须让出 CPU；产品前后台各自运行独立任务
*******************************************************************************/
static void product_boot_task(void *argument)
{
    bool initialized;
    unsigned diagnostic_count = 0;
    osSemaphoreId_t idle_wait;
    (void)argument;
    ++product_boot_attempts;
    initialized = product_boot_initialize();
    idle_wait = osSemaphoreNew(1U, 0U, NULL);
    ml307y_uart_diag_printf("[project] initialization %s", initialized ? "complete" : "failed");
    while (1)
    {
        /* 本轮排障只输出六次，用递增 tick 区分日志静默与调度停滞。 */
        if (diagnostic_count < 6U)
        {
            osDelay(5U * osKernelGetTickFreq());
            ++diagnostic_count;
            ml307y_uart_diag_printf("[project] scheduler alive tick=%u sample=%u/6",
                                   (unsigned)osKernelGetTickCount(), diagnostic_count);
        }
        else if (idle_wait)
        {
            /* 保持任务常驻，不再每五秒无业务唤醒；不向此信号量投递。 */
            (void)osSemaphoreAcquire(idle_wait, osWaitForever);
        }
        else
        {
            osDelay(60U * osKernelGetTickFreq());
        }
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
