# 一键报警联网浅休眠交付记录

本轮软件已实现并构建，尚未烧录或完成实板低功耗验收。仅处理本 D 盘工程，保留此前未提交改动、私有配置、协议及报警节奏。

## 行为

```text
设备运行
├─ 启动：恢复持久记录 → 网络/CONNECT → SUBACK → 注册业务确认 → 首次心跳
├─ 在线空闲：LED 低电平、PWM 关闭、停止周期采样
│  ├─ 保留按键中断、4G/MQTT 与 UART0
│  ├─ 前后台均无工作需求时，解除工作锁并允许浅休眠
│  ├─ 按键边沿：唤醒 → 30ms 消抖 → 持久报警 → 原声光节奏
│  └─ 每12小时：只处理心跳，不开启声光
├─ 报警未完成
│  ├─ 未收到匹配 FF/00：保留记录、间隔重试、保持工作锁
│  ├─ PUBACK：仅传输确认，不能删除报警或视为业务成功
│  ├─ 持久保存失败：锁定未完成状态，禁止因空队列而休眠
│  └─ 提示最长34秒结束；未确认不继续鸣叫，也不停止重试
└─ 报警完成
   ├─ 所有事件业务确认且删除落盘成功
   ├─ 声光结束、按键稳定释放、无待保存请求
   └─ 再次允许联网浅休眠
```

网络保活、下行和 UART0 活动仍可能唤醒处理器；不承诺整板仅按键耗电。采用 `CM_PM_SLEEP_MODE_LIGHT`，不启用 RAM 掉电深睡。唤醒配置失败则保留工作锁和按键轮询。平台长期不确认时，按照批准选择保持唤醒。

## 配置与接口

- 配置段：[project/inc/alarm_button/product_config.h](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/inc/alarm_button/product_config.h:14)。`ALARM_BUTTON_SLEEP_ENABLED=1`；设为0可回退常醒。`ALARM_BUTTON_SLEEP_DIAGNOSTICS=1` 控制普通任务中的休眠统计输出，回调内不打印。
- 实际构建核验：[out/sleep-20260928/verification.json](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/sleep-20260928/verification.json:1)，确认12小时心跳、明文模式及厂商字段0x4872保持不变。
- 按键接口新增 `wake_configured`，原 `wake_verified` 保持false；接口定义见 [project/inc/product_interface.h](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/inc/product_interface.h:18)。
- 系统接口新增后台工作锁和诊断入口，见 [project/inc/system_interface.h](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/inc/system_interface.h:48)；传输接口新增事件通知及下一等待期限，见 [project/inc/mqtt/kaiwan_cloud.h](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/inc/mqtt/kaiwan_cloud.h:121)。

## 定位索引

| 工程相对路径、文件名和准确行号 | 所在函数与职责 |
|---|---|
| [project/src/ml307y/base_gpio.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/base_gpio.c:67) | project_button_wakeup_configure：AGPIO0=100、双边沿模式4、失败恢复轮询 |
| [project/src/ml307y/alarm_key.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/alarm_key.c:73) | ml307y_alarm_key_set_wakeup：配置状态独立于实板验证标志 |
| [project/src/ml307y/alarm_buzzer.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/alarm_buzzer.c:29) | ml307y_alarm_buzzer_set：静音关闭 PWM，发声恢复原频率占空比 |
| [project/src/ml307y/system_port.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/system_port.c:337) | ml307y_update_power_hold：合并前后台工作锁，防止交叉解锁 |
| [project/src/ml307y/mqtt_port.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/mqtt_port.c:184) | ml307y_next_wait：网络事件、连接及发送超时期限 |
| [project/src/alarm_button/alarm_runtime.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_runtime.c:890) | alarm_process_background：业务完成后报告在线空闲，保存失败阻止休眠 |
| [project/src/alarm_button/alarm_runtime.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_runtime.c:958) | alarm_read_front_requests：空闲按网络/心跳期限阻塞，后台操作持锁 |
| [project/src/alarm_button/alarm_runtime.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_runtime.c:1252) | alarm_wait_for_next_event：按键通知、消抖、声光及持久请求约束 |
| [project/src/ml307y/product_start.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/product_start.c:144) | product_boot_task：六次诊断后永久阻塞；信号量失败时延时降级 |

## 验证结果

- RV64模拟器：14组C测试，加明文运行时、蜂鸣器宏覆盖两个变体通过。[C回归日志](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/sleep-c-tests-final.log:1)。不是原生主机执行，也不是实板证明。
- 定向休眠回归：覆盖工作锁所有权、配置失败降级、入睡边界按键、通知队列满、未确认/未保存/声光未完禁止休眠、32位计时回卷及心跳等待时不重复采样。[调度测试](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/sleep-scheduler-tests.log:1)；[工作锁测试](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/sleep-power-tests.log:1)。测试仅注入单线程事件交错，不代表真实RTOS并发压力验证。
- Python：38项通过。[Python检查日志](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/sleep-python-tests-final.log:1)。
- 报警与模板的交叉构建、产品隔离、配套底包检查通过。底包和最终烧录包均解压一次后与ELF机器码逐字节比较通过。[产物检查日志](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/sleep-artifact-checks.log:1)。
- 已记录修改前失败：[未调用PWM关闭接口](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/sleep-red-pwm.log:1)、[在线状态不能业务空闲](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/sleep-red-runtime.log:1)、[缺少网络通知接口](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/sleep-red-mqtt.log:1)；新增采样边界先失败再修复，见 [心跳重复采样复现](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/sleep-red-heartbeat-sampling.log:1)。
- 模拟器日志保留既有原子宏重定义警告；部分测试以单线程等价操作代替模拟器不支持的原子交换，固件仍编译实际原子操作。

## 交付与实板待验

- 配套产物清单及完整SHA-256：[报警固件清单](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/products/alarm_button/alarm_board_v1/ml307y/release.json:1)。底包标识 `c7b11ec45f724c1fb972`。
- 烧录文件：[ML307Y_alarm_button.mimgx](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/products/alarm_button/alarm_board_v1/ml307y/image/ML307Y_alarm_button.mimgx)（二进制文件无行号）。SHA-256：`c684f5becf5b80c002b16fb306c9d6bfb8cb61869addfe8e0822169d9e902e2c`。
- 原始基线及散列：[基线清单](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/sleep-baseline-20260928-175416/manifest.json:1)；本轮代码差异散列：[修改清单](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/sleep-20260928/changes.json:1)。
- COM12/115200/8N1已打开并监听15秒，收到0字节。未烧录新包，因此不能用于判断新包休眠、按键唤醒或死机。[串口采集状态](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/sleep-20260928/serial-status.json:1)。

实板验收需使用本次配套包，并记录完整启动及按键日志：

1. 确认无反复复位；看到按键唤醒配置成功、SUBACK、注册及心跳业务成功。
2. 无待确认报警时测静态电流和SDK休眠状态；检查物理96脚为低、74脚无PWM。
3. 实际进入休眠后按物理26脚，验证30ms消抖后的新报警、原声光节奏、平台业务确认及再次休眠；持续重按及临界时刻按下不得丢失。
4. 断网、拒绝回执或保存/删除失败时，验证记录保留且禁止休眠；恢复后重新完成业务。
5. 短周期测试配置验证定时唤醒只发心跳；正式交付包保持12小时，再完成长时间在线观察。

未执行烧录、实体按键、电平/波形和整板电流测量；这些项目仍待实板验收，不以模拟器或打包检查替代。
