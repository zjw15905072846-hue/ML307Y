#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>

/*-------------------------------------------define---------------------------------------------*/
/* 物理 96 脚在本版底包 HAL 中为 GPIO_PIN_B，不是 CM GPIO41。 */
#define ML307Y_LED_HAL_PIN 41

/*-------------------------------------------function---------------------------------------------*/
/* 底包扩展：物理 26 脚配置为 AGPIO0 输入上拉，不创建 SDK DTR 任务。 */
int project_button_input_init(void);
/* 返回 0 表示读取有效；按下接地为 true，失败时保持输出参数原值。 */
int project_button_input_read(bool *pressed);
/* 装配 AGPIO0 双边沿唤醒；仅回调通知，失败继续保留轮询输入。 */
int project_button_wakeup_configure(void (*notify)(void *), void *argument);
/* 底包扩展：物理 96 脚配置为数字输出，初始低电平灭灯。 */
int project_led_output_init(void);
/* 高电平亮、低电平灭；返回 0 表示输出寄存器回读一致。 */
int project_led_output_write(bool on);
