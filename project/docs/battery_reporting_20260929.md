# 电池电压与估算电量上报（2026-09-29）

用户批准同时上报实测电压和按电压估算的百分比。软件与固件包验证已完成；本次尚未烧录新包，实板采样精度、持续运行和平台字段显示待验收。

## 文档依据与估算边界

用户提供的原理图第 1 页：锂电池正极、TP4054 的 BAT 和模组 VBAT 同网；使用 SDK 内部 VBAT 采样，不占用控制 LED 的 ADC1。CHRG 未接模组，不能生成充电或充满状态。

协议第 24 页规定注册帧 Byte63 为电压、Byte65 为百分比；第 32～33 页规定手报事件帧 Byte29 为电压、Byte31 为百分比。电压单位为 0.1 V，百分比取 0～100；这些是完整二进制帧的一基字节编号，明文 HEX 文本中每个字节占两个字符。[注册字段提取文本](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/battery-doc-review-20260929/protocol.txt:807)；[手报字段提取文本](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/battery-doc-review-20260929/protocol.txt:1164)。

两份文档均未给出电芯容量、放电曲线或电压到百分比算法。用户同意采用电压估算；以下端点为本轮选择的联调配置，并非协议规定或电芯标定值。协议示例的 3.7 V 与 100% 不构成换算关系。

## 实现与定位

| 工程相对路径、文件名 | 函数或配置段 | 行号与入口 |
|---|---|---|
| [project/inc/ml307y/alarm_battery.h](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/inc/ml307y/alarm_battery.h:5) | 估算端点、采样范围、样本数宏 | 第 5 行 |
| [project/src/ml307y/alarm_battery.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/alarm_battery.c:26) | ml307y_alarm_battery_read_voltage | 第 26 行 |
| [project/src/ml307y/alarm_battery.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/alarm_battery.c:64) | ml307y_alarm_battery_estimate_percent | 第 64 行 |
| [project/inc/product_interface.h](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/inc/product_interface.h:42) | alarm_battery_interface_t.estimate_percent | 第 42 行 |
| [project/src/alarm_button/alarm_runtime.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_runtime.c:672) | alarm_collect_device_info | 第 672 行 |
| [project/src/alarm_button/handset_payload.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/handset_payload.c:42) | kaiwan_handset_telemetry，复用现有报文字段 | 第 42 行 |

默认每次读取 5 次 VBAT，插入排序后取中值；样本数可配置为 3、5、7、9。任意一次 SDK 失败或读数不在 2500～4500 mV 内，本次采样失败，不沿用旧值。采样与估算都在现有后台路径执行，不新增任务。

默认 3300 mV 对应 0%，4200 mV 对应 100%；范围内线性插值、四舍五入并限幅。百分比始终由同一次滤波后的毫伏值计算。它不等同库仑计测得的剩余容量，100% 也不代表已检测到充电结束。

| 电压 | 电压字段 | 估算百分比 | 百分比字段 |
|---|---|---|---|
| 3.30 V | 0x21 | 0% | 0x00 |
| 3.70 V | 0x25 | 44% | 0x2C |
| 3.75 V | 0x26，按现有 0.1 V 四舍五入 | 50% | 0x32 |
| 4.00 V | 0x28 | 78% | 0x4E |
| 4.20 V | 0x2A | 100% | 0x64 |

初次身份采集、新报警保存和周期心跳到期走现有采样触发点。注册和首次心跳可复用当前快照；后续心跳到期刷新。报警先保存事件快照再发送，重试保留原事件的电压和电量，不用最新读数覆盖旧记录。采样失败清除电压及百分比有效位，继续沿用已批准的 0xFF 未知值约定；估算接口缺失、失败或返回大于 100 时仅百分比无效。报文日志复用现有 UART0 的实际发送缓冲打印。

## 验证结果

- 先增加中值测试，在旧单次读取实现上得到断言失败；修复后通过。先增加业务采样接入测试，复现百分比有效位缺失，再完成业务接入。
- 15 组 RV64 模拟器 C 测试及配置变体通过，覆盖 3/5/9 次采样、空满端点覆盖、所有有效毫伏值的单调性、逐次采样失败、越界、未知值、恢复、注册/心跳/报警实际发送字节，以及旧报警快照不被新采样覆盖。[完整记录](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/battery-validation-20260929/simulation.log:1)
- 38 项 Python 回归通过。[结果](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/battery-validation-20260929/python-tests.log:41)
- 偶数采样数、过大采样数、空满端点相同及满电端点超范围，均在编译阶段拒绝。[记录](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/battery-validation-20260929/config-guards.log:1)
- 报警及模板的配套底包、应用均构建成功，产品入口与隔离校验通过。最终包中的底包解压一次与底包 ELF 机器码一致；应用部分也另行解压一次与应用 ELF 一致，保留区压缩描述头按打包规则单独处理。[产品校验](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/battery-validation-20260929/artifacts.log:1)；[应用机器码校验](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/battery-validation-20260929/application-machine-code.log:1)
- 模拟器仍有原有 CM OS 测试桩原子宏重复定义警告；本次报警应用构建日志未检出 warning/error。

## 交付与实板验收

新包：[ML307Y_alarm_button_battery_20260929.mimgx](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/battery-validation-20260929/ML307Y_alarm_button_battery_20260929.mimgx)。

SHA-256：`c211e693d5ef0d409f6eca6413b84f47c77a76e1d88e6c4dbb2c92454940ccac`。

本轮按小批次保存了修改前文件及固件；保留已有未提交修改，未提交或推送代码。[修改与基线哈希清单](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/battery-validation-20260929/change-manifest.json:1)。

待用户烧录该包后验收：

1. 用万用表测 VBAT，对照 COM12、115200/8N1 中注册、心跳和按键报警的实际发送帧及平台字段；电压编码本身有 0.1 V 量化，ADC 精度需另行实测。
2. 比较静置、联网发射、蜂鸣器鸣叫和接入充电时的读数。中值只能抑制孤立异常，不能消除持续负载压降或保证充电期间的容量估算准确。
3. 改变实际电池电压，确认平台百分比随读数变化；后续使用真实电芯数据调整估算参数。
4. 弱网重试期间再次按键，确认每个报警保留自身采样且只有匹配业务回执删除事件；继续观察至少两至三轮业务心跳。

尚未取得新包的实板启动日志、万用表对比或平台确认。构建通过、包校验通过及 MQTT 入队成功都不能代替这些结果。
