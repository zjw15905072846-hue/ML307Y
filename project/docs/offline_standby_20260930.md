# 离线待机交付与验证记录

日期：2026-09-30。用户决定先交付固件及验证记录，稍后烧录和实板验收。本轮没有烧录、打开COM12、操作按键或测量电流。

## 交付物与启用边界

两个包均显式启用离线待机，保留UART0，使用相同私有平台配置和存储身份；均为待实板核验的验证包，不标记为量产低功耗已验证。

- 一小时包：[ML307Y_offline_trial_1h.mimgx](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-standby-20260930-100342/ML307Y_offline_trial_1h.mimgx)。
  SHA-256：`8b499b91e091f68978e17ff35987147faf1603087a9aaf71c7edc4c2e59c873d`。
- 六十秒验收包：[ML307Y_offline_trial_60s.mimgx](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-standby-20260930-100342/ML307Y_offline_trial_60s.mimgx)。
  SHA-256：`81ace4475eac2f6cc058476c7a5f40ee1fd9b14ccd5512809363dfd2bca054bd`。
- 包及ELF完整哈希：[out/offline-standby-20260930-100342/SHA256SUMS.txt](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-standby-20260930-100342/SHA256SUMS.txt:1)。

短周期只覆盖业务心跳为60000ms，注册／心跳失败预算仍120000ms、失败休眠仍900000ms。最终构建产物已恢复为一小时离线验证包。源码默认离线开关仍为0；普通构建会保持原在线兼容模式，实板核验后才能调整默认值。

## 实现及保留内容

新增后台网络状态机，依次恢复射频、等待PDP、连接订阅、推进业务、停止MQTT、关闭射频、离线等待。按键中断分别唤醒前台与后台，使用合并标志；关网期间按键请求保留到恢复流程。前台只发布静止条件，不解除后台工作锁。

没有报警时，未完成注册／心跳达到两分钟预算后停止新尝试；声光结束且按键释放才收尾。十五分钟从成功进入离线等待计时。报警、报警持久化失败及控制序号保存失败禁止限时休眠。首次注册／心跳顺序及同次运行注册保留不变。

射频接口使用现有 `cm_modem_set_cfun()` / `cm_modem_get_cfun()`；设置成功且读回匹配才报告关闭，恢复后等待PDP。错误保持工作锁并按期限重试，不设置任何“实板已验证”标志，不使用整机断电或RAM掉电深睡。

既有MQTT停止接口首次请求失败后不会重试，本轮先加入失败复现，再补五秒重试及停止期间不自动重连。离线报警持久化不等待模组查询，仅先采电池；未知信号及时间遵循现有配置。

原有MQTT保活3600秒改动、私有配置、电池3000～4200mV估算设置均保留。现有测试仍使用旧3300mV端点的固定百分比，基线已出现失败；本轮仅调整测试向量和期望值与当前宏一致，没有修改电池生产算法。

完整框架：[project/docs/业务流程.md](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/docs/业务流程.md:1)。

## 验证结果及证据

| 验证层级 | 结果 | 证据 |
|---|---|---|
| 旧业务完成仍在线 | 预期断连断言失败，已记录 | [out/offline-standby-20260930-100342/red.log](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-standby-20260930-100342/red.log:1) |
| 断连请求失败后不重试 | 预期停止断言失败，修复后通过 | [out/offline-standby-20260930-100342/mqtt-stop-red.log](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-standby-20260930-100342/mqtt-stop-red.log:1) |
| C逻辑回归 | 16组RV64模拟器套件及变体通过，包括离线一小时／八小时 | [out/offline-standby-20260930-100342/c-tests.log](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-standby-20260930-100342/c-tests.log:1) |
| Python主机检查 | 38项通过 | [out/offline-standby-20260930-100342/python-tests.log](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-standby-20260930-100342/python-tests.log:1) |
| 六十秒报警包交叉构建 | 通过 | [out/offline-standby-20260930-100342/build-short.log](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-standby-20260930-100342/build-short.log:1) |
| 一小时报警包交叉构建 | 通过 | [out/offline-standby-20260930-100342/build-1h.log](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-standby-20260930-100342/build-1h.log:1) |
| 模板交叉构建 | 本轮重新构建通过 | [out/offline-standby-20260930-100342/build-template.log](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-standby-20260930-100342/build-template.log:1) |
| 双bin配套、ELF入口、产品隔离 | 通过 | [out/offline-standby-20260930-100342/artifacts.log](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-standby-20260930-100342/artifacts.log:1) |
| 保存的两个总包内底包 | 单次解压并与底包ELF段机器码逐字节一致 | [out/offline-standby-20260930-100342/verification.json](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-standby-20260930-100342/verification.json:1) |
| COM12、实体按键、平台与电流 | 待用户烧录后验收 | 未执行，不以编译替代 |

C测试是在RV64模拟器执行，不是Windows原生C执行；模拟器测试中的原子操作替身不证明真实RTOS并发。已有模拟头文件的原子宏重复定义警告保留。原生主机检查指Python测试。

重点覆盖：完整注册／心跳业务回执、离线按键先保存后发、PUBACK不删除、删除失败保持唤醒、心跳期限不被报警改变；无网络或无回执的两分钟预算、十五分钟重试；计时回卷、射频失败及错误读回、停止失败、队列满、关闭期间按键及过期MQTT回调。

## 实板验收步骤和可能问题

1. 先烧录六十秒包，记录固件启动时间及 `offline-standby-enabled=1`、`heartbeat-interval-ms=60000`。需在COM12、115200/8N1采集完整连续日志，保存对应包哈希。
2. 核对注册业务确认、首次心跳业务确认、`network-stopping`、`offline-wait`。后两者只说明应用流程，不独立证明芯片睡眠或射频电流。
3. 在真实低电流等待区间按物理按键，核对本地提示、持久保存、恢复联网、报警业务成功、持久删除以及再次休眠。入睡前后按下、长按、释放重按分别测试；至少连续三轮。
4. 六十秒周期至少三轮，确认每轮只发心跳、不重复注册、不亮灯鸣叫。随后烧录一小时包，核对 `heartbeat-interval-ms=3600000`，观察正式周期；中途报警不改变原心跳期限。
5. 无服务／平台不回复时检查两分钟后进入关闭流程、成功离线后十五分钟再试；等待期间按键应提前唤醒。未确认报警则保持唤醒，声光按原时限结束。
6. CFUN设置失败、恢复驻网失败、MQTT停止失败时应明确报错，不能假报低功耗。断网将增加报警上传延迟；测量从按键到业务确认的实际时长。
7. 明确功耗仪各曲线含义、供电点、USB和串口接线，以同一条件对比在线基线和离线包；记录平均电流、周期电量、LED低电平和PWM停止。不能将未知三条曲线相加或直接归因于某外设。
8. 保留UART0、供电电路及未确认报警重试都可能影响功耗。只有实测通过才能将默认离线开关改为1并形成量产验收结论。

## 基线与修改范围

本轮只访问当前D盘工程进行开发，未迁移到其他工程。基线压缩包及散列已保存在项目内交付目录；包含私有配置，仅供本地恢复，不作为公开交付资料。原私有头文件和原MQTT配置与基线逐字节一致，见上方验证记录。按测试、接口、调度、构建配置、文档分批写入，每批不超过三份源文件；未修改协议字节定义、Topic、凭据或报警节奏。
