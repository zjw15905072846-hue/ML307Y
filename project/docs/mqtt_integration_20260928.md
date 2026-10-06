# 一键报警 MQTT 对接记录与待补参数

日期：2026-09-28。仅处理用户指定的 D 盘工程；用户已批准按每批最多三份文件备份、补测试、修复、回归及重建。用户随后提供厂商标识 4872，已按十进制写入宏，对应 0x1308；factoryCode 和未知电量编码仍待提供，不使用文档示例冒充正式值。

## 协议核对

本次阅读用户提供的 219 页铠湾 MQTT V3.6 协议，原文件 SHA-256 为 `b1b005ee844f0a778761799d537e162a2dacfb27b59577c85c73255abb6ebe74`。提取正文并渲染核对了加密、历史事件和回执表格。

| 协议内容 | 本轮采用的规则与边界 | 依据 |
|---|---|---|
| 上下行主题 | 使用本机真实 IMEI 的 aesdata/up 与 aesdata/down；使用用户给定 Broker，不改成文档测试地址 | [第 9 页](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-protocol-20260928/protocol-pages.txt:327) |
| AES | AES128-CBC、随机 16 字节 IV、不足块补 0x30、JSON 包含 factoryCode/iv/encryptData | [第 10～11 页](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-protocol-20260928/protocol-pages.txt:349) |
| 注册 | 每次上电及重连重新注册；命令 01，手报类型 04，数据体 55 字节；身份由设备/SIM 读取 | [第 23～24 页](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-protocol-20260928/protocol-pages.txt:745) |
| 心跳与报警 | 命令 02、设备类型 04、数据体 21 字节；心跳事件 01、紧急报警事件 0C；周期按用户要求保持 12 小时 | [第 32～33 页](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-protocol-20260928/protocol-pages.txt:1120) |
| 历史事件 | 命令 0C，含条数、条目编号及真实发生 UTC；现有单条格式为 27 字节；旧记录没有真实时间时不补造 | [第 103～104 页](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-protocol-20260928/protocol-pages.txt:4151) |
| 业务回执 | 命令 FF、同一消息序号、回复 00 才成功；数据长度和示例均为一字节回复，表内“2 字节”存在笔误 | [第 208～209 页](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-protocol-20260928/protocol-pages.txt:7909) |
| 拒绝码 | 01 为 CRC 失败、02 为解析异常、03 为厂商校验失败 | [第 219 页](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-protocol-20260928/protocol-pages.txt:8341) |

表格指定版本 0x36，部分示例仍用旧版 0x35 且厂商/CRC 为 XX，不能逐字照抄。文档加密样例没有配套明文和密钥；用当前用户 AES 密钥检查其上下行示例未恢复出 WTK 帧，不能据此判定用户密钥错误，也不能把该示例作为互通通过证据。当前采用 HEX 字符串明文和大端 CRC；正式平台回执或完整参考向量仍需验证这两项。

## 本次修改

- 补充固定参考向量，覆盖手报注册、心跳、报警、历史和一字节成功回执。参考 CRC/AES 由独立 Python 库计算，采用虚构厂商和测试密钥；与生产 C 编码比较，不把自身加解密往返当成平台互通证明。
- 修正 factoryCode 恰好 32 字符的错误限制。文档只给字符串和一个 32 字符示例；现在允许非空且不超过本地 32 字符容量的值，仍拒绝空串和缺终止符。
- 增加缺参数、缺电量、随机源、加密、传输、注册/心跳超时与平台拒绝的具体阶段诊断。正常阶段也输出队列提交、MQTT PUBACK、注册/心跳业务确认及报警持久删除完成；不打印账户、密钥或完整报文。
- 修复待补报旧记录阻塞新报警：检查每条记录是否满足发送条件，旧记录保留，发送最早满足条件的记录；按已确认事件 ID 原子删除，其他记录顺序不变。没有可发送记录时不反复分配持久序号。没有修改现有 Flash 镜像格式。
- 注册/心跳超时和重试复用已有产品宏；启动标记改为 `mqtt-20260928`。账户、独立 AES 密钥、12 小时心跳、声光参数及此前存储截断同步修复保持原值。

## 参数入口

