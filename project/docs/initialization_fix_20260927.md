# 2026-09-27 初始化修复与上板验收

本轮仅处理当前 D 盘工程。按用户最新要求，物理 26 脚按键、物理 96 脚 LED 均启用；LED 高电平亮、低电平灭。报警时序、持久队列、平台业务确认规则保持原样。

## 已确认的原因与修复

- 按键原先调用物理 26 脚 FUNCTION2，但当前底包掩码为 0x02，只支持 FUNCTION1，调用必然返回 -1；CM GPIO 表只有 18 项，CM GPIO26 不能代表物理 26 脚。
- 底包 cm_dtr_init 的实际输入为 AGPIO_PIN0=100。新底包扩展直接调用同版 HAL 配置输入上拉并读取，避免创建 SDK DTR 休眠任务；保留工作锁、5ms 前台轮询和 30ms 消抖。
- 蜂鸣器物理 74 脚对应 CM GPIO16，GPIO 路径要求 FUNCTION3，旧 FUNCTION2 导致初始化失败。已修正，未擅自改变原有 GPIO 蜂鸣器模式。
- 原理图中 96 脚经 R6 驱动 Q2=S8050，确认为高亮低灭。当前底包 CM_ADC_1 对应 HAL_ADC4，其管脚路径为 GPIO_PIN_B=41；新增 HAL 数字输出，初始关闭，并核对输出寄存器回读。该脚不作 ADC 电池采样。
- 启动任务输出固件编译时间、产品名和 initialization complete/failed，删除无意义的 Hello World/11111 循环打印；任务继续带延时常驻。
- 旧测试桩无条件接受错误编号，未能发现实板初始化失败。新测试按真实映射约束拒绝错误参数，按键和蜂鸣器失败已在修复前分别复现。

## 定位索引

以下链接标签为工程相对路径；链接目标为当前工程绝对路径及准确行号。

| 工程相对路径 / 文件名 | 所在函数或配置段 | 内容 |
|---|---|---|
| [project/src/ml307y/alarm_key.c:72](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/alarm_key.c:72) | ml307y_alarm_key_init | 按键初始化与首次读取 |
| [project/src/ml307y/alarm_led.c:54](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/alarm_led.c:54) | ml307y_alarm_led_init | LED 高亮低灭接口 |
| [project/src/ml307y/alarm_buzzer.c:67](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/alarm_buzzer.c:67) | ml307y_alarm_buzzer_init | GPIO16 的 FUNCTION3 |
| [project/src/ml307y/base_gpio.c:49](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/base_gpio.c:49) | project_button_input_init | AGPIO0 输入与 HAL ABI |
| [project/src/ml307y/base_gpio.c:102](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/base_gpio.c:102) | project_led_output_init | GPIO_PIN_B 输出 |
| [project/src/ml307y/product_start.c:140](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/product_start.c:140) | product_boot_task | 启动结果、常驻循环与本轮六次存活诊断 |
| [project/src/ml307y/file_port.c:32](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/file_port.c:32) | ml307y_file_trace | 文件阶段、SDK 返回码与字节数 |
| [project/tests/alarm/test_file_port.c:173](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/tests/alarm/test_file_port.c:173) | main | 注入不同文件故障并验证阶段诊断 |
| [project/build/manifests/alarm_button.json:24](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/build/manifests/alarm_button.json:24) | resources / led_sdk_pin | 声明物理 96 脚、HAL LED 标识 41 |

## 验证与产物

