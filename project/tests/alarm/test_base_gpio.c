/*------------------------------------------includes--------------------------------------------*/
#include <assert.h>
#include <stdio.h>
#include "../../src/ml307y/base_gpio.c"

/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
static int32_t init_result;
static int32_t direction;
static uint8_t input_level = 1;
static uint8_t output_level;
static bool output_readback_fault;
static project_hal_gpio_config_t last_config;
static unsigned writes;
static unsigned wake_notifications;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : notify_wakeup
* Description    : 记录中断通知，不在中断执行业务
* Input          : argument - 计数器地址
* Output         : 通知次数
* Return         : 无
* Attention      : 模拟通知不证明实板唤醒
*******************************************************************************/
static void notify_wakeup(void *argument)
{
    ++*(unsigned *)argument;
}
/*******************************************************************************
* Function Name  : HAL_GPIO_Init
* Description    : 捕获底包 HAL 参数并注入初始化结果
* Input          : config - 真实桥接层生成的 ABI 参数
* Output         : last_config - 参数快照
* Return         : init_result - 模拟 HAL 返回值
* Attention      : 不模拟 CM 编号，直接核对 HAL 引脚和结构布局
*******************************************************************************/
int32_t HAL_GPIO_Init(project_hal_gpio_config_t *config)
{
    last_config = *config;
    return init_result;
}

/*******************************************************************************
* Function Name  : HAL_GPIO_Get_Dir
* Description    : 模拟初始化后的方向回读
* Input          : pin - HAL 引脚
* Output         : 无
* Return         : direction - 方向枚举
* Attention      : 只允许已经初始化的引脚
*******************************************************************************/
int32_t HAL_GPIO_Get_Dir(int32_t pin)
{
    assert(pin == last_config.pin);
    return direction;
}

/*******************************************************************************
* Function Name  : HAL_GPIO_Read_Pin
* Description    : 模拟按键输入与 LED 输出寄存器回读
* Input          : pin - HAL 引脚
* Output         : 无
* Return         : 输入或输出电平，可注入回读不一致
* Attention      : 只允许 AGPIO0 和 GPIO_PIN_B
*******************************************************************************/
uint8_t HAL_GPIO_Read_Pin(int32_t pin)
{
    assert(pin == 100 || pin == 41);
    return pin == 100 ? input_level : (output_readback_fault ? !output_level : output_level);
}

/*******************************************************************************
* Function Name  : HAL_GPIO_Write_Pin
* Description    : 记录 LED 目标电平
* Input          : pin/level - HAL 引脚和电平
* Output         : 写入计数及电平
* Return         : 无
* Attention      : 严禁写入按键或把物理 96 作为 HAL 编号
*******************************************************************************/
void HAL_GPIO_Write_Pin(int32_t pin, uint8_t level)
{
    assert(pin == 41 && level <= 1U);
    output_level = level;
    ++writes;
}

/*******************************************************************************
* Function Name  : main
* Description    : 验证私有 HAL ABI、实际引脚参数、初始关闭及失败传播
* Input          : 无
* Output         : 测试结果
* Return         : 0 - 全部通过
* Attention      : 输出寄存器模拟不等于真实 LED 或按键验收
*******************************************************************************/
int main(void)
{
    bool pressed = true;
    assert(project_button_input_read(&pressed) < 0 && pressed);
    assert(project_led_output_write(true) < 0 && writes == 0);
    init_result = 1;
    assert(project_button_input_init() == -1);
    assert(project_button_input_read(&pressed) < 0 && pressed);
    init_result = 0;
    direction = 1;
    assert(project_button_input_init() == -5);
    direction = 0;
    assert(project_button_input_init() == 0);
    assert(last_config.pin == 100 && last_config.mode == 33 && last_config.pull == 1);
    assert(last_config.interrupt_mode == 0 && last_config.interrupt_handler == NULL);
    assert(project_button_input_read(&pressed) == 0 && !pressed);
    input_level = 0;
    assert(project_button_input_read(&pressed) == 0 && pressed);
    input_level = 2;
    assert(project_button_input_read(&pressed) < 0 && pressed);
    assert(project_button_input_read(NULL) < 0);
    assert(project_button_wakeup_configure(NULL, NULL) < 0);
    init_result = 1;
    assert(project_button_wakeup_configure(notify_wakeup, &wake_notifications) < 0);
    init_result = 0;
    assert(project_button_input_init() == 0); /* HAL 持续失败后的恢复须重新初始化。 */
    assert(project_button_wakeup_configure(notify_wakeup, &wake_notifications) == 0);
    assert(last_config.pin == 100 && last_config.interrupt_mode == 4);
    assert(last_config.interrupt_handler != NULL);
    last_config.interrupt_handler();
    last_config.interrupt_handler();
    assert(wake_notifications == 2U);
    init_result = 2;
    assert(project_led_output_init() == -2);
    assert(project_led_output_write(true) < 0);
    init_result = 0;
    assert(project_led_output_init() == -5);
    direction = 1;
    output_readback_fault = true;
    assert(project_led_output_init() == -5);
    output_readback_fault = false;
    assert(project_led_output_init() == 0 && output_level == 0);
    assert(last_config.pin == 41 && last_config.mode == 36 && last_config.pull == 2);
    assert(last_config.interrupt_mode == 0 && last_config.interrupt_handler == NULL);
    assert(project_led_output_write(true) == 0 && output_level == 1);
    assert(project_led_output_write(false) == 0 && output_level == 0);
    output_readback_fault = true;
    assert(project_led_output_write(true) == -5);
    puts("base GPIO: ABI, AGPIO0, GPIO_PIN_B, initial off and HAL errors passed");
    return 0;
}