真实参数集中在项目内已忽略的[私有配置](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:1)中。修改后必须把该文件作为 `--provision` 参数重建。

| 宏 | 当前状态与操作 | 位置 |
|---|---|---|
| ALARM_BUTTON_MANUFACTURER_ID | 已填十进制 4872，即 0x1308；帧中大端字节为 13 08 | [私有配置第 16 行](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:16) |
| ALARM_BUTTON_FACTORY_CODE | 待平台提供字符串；与 AES 密钥不同；当前本地容量最多 32 字符 | [私有配置第 17 行](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:17) |
| ALARM_BUTTON_UNKNOWN_TELEMETRY_VERIFIED / BYTE | 待确认手报未知电压、电量和信号的占位编码；候选 FF 未启用，不能虚报为 100% | [私有配置第 24 行](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:24) |
| ALARM_BUTTON_HEARTBEAT_HOURS | 已设 12，与 MQTT keepalive 分开 | [私有配置第 15 行](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:15) |
| ALARM_BUTTON_CLOUD_DIAGNOSTICS | 当前 1；设 0 关闭正常阶段日志，故障仍报告 | [私有配置第 20 行](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:20) |
| ALARM_BUTTON_CLOUD_ENABLED / PROTOCOL_VERIFIED | 当前都为 0；参数和协议设置确认后需同时启用再重建，不会自动用示例值连接 | [私有配置第 21 行](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:21) |
| ALARM_BUTTON_AES_HEX_PLAINTEXT / CRC_LITTLE_ENDIAN | 当前 HEX 明文、大端 CRC；用平台已接受的完整报文或实际联调核验 | [私有配置第 28 行](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:28) |
| ALARM_BUTTON_REPLAY_MODE / UTC_VERIFIED | 当前 0/0；旧事件保留；有已验证 UTC 才能使用历史模式 1，模式 2 需平台允许实时重投 | [私有配置第 32 行](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:32) |

## 执行与验收流程

```text
设备与存储初始化
├─ 配置缺失：打印具体 cloud-config-*，按键声光与持久保存继续工作
└─ 配置齐备且读取到真实 IMEI / IMSI / ICCID
   └─ 请求 MQTT 连接，等待订阅完成
      ├─ 断线：重连后重新注册
      └─ 注册入队
         ├─ MQTT PUBACK：仅传输完成，继续等待业务回执
         ├─ 平台拒绝 / 回执超时：保留状态，退避后重试
         └─ 同序号 FF/00：注册成功，发送启动心跳
            ├─ 心跳 FF/00：记录完成时刻，12 小时后到期
            └─ 处理持久报警
               ├─ 旧记录缺时间或补报策略：保留，继续检查后面的记录
               ├─ 缺遥测且未知值未获确认：保留并报告字段
               └─ 可发送：提交报警并等待对应 FF/00
                  ├─ 过期回执 / 失败回复：不删除
                  ├─ 删除落盘失败：保留事件，不报告完成
                  └─ 删除落盘成功：通知该事件完成，其他事件继续保留
```

## 定位索引

| 工程相对路径、文件名与行号 | 函数 | 用途 |
|---|---|---|
| [project/src/alarm_button/alarm_runtime.c:403](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_runtime.c:403) | alarm_load_cloud_config | 校验配置并逐项诊断 |
| [project/src/alarm_button/alarm_runtime.c:331](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_runtime.c:331) | alarm_send_registration_or_heartbeat | 注册、心跳、超时及重试 |
| [project/src/alarm_button/alarm_runtime.c:538](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_runtime.c:538) | alarm_on_cloud_message | 关联业务回执 |
| [project/src/alarm_button/alarm_runtime.c:271](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_runtime.c:271) | alarm_event_is_ready | 筛选满足发送条件的事件 |
| [project/src/alarm_button/alarm_core.c:295](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_core.c:295) | alarm_reporter_poll | 发送、重试及跳过待补报事件 |
| [project/src/alarm_button/alarm_core.c:184](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_core.c:184) | alarm_store_remove | 按业务确认 ID 持久删除 |
| [project/src/kaiwan/kaiwan_protocol.c:483](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/kaiwan/kaiwan_protocol.c:483) | kaiwan_protocol_wrap_json | 有界 factoryCode 与 AES JSON 编码 |
| [project/tests/alarm/test_runtime.c:426](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/tests/alarm/test_runtime.c:426) | test_pending_history_does_not_block_new_alarm | 旧记录保留、新事件确认及写失败回归 |