- 12 组 RV64 模拟器 C 回归全部通过，包括 HAL ABI/参数、初始化错误传播、LED 输出失败重试、持久化、按键重复触发及业务确认隔离。这不是主机原生测试或实板测试。
- 25 项 Python 检查全部通过，包括构建依赖、产品隔离和引脚资源配置拒绝。
- 报警与模板固件的底包、应用交叉编译和打包成功；本轮构建日志无编译 warning/error。
- 两产品的 ELF 入口、生成配置、缓存隔离和固件哈希校验通过；4 个新增底包导出地址与真实 ELF 符号逐项一致。
- [C 回归记录](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/init-diagnosis-20260927/c-tests.log:1)、[Python 回归记录](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/init-diagnosis-20260927/python-tests.log:1)、[配套检查记录](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/init-diagnosis-20260927/artifacts.log:1)。C 测试中的既有 cm_os 模拟头仍有原子宏重定义警告，不影响测试结果。
- [烧录包](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/products/alarm_button/alarm_board_v1/ml307y/image/ML307Y_alarm_button.mimgx)；[发布回执](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/products/alarm_button/alarm_board_v1/ml307y/release.json:1)。必须使用本包中的配套底包和应用，不能混用旧底包。
- 底包身份：`4ae5f7ed14146ed0f1bc`。
- 第一版初始化修复包 SHA-256：`b794100b1876c4dbc2f51bb7b99295cc291a6515064126ddc3af8b6b51611ff4`。随后实板发现保存失败，当前产物已更新为下述诊断包，不能把第一版测试结果等同于功能验收。

## 实板边界与验收

用户已更正本板 UART0 为 COM12，并授权采集日志；使用 115200/8N1。此前 COM10 为误认端口，其打开失败不能视为本板故障。第一次采集无字节并断开；重新采集后已收到编译时间为 Sep 27 2026 00:55:15 的报警固件完整启动输出：按键首次 pressed=0，按键、LED、蜂鸣器均 ready，两个业务任务创建，随后 initialization complete 和 alarm-worker error=-10。见[本次启动原始日志](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/init-diagnosis-20260927/uart0-com12-reconnect.log:1)。

后续采集收到 alarm-ui error=-2、alarm-worker error=-2，以及新的 alarm-ui error=-2。见[按键期间原始日志](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/init-diagnosis-20260927/uart0-com12-button.log:1)。这些输出证明两个任务在相应时刻仍有执行；-10 为云参数未配置，-2 为报警记录保存失败。尚未取得声光实物确认与持久保存成功证据，不能宣称实板跑通。

用户已批准继续定位保存故障。先用注入零写、同步、关闭、打开失败的测试复现“错误只有汇总码，无法定位接口”的问题，再补文件适配分阶段日志；测试由失败转为通过。实际 Flash 保存失败的底层原因仍须由新包日志确认，不能把测试桩通过当作已修复 Flash。

- 01:24 诊断包标识：`diagnostic=storage-20260927`，生成于 2026-09-27 01:24:10；后续平台参数版本见文末。
- 01:24 诊断包 SHA-256：`c865d2cc88be0a3ed88d7fed0db9cc438354690da637efb736fa22c8a5407171`。
- 文件诊断报告 probe、filesize、open-read、read、close-read、open-write、write、sync、close-write 的返回码，以及读写完成字节数；保留原有失败传播、双槽回读和数据保护规则。
- 启动后每五秒输出一次 scheduler alive，最多六次；该输出只能证明启动任务和调度当时仍运行，不能代替按键和保存验收。
- [复现测试失败记录](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/init-diagnosis-20260927/storage-file-red.log:1)、[修复后文件适配测试](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/init-diagnosis-20260927/storage-file-green.log:1)、[25 项 Python 检查](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/init-diagnosis-20260927/storage-python-tests.log:1)已完成；新报警诊断包已交叉编译并通过配套检查。
- 新诊断改动的 [12 组 RV64 模拟器回归](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/init-diagnosis-20260927/storage-c-tests.log:1)全部通过；报警和模板均已重新构建，[两产品配套检查](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/init-diagnosis-20260927/storage-artifacts.log:1)通过。

助手未执行烧录。01:25:21 至 01:31:21 的 COM12 采集窗口内没有收到新字节，已结束并释放串口；[该次采集记录](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/init-diagnosis-20260927/uart0-com12-storage.log:1)仅有打开标记。仍需用户烧入新诊断包后取得同次上电和按键日志；新包不是“保存已修复”的交付结论。

1. 烧录上述完整包后重新上电，UART0 使用 115200/8N1。应依次见到 UART0 ready、按键首次电平、alarm key ready、alarm LED ready、alarm buzzer ready、两个报警任务创建以及 initialization complete；核对本次编译时间，避免旧包或混合日志。
2. 松开按键上电应报告 pressed=0；按住上电应为 pressed=1。普通按下一次，消抖后新增一条报警并点亮 LED；释放后重按须新增事件，持续按住不能反复新增。
3. LED 按原有规则验证：0～1 秒常亮，1～4 秒按 250ms 开/关闪烁，前 4 秒不被业务确认打断。后续状态按原有成功回执与 34 秒结束规则执行。
4. 连续运行、重复断电启动，观察是否复位、日志中断或初始化失败；若失败，保留同次上电全部日志和所烧包哈希，不能只取最后的汇总错误。

