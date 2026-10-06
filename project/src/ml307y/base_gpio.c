/*------------------------------------------includes--------------------------------------------*/
#include <stddef.h>
#include <stdint.h>
#include "ml307y/base_gpio.h"

/*-------------------------------------------define---------------------------------------------*/
/* 当前 CM 底包 DWARF 与 cm_dtr_init/HAL_ADC_Single_GetValue 的实际参数。 */
#define PROJECT_BUTTON_HAL_PIN 100 /* 物理 26 脚为 AGPIO_PIN0。 */
#define PROJECT_HAL_INPUT 33
#define PROJECT_HAL_OUTPUT 36
#define PROJECT_HAL_PULL_UP 1
#define PROJECT_HAL_PULL_DOWN 2

/*-------------------------------------------typedef---------------------------------------------*/
/* 私有 HAL ABI 限于底包，产品只调用上面声明的窄接口。 */
typedef struct
{
    int32_t pin; /* HAL 引脚标识，与物理脚号及 CM GPIO 编号分开。 */
    int32_t mode; /* 输入 33，推挽输出 36。 */
    int32_t pull; /* 上拉 1，下拉 2。 */
    int32_t remap; /* 本次不启用数字外设复用。 */
    int32_t interrupt_mode; /* 0 表示不启用中断。 */
    void (*interrupt_handler)(void); /* 中断回调；轮询模式始终为空。 */
    int32_t keep; /* 沿用 SDK 零初始化的保持选项。 */
} project_hal_gpio_config_t;

_Static_assert(sizeof(project_hal_gpio_config_t) == 40, "GPIO HAL ABI size mismatch");
_Static_assert(offsetof(project_hal_gpio_config_t, interrupt_handler) == 24,
               "GPIO HAL callback ABI mismatch");
_Static_assert(offsetof(project_hal_gpio_config_t, keep) == 32, "GPIO HAL keep ABI mismatch");
/*-------------------------------------------variables-------------------------------------------*/
static bool project_button_input_ready;
static bool project_led_output_ready;
static void (*project_button_notify)(void *);
static void *project_button_argument;
/*-------------------------------------------function---------------------------------------------*/
/* 仅声明当前底包已核验的私有 ABI，接口来源对应内核库和链接映射。 */
extern int32_t HAL_GPIO_Init(project_hal_gpio_config_t *config);
extern int32_t HAL_GPIO_Get_Dir(int32_t pin);
extern uint8_t HAL_GPIO_Read_Pin(int32_t pin);
extern void HAL_GPIO_Write_Pin(int32_t pin, uint8_t level);

/*******************************************************************************
* Function Name  : project_button_interrupt
* Description    : 将 AGPIO0 边沿通知交给按键任务
* Input          : 无
* Output         : 唤醒通知
* Return         : 无
* Attention      : 不打印、不消抖、不执行文件和网络操作
*******************************************************************************/
static void project_button_interrupt(void)
{
    if (project_button_notify)
    {
        project_button_notify(project_button_argument);
    }
}

/*******************************************************************************
* Function Name  : project_button_wakeup_configure
* Description    : 按同版底包 cm_dtr_init 的 HAL 配置接入 AGPIO0 双边沿唤醒
* Input          : notify - 中断通知；argument - 原样传回的上下文
* Output         : AGPIO0 中断及 PMU 唤醒配置
* Return         : 0 - 配置完成；负值 - 参数或 HAL 失败
* Attention      : 配置成功不代表实板验证；失败恢复原轮询输入，不创建 DTR 任务
*******************************************************************************/
int project_button_wakeup_configure(void (*notify)(void *), void *argument)
{
    project_hal_gpio_config_t config = {0};
    int32_t result;
    if (!project_button_input_ready || !notify)
    {
        return -1;
    }
    project_button_argument = argument;
    project_button_notify = notify;
    config.pin = PROJECT_BUTTON_HAL_PIN;
    config.mode = PROJECT_HAL_INPUT;
    config.pull = PROJECT_HAL_PULL_UP;
    config.interrupt_mode = 4; /* 同版 cm_dtr_init：双边沿，HAL 同时配置 AGPIO 唤醒。 */
    config.interrupt_handler = project_button_interrupt;
    result = HAL_GPIO_Init(&config);
    if (result != 0)
    {
        project_button_notify = NULL;
        (void)project_button_input_init();
        return result > 0 ? -result : result;
    }
    return 0;
}