## 剩余问题与测试

1. 平台参数：仍缺 factoryCode、未知遥测编码约定。ManufactureId 已填十进制 4872，MQTT 账号密码和 AES 密钥已有，不需要重复提供。
2. 平台互通：参数补齐后验证真实 MQTT 登录、订阅、注册、启动心跳、按键报警和匹配 FF/00。文档样例不足以独立证明实际 AES/CRC 字节约定；不得把此轮模拟回执当作真实服务器回复。
3. 历史记录：核验设备授时后启用真实 UTC 历史补报；此前没有 UTC 的旧记录不能恢复发生时间。它们仍占用 96 条队列容量，需平台明确允许的处理方式，不能擅自删除。
4. 序号长期边界：文档允许 FFFF 后重新计数；现有防旧回执串单实现仍在序号耗尽时显式停发。自动回卷和跨轮迟到回执隔离尚未完成，投产前须单独处理及验证。
5. 实板可靠性：已有六次连续保存成功日志，仍需正常断电恢复、写入中断电、反复断网和连续 12 小时心跳实测。本轮没有烧录或连接真实平台。

## 软件验证与交付

- 三项问题均先复现再修复：[缺参数诊断失败](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-protocol-20260928/runtime-red.log:1)、[factoryCode 长度限制失败](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-protocol-20260928/factory-length-red.log:1)、[旧记录阻塞新报警失败](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-protocol-20260928/history-block-red.log:1)。对应定向测试及最终全套回归均通过。
- 12 组 RV64 模拟器 C 回归和蜂鸣器宏覆盖测试通过，见[C 回归](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-protocol-20260928/c-tests.log:1)。保留原有测试桩原子宏重定义警告，未把它们隐藏。
- 使用虚构且完整的配置另行运行注册、心跳、断线重连、平台拒绝、超时、业务确认和旧记录保留流程，通过[配置齐备的模拟测试](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-protocol-20260928/runtime-configured-final.log:1)，没有连接真实 Broker。
- 30 项 Python 检查通过，见[Python 回归](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-protocol-20260928/python-tests.log:1)。当前私有配置的 MQTT 参数、独立 AES 密钥和心跳宏与基线一致。
- 报警底包与应用构建、打包成功，编译日志无 warning/error 诊断；[配套及隔离检查](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-protocol-20260928/artifacts.log:1)通过。模板产物沿用已有包，本轮未重新构建模板。

厂商标识补齐前的包已备份为[alarm-before-manufacturer.mimgx](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-manufacturer-20260928/alarm-before-manufacturer.mimgx)。生成时间 2026-09-28 10:44:20，大小 10455197 字节，SHA-256：`372939d904367ad7abca88b06d1fd2cfc6840d50ee607e32a259558971e184e9`。

启动标记为 `diagnostic=mqtt-20260928`。当前 factoryCode 与未知值约定未填，两个云开关仍关闭；厂商标识补齐后的包不应再报告 cloud-config-manufacturer，其他未满足项继续明确报告。生成包和模拟测试不等于设备已烧入，也不等于平台已注册成功。

## 厂商标识 4872 的配套包

本次只修改厂商标识宏和说明。编译断言确认十进制 4872 等于 0x1308，其他私有宏逐项保持不变。报警底包与应用重建成功，编译日志无 warning/error 诊断，[配套检查](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-manufacturer-20260928/artifacts.log:1)通过；本次未重复执行前一版的整套运行时回归，也未烧录或连接平台。参数补齐后仍须用真实 FF/00 回执验证厂商识别。

当前[烧录包](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/products/alarm_button/alarm_board_v1/ml307y/image/ML307Y_alarm_button.mimgx)生成于 2026-09-28 10:50:56，大小 10466269 字节，SHA-256：`113b49f471f441374d19e0174a6ba3c93a5b729d65d86fe62b937f7208131e42`。厂商参数与验证状态见[本次核验记录](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-manufacturer-20260928/verification.json:1)。