AT 硬件手册第 20/24/47 页仍把 26 脚标为 RSV，第 37 页描述 96 脚为 0～1.4V ADC 输入；这些页不保证上述 OpenCPU 数字功能。当前修改依据同版底包实现和用户指定的实板接线，必须用实板电平、LED 和报警结果验证。蜂鸣器有源/无源及载波仍未由实物确认，本轮未推定其鸣叫已通过。

底包原始证据保留在[按键及 GPIO 表核验](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/init-diagnosis-20260927/sdk-pin-evidence.txt:1)与[ADC1 到 GPIO_PIN_B 路径核验](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/init-diagnosis-20260927/sdk-led-evidence.txt:1)。

## 整板全链路目标核验（01:36）

用户最新目标包括整板运行、实体按键报警和真实平台上报，不能缩减为编译通过、任务创建、模拟回执或本地测试 Broker 成功。当前目标未完成。

| 验收项 | 当前证据 | 结论与所缺证据 |
|---|---|---|
| 配套底包和应用启动 | 00:55:15 固件启动日志包含三个器件 ready、两个业务任务和初始化完成 | 初始化返回成功已有实板证据；新诊断包是否烧入仍未确认 |
| 物理 26 脚按键触发 | 首次输入未按下；后续前后台产生报警保存失败输出 | 保存请求链路已有运行迹象；缺少与实体按下/释放对应的连续日志和声光确认 |
| 96 脚高亮低灭及蜂鸣器时序 | 初始化成功、软件时序模拟测试通过 | 实际发光、鸣叫、重复按键和前四秒提示未验收 |
| 报警事件可靠落盘 | 实板报告 -2；分阶段诊断及错误注入测试已补齐 | 明确失败，尚缺具体 SDK 失败阶段及修复后的读回/重新上电恢复证据 |
| 网络与真实平台上报 | 当前云开关关闭，Broker、账户、密钥、厂商信息为空 | 无法建立真实连接；缺正式平台配置与协议样例 |
| 业务回执确认并删除对应记录 | 12 组 RV64 模拟器测试涵盖注册、心跳、加密 FF/00 业务确认、旧事件隔离 | 软件逻辑有测试证据；缺目标平台真实回执与板端持久状态核验 |
| 离线重试与断电恢复 | 双快照故障注入、回读、删除失败保留测试通过 | 尚需实板断网、掉电和恢复后上报验证；历史补报约定仍未提供 |

本轮重新采集 COM12 的窗口为 01:32:52 至 01:35:52，未收到新字节，进程已正常结束并释放串口。见[继续运行采集记录](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/init-diagnosis-20260927/uart0-com12-goal-resume.log:1)。不能将这个静默窗口解释为断定死机或断定正常。

已在当前工程中查找产品私有配置和烧录入口，未找到正式凭据或可执行下载工具；运行进程中也没有匹配的下载程序。旧提交的报警配置同样为空，未把测试配置当作正式账户。构建目录有历史遗留的 G 盘测试头文件引用，但当前编译没有启用 ALARM_BUTTON_PROVISION_HEADER，未访问该外部路径。

云配置阻断来源为 [project/inc/alarm_button/provisioning.h:9](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/inc/alarm_button/provisioning.h:9)，检查入口为 [project/src/alarm_button/alarm_runtime.c:264](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_runtime.c:264) 的 alarm_load_cloud_config。需要提供工程内正式配置及平台样例，并烧入新诊断包取得失败阶段；在这些外部条件补齐前不能完成真实上报或声光实测。已请求用户补充，未虚构账户、密钥、历史补报或平台成功回执。

## 01:39 设备状态与受阻条件

再次尝试打开 COM12 时，系统返回端口不存在。随后实时查询 SerialPort.GetPortNames 和当前 Ports 设备，只返回 COM10 / JLVirtualJtagSerial Device；该端口此前已确认为非本板 UART0，未擅自改用。当前不只是没有新日志，而是用户确认的板子串口已不可用。见[实时核验记录](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/init-diagnosis-20260927/goal-blockers-0139.txt:1)。