/*******************************************************************************
* Function Name  : project_button_input_init
* Description    : 按当前底包的 AGPIO0 路径初始化物理 26 脚为输入上拉
* Input          : 无
* Output         : 初始化成功标志
* Return         : 0 - 成功；负值 - HAL 初始化或输入方向核验失败
* Attention      : 不调用 CM GPIO26，不创建会控制休眠锁的 SDK DTR 任务
*******************************************************************************/
int project_button_input_init(void)
{
    project_hal_gpio_config_t config = {0};
    int32_t result;
    project_button_input_ready = false;
    config.pin = PROJECT_BUTTON_HAL_PIN;
    config.mode = PROJECT_HAL_INPUT;
    config.pull = PROJECT_HAL_PULL_UP;
    result = HAL_GPIO_Init(&config);
    if (result != 0)
    {
        return -result;
    }
    if (HAL_GPIO_Get_Dir(config.pin) != 0)
    {
        return -5;
    }
    project_button_input_ready = true;
    return 0;
}

/*******************************************************************************
* Function Name  : project_button_input_read
* Description    : 读取 AGPIO0 并转换为低电平按下状态
* Input          : pressed - 按下状态的输出地址
* Output         : pressed - 仅在读取有效时写入
* Return         : 0 - 成功；-1 - 未初始化或参数错误；-5 - 电平无效
* Attention      : 普通任务轮询调用，30ms 消抖由按键器件负责
*******************************************************************************/
int project_button_input_read(bool *pressed)
{
    uint8_t level;
    if (!project_button_input_ready || !pressed)
    {
        return -1;
    }
    level = HAL_GPIO_Read_Pin(PROJECT_BUTTON_HAL_PIN);
    if (level > 1U)
    {
        return -5;
    }
    *pressed = level == 0U;
    return 0;
}

/*******************************************************************************
* Function Name  : project_led_output_init
* Description    : 将物理 96 脚对应的 GPIO_PIN_B 配置为初始低电平输出
* Input          : 无
* Output         : LED 数字输出及就绪标志
* Return         : 0 - 成功；负值 - HAL 初始化或方向、电平回读失败
* Attention      : 该引脚不再作 ADC 采样；先预置低电平，初始化后再次确认关闭
*******************************************************************************/
int project_led_output_init(void)
{
    project_hal_gpio_config_t config = {0};
    int32_t result;
    project_led_output_ready = false;
    config.pin = ML307Y_LED_HAL_PIN;
    config.mode = PROJECT_HAL_OUTPUT;
    config.pull = PROJECT_HAL_PULL_DOWN;
    HAL_GPIO_Write_Pin(config.pin, 0U);
    result = HAL_GPIO_Init(&config);
    if (result != 0)
    {
        return -result;
    }
    HAL_GPIO_Write_Pin(config.pin, 0U);
    if (HAL_GPIO_Get_Dir(config.pin) != 1 || HAL_GPIO_Read_Pin(config.pin) != 0U)
    {
        return -5;
    }
    project_led_output_ready = true;
    return 0;
}

/*******************************************************************************
* Function Name  : project_led_output_write
* Description    : 设置 LED 高亮低灭并核对 HAL 输出寄存器
* Input          : on - true 点亮，false 关闭
* Output         : 物理 96 脚目标电平
* Return         : 0 - 回读一致；-1 - 未初始化；-5 - 回读不一致
* Attention      : 输出回读不等同于 LED 实际亮度或板端电气验证
*******************************************************************************/
int project_led_output_write(bool on)
{
    uint8_t level = on ? 1U : 0U;
    if (!project_led_output_ready)
    {
        return -1;
    }
    HAL_GPIO_Write_Pin(ML307Y_LED_HAL_PIN, level);
    return HAL_GPIO_Read_Pin(ML307Y_LED_HAL_PIN) == level ? 0 : -5;
}

