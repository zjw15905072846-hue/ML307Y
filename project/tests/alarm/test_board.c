/*------------------------------------------includes--------------------------------------------*/
#include "board.h"
#include "alarm_button/alarm_core.h"
#include <assert.h>
#include <stdio.h>
/*-------------------------------------------define---------------------------------------------*/
/*-------------------------------------------typedef---------------------------------------------*/
/*-------------------------------------------variables-------------------------------------------*/
static int init_calls;
static int writes;

/*-------------------------------------------function---------------------------------------------*/
/*******************************************************************************
* Function Name  : init_hw
* Description    : 计数模拟硬件初始化
* Input          : 见签名；context/store 为独占上下文，now 为单调毫秒
* Output         : 上下文及显式输出参数
* Return         : 见签名；状态码 ALARM_OK 表示成功，负值表示失败
* Attention      : 由一个业务任务串行调用；存储端口必须提供原子提交
*******************************************************************************/
static bool init_hw(void *u, const alarm_board_config_t *c)
{
    (void)u;
    (void)c;
    init_calls++;
    return true;
}

/*******************************************************************************
* Function Name  : read_key
* Description    : 模拟输入
* Input          : 见签名；context/store 为独占上下文，now 为单调毫秒
* Output         : 上下文及显式输出参数
* Return         : 见签名；状态码 ALARM_OK 表示成功，负值表示失败
* Attention      : 由一个业务任务串行调用；存储端口必须提供原子提交
*******************************************************************************/
static bool read_key(void *u, bool *p)
{
    (void)u;
    *p = false;
    return true;
}

/*******************************************************************************
* Function Name  : write_output
* Description    : 模拟输出
* Input          : 见签名；context/store 为独占上下文，now 为单调毫秒
* Output         : 上下文及显式输出参数
* Return         : 见签名；状态码 ALARM_OK 表示成功，负值表示失败
* Attention      : 由一个业务任务串行调用；存储端口必须提供原子提交
*******************************************************************************/
static bool write_output(void *u, bool l, bool b)
{
    (void)u;
    (void)l;
    (void)b;
    writes++;
    return true;
}

/*******************************************************************************
* Function Name  : battery_voltage
* Description    : 模拟内部电池电压
* Input          : 见签名；context/store 为独占上下文，now 为单调毫秒
* Output         : 上下文及显式输出参数
* Return         : 见签名；状态码 ALARM_OK 表示成功，负值表示失败
* Attention      : 由一个业务任务串行调用；存储端口必须提供原子提交
*******************************************************************************/
static bool battery_voltage(void *u, uint16_t *v)
{
    (void)u;
    *v = 3700;
    return true;
}

/*******************************************************************************
* Function Name  : main
* Description    : 未核验引脚和冲突映射不得访问硬件
* Input          : 见签名；context/store 为独占上下文，now 为单调毫秒
* Output         : 上下文及显式输出参数
* Return         : 见签名；状态码 ALARM_OK 表示成功，负值表示失败
* Attention      : 由一个业务任务串行调用；存储端口必须提供原子提交
*******************************************************************************/
int main(void)
{
    alarm_board_t b;
    alarm_board_operations_t operations = {0, init_hw, read_key, write_output, battery_voltage};
    alarm_board_config_t c = {-1, 41, 4, false, false, 0};
    assert(alarm_board_init(&b, &c, &operations) == ALARM_ERROR_NOT_READY && init_calls == 0);
    c.pinmap_verified = true;
    c.key_sdk_pin = 41;
    assert(alarm_board_init(&b, &c, &operations) == ALARM_ERROR_CONFIG && init_calls == 0);
    c.key_sdk_pin = 3;
    assert(alarm_board_init(&b, &c, &operations) == 0 && init_calls == 1);
    assert(alarm_board_output(&b, true, true));
    assert(writes == 2);
    assert(alarm_board_output(&b, true, true) && writes == 2);
    puts("board: unverified/conflicting resources blocked; single output owner passed");
    return 0;
}