正式私有配置目录仍不存在；当前允许访问的项目内也没有找到下载工具。烧录/实板连接和正式云端配置这两个必要条件已连续三个目标回合无法满足，现阶段没有可独立推进到实板闭环的操作。目标应标为受阻而非完成，不通过继续改动未证实的 GPIO、删除存储数据或虚构平台回执替代验收。

恢复所需：重新连接本板并确认 UART0 端口，烧入当前诊断包或提供项目内可用下载工具；提供项目内正式平台配置与协议样例。恢复后先采集新包标识及首个保存失败阶段，再继续真实报警落盘、平台上报、匹配业务回执、重复按键和断电恢复验证。

## 09:20 平台参数宏与十二小时心跳

用户已补充服务器域名、1883 端口、MQTT 账号和 16 字符 AES 密钥，并明确 MQTT 登录密码另行提供；厂商信息以后补充。已将这些参数集中为[独立平台宏](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:5)，该文件受项目 Git 忽略规则保护。本记录不抄录账号或密钥。

- ALARM_BUTTON_BROKER_HOST、BROKER_PORT、MQTT_USERNAME、AES_KEY_TEXT 已填。
- ALARM_BUTTON_AES_KEY_BYTES 直接采用 AES_KEY_TEXT 的 16 个 ASCII 字节，编译时校验长度；MQTT_PASSWORD 保留为空，禁止拿 AES 密钥代替登录密码。
- ALARM_BUTTON_HEARTBEAT_HOURS 为 12；[毫秒周期宏](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/inc/alarm_button/product_config.h:6)统一用于前后台业务心跳调度，未修改 MQTT keepalive 秒数。原有上电注册与首次心跳流程保持原样。
- MANUFACTURER_ID、FACTORY_CODE 仍待填；CLOUD_ENABLED 与 PROTOCOL_VERIFIED 保持 0，待密码、厂商信息与平台样例确认后启用。仅提供域名与 AES 密钥还不能证明能登录或被平台接受。

新增回归先复现旧 22 小时周期不符合要求，再验证成功回执后的 12 小时到期边界、到期前不触发、同一时刻不重复触发及 32 位毫秒回卷。12 组 RV64 模拟器 C 回归、25 项 Python 检查全部通过，私有配置编译验证确认 AES 与 MQTT 密码分离；0 或 597 小时的无效周期均被拒绝。证据：[周期修改前失败](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/platform-config-20260927/heartbeat-red.log:1)、[C 回归](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/platform-config-20260927/c-tests.log:1)、[Python 检查](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/platform-config-20260927/python-tests.log:1)、[配置编译检查](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/platform-config-20260927/config-checks.json:1)。

已用私有配置构建报警包，生成时间 2026-09-27 09:20:48，SHA-256 为 `d5957a4b1734290b7831f92302df9b72cf3881b2d83627ad86bf1c89c483eecc`。该次[报警包](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/products/alarm_button/alarm_board_v1/ml307y/image/ML307Y_alarm_button.mimgx)随后已被下节 MQTT 凭据更新版本覆盖；[配套核验](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/platform-config-20260927/artifacts.log:1)通过。以后修改宏后，须按[平台配置构建说明](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/docs/new_product_guide.md:94)显式带 --provision 重新构建。

剩余实板验证包括 MQTT 登录、注册/报警业务回执、十二小时心跳、真实 Flash 保存及断电恢复。此次配置修改没有解除原保存失败，也没有执行烧录或声称上报成功。

## 09:32 MQTT 登录凭据补齐

用户已单独提供最新 MQTT 登录账号和密码，现已写入[平台宏配置](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:7)。本节取代 09:20 记录中的“MQTT 密码待提供”状态。此前确认的 AES 密钥保持不变；MQTT_PASSWORD 与 AES_KEY_TEXT 各自定义，即使当前值相同也不互相引用。本记录不抄录账户或密钥。

