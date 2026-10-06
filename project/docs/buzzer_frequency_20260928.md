# XCL-5020ATP 蜂鸣器声频调整

日期：2026-09-28。仅修改当前 D 盘工程；用户已批准并明确要求可编辑宏。

## 结果与依据

规格书第 2 页要求 4000 Hz、50% 方波；第 4 页给出外部三极管驱动电路。当前板原理图中 PWM0 通过 Q3 驱动蜂鸣器，对应模组物理 74 脚。旧配置频率为 0，只输出 GPIO 高低电平，未产生声频。现使用已有 PWM 驱动，默认周期 250000 ns、高电平 125000 ns；静音高电平时间为 0。

依据：[规格书文字提取](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/buzzer-20260928/datasheet.txt:28)；SDK 示例同样将物理 74 脚复用为 FUNCTION1/PWM0，见 [breathled.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/examples/breathled/src/breathled.c:56)。

## 修改入口

| 工程相对路径与文件名 | 配置段或函数 | 作用 |
|---|---|---|
| [project/inc/ml307y/alarm_buzzer.h](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/inc/ml307y/alarm_buzzer.h:8) | define | ALARM_BUZZER_FREQUENCY_HZ=4000U、ALARM_BUZZER_DUTY_PERCENT=50U；以后只改这两个宏 |
| [project/src/ml307y/alarm_buzzer.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/alarm_buzzer.c:29) | ml307y_alarm_buzzer_set | 从宏计算占空比，静音与失败重试保留原逻辑 |
| [project/src/ml307y/product_start.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/product_start.c:93) | product_boot_initialize | 用声频宏初始化，原就绪日志增加实际 Hz 与占空比 |
| [project/build/selection.py](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/build/selection.py:102) | select / apply | 强制声明 PWM:0，移除生成的旧频率宏，拒绝清单重复定义 buzzer_hz |

宏范围为 200～20000 Hz、1～99%；非法值在编译期拒绝。4000 Hz、50% 是本型号规格条件，调到其他频率不保证相同响度。已有有源 GPIO 兼容接口仍保留，但本板启动宏禁止 0 Hz。

## 验证

- 先补测试：旧配置没有输出 PWM，复现断言失败，见 [驱动复现](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/buzzer-20260928/devices-red.log:1)。缺少 PWM 资源与旧清单覆盖频率也先复现失败。
- 12 组 RV64 模拟器 C 测试通过，另以 2000 Hz、25% 编译并运行同一器件测试，验证宏会改变真实驱动调用；覆盖初始化静音、发声、重复开关、PWM 失败后重试及已有声光节奏。见 [C 测试](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/buzzer-20260928/c-tests.log:1)。测试桩存在既有原子宏重定义警告，本次未改动该桩；不等于原生主机或实板测试。
- 30 项 Python 检查通过，包含非法频率、占空比、边界配置和产品资源验证。见 [Python 测试](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/buzzer-20260928/python-tests.log:1)。第一次运行的 3 项构建图检查因新底包目录尚未生成而失败；完成底包构建后重新运行全部检查通过，首次日志已保留。
- 报警和模板产品均已构建，固件编译日志无 warning/error 诊断；配套底包、接口表、应用和产品隔离检查通过。见 [配套核验](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/buzzer-20260928/artifacts.log:1)。目标文件反汇编确认蜂鸣器初始化实参为 4000 Hz。

## 烧录包

[ML307Y_alarm_button.mimgx](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/products/alarm_button/alarm_board_v1/ml307y/image/ML307Y_alarm_button.mimgx)

生成时间：2026-09-28 09:41:54；底包标识：`722b56135a0f6fb6874a`。

SHA-256：`c40af5dc486a31bbf48dc3d50cee1dd615c0d0dfed09f1c69381f0ae589a5b3e`。

保留用户已配置的私有 MQTT/AES 参数和 12 小时业务心跳。本次未执行烧录、未连接真实平台、未解除原存储失败及云配置待补齐条件。

## 实板风险与验收

1. 烧入本次配套包，UART0 应出现 `alarm buzzer ready hz=4000 duty=50%`；若数值不符先核对所烧包。
2. 按下按键：首秒静音，随后三秒每 250 ms 鸣/停；释放再按应重新开始提示。验证停止时不会持续鸣叫。
3. 若仍无声或音量小，测物理 74 脚/Q3 输入是否为周期 250 us、高电平 125 us 的方波，再核对蜂鸣器供电与 Q3 驱动电路；源码、模拟器通过不能证明实际波形或响度。
4. 原报警保存失败 -2 和真实平台闭环仍是独立未完成项；蜂鸣器改动不能作为这些故障已修复的证据。
