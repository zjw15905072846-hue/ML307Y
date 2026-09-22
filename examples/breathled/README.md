# breathled APP 简介

## **呼吸灯功能介绍**

led逐渐由亮到灭、由灭到亮带来的亮度层次变化，类似于呼吸效果。

## **APP实现功能**

本APP基于pwm功能，实现网络状态提示功能，包括以下子功能：

1. 未成功驻网时，led熄灭；
2. 成功驻网后，启动定时器，动态调整pwm的输出占空比，控制led的亮度，实现呼吸效果。

## **APP执行流程**

1、进入主程序后等待驻网，此阶段led不显示。
2、驻网成功后，定时器启动，定时发出消息通知主程序调整pwm占空比。
osMessageQueuePut(s_breath_msg, &msg, 0, 0);

3、主程序接收消息，调整占空比，当占空比低于100%时，占空比递增；占空比达到100%后，占空比递减。循环往复达到呼吸效果。
s_breath_flag = period_h == BREATH_LED_PWM_PERIOD ? false : s_breath_flag; //确认呼吸方向
s_breath_flag = period_h == 0 ? true : s_breath_flag;

## **APP使用说明**

- 是否需要外设支撑：pwm端口外接led驱动电路


---