- 服务器和 1883 端口保持用户指定值，业务心跳仍每 12 小时。
- 厂商标识、32 字节厂商码和平台协议样例仍待补齐，两个云配置开关保持 0。此次未连接平台或执行烧录。
- 已核对私有宏内容，编译验证 MQTT 凭据、AES 长度和 43200000 毫秒心跳配置；使用私有配置重新构建底包和应用成功，编译日志没有 warning/error 诊断，配套产物检查通过。
- 本次仅修改配置和说明，没有改动业务逻辑，未重复运行上一节的 12 组 RV64 模拟器回归与 25 项 Python 检查，也未把先前结果当作本次重新执行。

最新烧录包生成时间 2026-09-27 09:31:12，SHA-256 为 `5a0410310b2831ba0db7200765f162bb68847ef1022e190101b2c73ca4e0431c`。检查记录：[配置编译核验](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-config-20260927/config-checks.json:1)、[构建结果](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-config-20260927/build-alarm.log:1)、[配套核验](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-config-20260927/artifacts.log:1)。

后续风险与测试：厂商/协议参数未确认时仍无法验收真实上报；参数补齐后须用本板验证 MQTT 登录、注册及匹配事件的业务回执。实板保存失败 -2 尚未定位到具体 SDK 阶段，须采集新诊断包日志，验证保存读回及断电恢复。十二小时心跳须补实板持续运行验证，初始化返回成功不能替代按键、LED、蜂鸣器和完整报警链路验收。

## 2026-09-28 新日志与声光确认

用户提供的 09:49 实板日志已明确保存失败阶段：slot 1 写入并读回 1980 字节成功，随后覆盖 slot 0 的 cm_fs_write 返回 -28。配套底包证据确认该值为 LittleFS 无可分配空间；此前“尚不知具体 SDK 失败阶段”的状态已更新。

报错后 scheduler alive 的 tick 仍持续增加。用户同时明确确认按键触发后“灯和声音都正常”，因此此前的声光未验收状态更新为该版本已有用户实测确认。

现已修改非活动槽的写入顺序，先同步截断再写新快照，并补充容量诊断和失败保护测试；新包尚待烧录后验证连续保存及断电恢复。详见[存储负 28 修复记录](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/docs/storage_nospace_20260928.md:1)。云配置 -10 与真实平台闭环仍未解决，不与本次存储错误混为一谈。

## 2026-09-28 10:10～10:11 连续保存通过

用户新日志包含 space-before / space-after-truncate，确认运行到了本次新增存储路径。首次初始化保存及后续五次交替保存均完成 1980/1980 的写入和读回，无 -28 或 -2；后四次旧槽复用均从 free=0 释放到 4096 字节，板上总容量为 32768 字节。原始证据：[本次 UART0 日志](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/storage-nospace-20260928/user-uart0-101046.log:1)。本节更新上一节“连续保存尚待实板验证”的状态。

两槽启动探测均为缺失，因此尚不能证明断电后的旧报警恢复；下一步在不重新烧录、不清空文件系统的情况下正常断电重启，并核对已有槽读回及再次按键保存。写入途中掉电、长期运行、真实平台上报和业务回执仍待验收。-10 仍是厂商/协议配置条件未满足，MQTT 登录凭据已补齐。本次只更新记录，没有修改或重建固件。

## 2026-09-28 MQTT V3.6 核对与修复

用户提供完整协议并批准修改，同时确认 ManufactureId、factoryCode 和未知电量编码均未提供，要求保留宏。已核对注册、手报、AES、历史及 FF/00 回执，修复 factoryCode 固定长度限制和旧待补报记录阻塞新报警，并细分缺参数、超时、拒绝和成功阶段诊断。心跳仍为 12 小时，凭据保持原值；启动标记更新为 mqtt-20260928。

本轮具体修改、代码定位、可编辑宏及尚缺参数以[MQTT 对接记录](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/docs/mqtt_integration_20260928.md:1)为准。旧记录继续保留；新配置不会用文档示例或虚构电量绕过检查，真实注册和报警上报仍需正式厂商参数及平台联调。

## 2026-09-28 厂商标识补齐

用户提供厂商标识 4872，现已按十进制写入[厂商标识宏](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:16)，对应十六进制 0x1308，协议大端字节为 13 08。此条取代之前的“厂商标识待提供”状态。factoryCode、未知遥测约定和真实平台联调仍待完成，两个云开关保持关闭。
