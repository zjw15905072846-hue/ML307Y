#pragma once
/*------------------------------------------includes--------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
/*-------------------------------------------typedef---------------------------------------------*/
/* 读取真实毫伏值；false 表示不可用，不得由此虚构充电状态或电量百分比。 */
typedef bool (*battery_read_mv_fn)(void *user, uint16_t *millivolts);

/* 采样入口由板或平台绑定，接口本身不指定 ADC 通道。 */
typedef struct
{
    void *user; /* 与采样实现绑定的上下文。 */
    battery_read_mv_fn read_mv; /* 成功时填写 millivolts，失败时输出无效。 */
} battery_if_t;

/*-------------------------------------------function---------------------------------------------*/
