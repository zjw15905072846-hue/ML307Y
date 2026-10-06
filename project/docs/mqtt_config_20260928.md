# MQTT 配置加载与注册、上报打印

后续实板进展：12:41～12:42 的日志已确认配置生效，出现 `mode-plaintext-hex` 和 `mqtt-started`，随后设备自行重启；用户明确表示期间没有手动断电或复位。平台注册和报警上报尚未完成，自动重启根因仍在定位。

当前正常输出已更新为 `mqtt-trace-20260928` 诊断版，增加上电原因以及轮询、原子交换、SDK 状态、PDP 查询、连接请求和回调阶段日志，保留注册及上报打印。详见[诊断包核验记录](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-provision-20260928/trace-verification.json:1)。本页下面的配置修复测试描述对应此前 mqtt-config 版本。

用户 2026-09-28 12:33 提供的实板日志已确认 mqtt-bootfix 版本完成初始化、调度持续运行，按键后的 1980 字节存储写入和读回均成功。当前故障是云配置未进入该次编译的固件：MQTT 默认关闭，厂商、地址和账号均为空，同时恢复了默认 AES 模式。

已对照当前输出 ELF 和此前交付的启动修复包：当前输出只有缺配置诊断；此前交付包包含明文模式和联调占位配置。不能将头文件存在、旧包校验通过视为这次重新编译已经带入配置。

本次修复让普通报警产品构建自动选用项目内已有的私有配置；显式指定其他配置仍优先，模板不会自动带入报警账号。私有配置内容及用户提供的凭据没有改动。

## 编译与输出

在当前工程根目录执行：

```powershell
python -B project/tools/build_product.py alarm_button
```

[编译入口](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/tools/build_product.py:13)的 main 会打印 `Provision header: project/private/alarm_cloud.h`，依次编译底包和应用并核验打包。私有配置保存在[alarm_cloud.h](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:1)。

输出[ML307Y_alarm_button.mimgx](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/products/alarm_button/alarm_board_v1/ml307y/image/ML307Y_alarm_button.mimgx)，当前启动标识为 `mqtt-trace-20260928`。此前配置修复版已保留为[独立备份](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-provision-20260928/ML307Y_alarm_button_mqtt_config_20260928.mimgx)，该备份 SHA-256 见[配置版核验清单](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-provision-20260928/verification.json:1)。

## 注册与上报打印

当前[打印宏](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:39)为 `ALARM_BUTTON_PACKET_LOG_ENABLED 1`。注册 registration、心跳 heartbeat、报警 alarm 均输出提交给 MQTT 的同一份发送缓冲，而不是另外拼接示例。

以下仅表示日志格式，省略号是占位，不是已收到的实板报文：

```text
[project][mqtt-tx] type=registration seq=... bytes=... queue_result=...
[project][mqtt-tx] seq=... topic=iot/devices/本机IMEI/sys/fire/data/up
[project][mqtt-tx] seq=... offset=0 data=完整协议帧十六进制文本
```

报警发送时 type 为 alarm。长报文按 160 字符分段，依据相同 seq 和递增 offset 拼接。queue_result 是本地传输入队结果；平台注册和报警成功仍由匹配的业务回执确认。MQTT 未就绪时不会假装已经发送报文。

## 代码定位

| 工程相对路径、文件名 | 函数或配置段 | 作用 |
| --- | --- | --- |
| [project/build/selection.py](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/build/selection.py:151) | resolve_provision | 默认配置与显式配置选择、产品隔离、项目路径约束 |
| [project/build/selection.py](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/build/selection.py:197) | apply | 向真实编译环境加入私有头宏，构建清单记录所选配置路径 |
| [project/tools/build_product.py](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/tools/build_product.py:29) | main | 普通命令自动传入已选配置，并打印配置路径 |
| [project/tests/test_build_graph.py](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/tests/test_build_graph.py:14) | test_alarm_default_build_tracks_existing_private_config | 用真实 SCons 构建图复现漏配置并验证默认依赖 |
| [project/src/alarm_button/alarm_runtime.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_runtime.c:228) | alarm_publish_frame | 注册、心跳及报警编码、入队后传递实际发送缓冲给打印接口 |
| [project/src/ml307y/system_port.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/system_port.c:254) | ml307y_packet_log | 复用 UART0 分行完整打印类型、序号、Topic、HEX 和入队结果 |
| [project/src/ml307y/product_start.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/product_start.c:61) | product_boot_initialize | 当前诊断版启动标识及上电原因 |

## 测试与实板边界

- 修改前，普通构建没有私有头依赖的复现测试失败；修复并构建新底包后，38 项 Python 测试通过。
- 重新运行既有明文 RV64 测试：注册、心跳、报警均调用打印接口；打印内容、Topic、序号和结果与实际发布缓冲逐字节匹配，失败发送也有对应日志。本次没有修改这些 C 业务实现。
- 最终 ELF 已确认存在明文模式、未知电量联调占位和本机 IMEI 上行 Topic 后缀；默认云关闭和缺账号诊断已不再进入该构建。反汇编确认发布后存在报文打印回调调用。
- 当前保留 12 小时业务心跳、厂商标识 4872、未知电量 0xFF、关闭 AES；factoryCode 不阻塞本轮明文联调。

实板后续核验：烧入本次版本后，应看到新启动标识及 `mode-plaintext-hex`，随后核对网络及订阅阶段、registration 的完整报文；平台注册成功后按一次实体按键，核对 alarm 报文和业务回执。若网络或平台没有响应，保留阶段日志和待确认记录，不用本地入队成功代替业务成功。此前实板已确认的初始化和存储成功，不等于本次平台接入已经通过。

诊断版新增日志判读：若停在 `mqtt-poll-enter`，先核对原子交换是否返回；出现 `mqtt-poll-atomic-ok` 后再看 SDK 状态和 PDP 查询；出现 `mqtt-connect-enter` 而无 `mqtt-connect-result`，说明连接调用尚未正常返回。若返回成功但没有 CONNACK，应继续核对 SDK 异步任务、网络及设备复位原因。日志只能定位最后完成的阶段，不能单凭相邻日志断定具体硬件或 SDK 根因。上电原因枚举见[cm_pm.h](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/include/cmiot/cm_pm.h:33)，0 为正常上电，2 为软件复位，5 为硬件看门狗复位；其余值按 SDK 枚举解释。
