# 明文 MQTT 联调交付（2026-09-28）

用户本轮要求先跳过第四部分数据加密，发送注册、心跳、报警，并通过 COM12 打印实际报文。该记录取代旧交付中“必须补 factoryCode 才能尝试发送”的限制，仅限本轮明文模式。正式平台验证状态仍为未确认。

## 烧录包

- [ML307Y_alarm_button.mimgx](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/products/alarm_button/alarm_board_v1/ml307y/image/ML307Y_alarm_button.mimgx)
- 生成时间：2026-09-28T11:14:20；大小：10499021 字节。
- SHA-256：`0489ad239ba605c9916f2029b95c7752cd6a127b436d2c35888cac4a066932a7`。
- 启动版本标记：`diagnostic=mqtt-plain-20260928`。
- 配套底包身份：`722b56135a0f6fb6874a`。
- [完整验证记录](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-plaintext-20260928/verification.json:1)。旧包已在项目内备份。

## 当前参数与定位

| 工程相对路径、文件名、准确行号 | 函数或配置段 | 当前行为 |
| --- | --- | --- |
| [project/private/alarm_cloud.h:15](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:15) | 产品配置宏 | 心跳 12 小时，厂家标识 0x4872（报文字节 48 72；用户于 2026-09-28 纠正原十进制解释） |
| [project/private/alarm_cloud.h:21](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:21) | CLOUD_ENABLED / PROTOCOL_VERIFIED | 云连接开启；正式平台协议验证保持 0 |
| [project/private/alarm_cloud.h:37](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:37) | ENCRYPTION_ENABLED / PLAINTEXT_TRIAL | 加密为 0、明文联调为 1；不依赖 factoryCode 或随机 IV |
| [project/private/alarm_cloud.h:39](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:39) | PACKET_LOG_ENABLED | 实际报文打印为 1，可改 0 关闭 |
| [project/private/alarm_cloud.h:40](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:40) | UP_TOPIC_SUFFIX / DOWN_TOPIC_SUFFIX | `/sys/fire/data/up` 与 `/sys/fire/data/down` |
| [project/private/alarm_cloud.h:43](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:43) | UNKNOWN_TELEMETRY_TRIAL | 联调允许未知字段填 0xFF，采样有效位保持真实；正式确认宏仍为 0 |
| [project/src/alarm_button/alarm_runtime.c:228](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_runtime.c:228) | alarm_publish_frame | 直接 HEX 编码并提交 MQTT，打印同一份发送内容 |
| [project/src/alarm_button/alarm_runtime.c:567](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_runtime.c:567) | alarm_on_cloud_message | HEX 解码并校验长度、版本、厂家、CRC 和回执序号 |
| [project/src/alarm_button/alarm_runtime.c:718](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_runtime.c:718) | alarm_make_platform_topics | 读取本机 IMEI 拼主题，未硬编码示例设备 |
| [project/src/ml307y/system_port.c:254](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/system_port.c:254) | ml307y_packet_log | 复用 UART0，单段最多 160 字符，超过时按 offset 分行完整打印 |

当前上行：`iot/devices/{本机IMEI}/sys/fire/data/up`。下行按配对结构先配置为 `iot/devices/{本机IMEI}/sys/fire/data/down`，仍需真实回执验证。密码及 AES 密钥均未改动且不会打印。

如日后恢复 AES，须同时调整加密开关及两个 Topic 后缀，补全 factoryCode，并完成平台协议验证，不能只切一个开关。

## 发送与日志

上行 MQTT payload 是完整协议帧的 ASCII 十六进制字符串，形如 `57544B361308…454E44`，包含实际帧头、协议版本、厂家、序号、命令、长度、数据体、CRC 和帧尾；不带 AES JSON 包装，也不把日志前缀或 CRLF 放入 MQTT payload。

每次发送日志包含：

```text
[project][mqtt-tx] type=registration seq=… bytes=142 queue_result=0
[project][mqtt-tx] seq=… topic=iot/devices/实际IMEI/sys/fire/data/up
[project][mqtt-tx] seq=… offset=0 data=完整十六进制注册包
```

这是格式说明，不是实板日志。心跳类别为 `heartbeat`，报警类别为 `alarm`；实时心跳与报警各为 74 个 HEX 字符。`queue_result=0` 只表示传输层接受本次发送，后续须分别看到 `registration-confirmed`、`heartbeat-confirmed`、`alarm-confirmed-and-saved` 才能确认对应业务成功。发送失败也记录报文及失败返回值，方便核对。

注册成功后发送启动心跳；按键报警仍先保存，再等待匹配 FF/00 业务回执后删除。断线重连重新注册。原按键、声光节奏和 12 小时心跳规则保留。

## 已完成验证

- 修改前新增明文测试，确认原发送流程因缺 factoryCode/协议验证阻止联调；修复后通过。
- 12 组 RV64 模拟 C 测试、明文运行流程变体及蜂鸣器宏变体通过；含缺电量占位、完整明文帧、4872 厂家字节、无需随机源、重连注册、发送失败、错误 CRC/厂家/版本/主题、旧回执及删除失败保护。
- 测试逐字节比较日志回调内容与实际 MQTT publish 内容；区分注册、心跳、报警，验证打印开关关闭后无报文输出。
- 30 项 Python 测试通过；私有宏编译断言通过；原凭据、厂家、心跳等宏保持一致。
- 底包和应用构建成功，编译日志均无 warning/error；模板现有产物与报警产物隔离、包哈希和底包配套检查通过。模板本轮未重建。
- 测试 SDK 桩仍有已有的 __ATOMIC_* 宏重定义警告，与固件编译日志分开报告。
- 本轮未烧录、未连接真实平台，也未收到实板业务回执。

## 上板检查与可能问题

1. 烧入新包并上电，在 COM12、115200/8N1 核对版本标记；检查 Topic 中是本板实际 IMEI。
2. 核对完整注册包，随后等待 `registration-confirmed`。若只有发送日志或 PUBACK，平台业务注册尚未确认；检查平台明文接入、下行 Topic、CRC 和厂家解释。
3. 按键触发并确认声光正常，检查 `type=alarm` 和 `alarm-confirmed-and-saved`；服务器未确认时记录必须继续保留。
4. 断网重连后验证重注册和重试；连续重按及旧回执不能结束新提示。若 UART 输出中断，应检查 `mqtt-packet-log` 错误和串口实际收发。
5. 0xFF 暂作用户许可的未知值，平台可能拒绝；收到拒绝时再按平台实际约定改宏。下行 `/data/down` 是当前配对配置，尚无实机确认。
6. 无 UTC 的旧历史记录仍保留待约定；16 位序号耗尽仍明确停止，不猜测回卷；队列最多 96 条，未确认记录占用容量。掉电恢复、长时间运行与实际 12 小时心跳仍需上板检验。
