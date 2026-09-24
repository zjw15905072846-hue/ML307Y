# ML307Y 产品开发区

本工程目前包含一键报警产品和构建隔离验证用的虚拟模板。门禁仍保留在旧 D 盘工程；新工程没有迁入门禁任务。厂商启动、链接和底层目录保留，自研代码集中在本目录。

```text
project/
├─ inc/                        全项目共用头文件目录
│  ├─ key.h、indicator.h 等     公共状态与功能接口直接放此处
│  ├─ alarm_button/            报警功能的多个头文件
│  ├─ kaiwan/                  铠湾协议的多个头文件
│  ├─ mqtt/                    MQTT 传输的多个头文件
│  └─ ml307y/                  ML307Y 器件接口与平台配置
├─ src/                        全项目共用源码目录
│  ├─ indicator.c 等           不依赖器件的单文件功能
│  ├─ alarm_button/            报警业务、队列和手报数据体
│  ├─ kaiwan/                  铠湾协议编解码与会话
│  ├─ mqtt/                    传输配置与接收组包
│  └─ ml307y/                  按键、灯、蜂鸣器、电池及其他 CM 适配
├─ build/                      产品注册表、清单和配套校验
├─ tests/                      主机与构建隔离回归
├─ tools/                      新产品创建、构建和验收入口
├─ docs/                       扩展说明、硬件边界与验收记录
└─ SConscript
```

只有同一功能有多个文件时才创建对应子目录，不为单文件功能创建空目录。ML307Y 按键、LED、蜂鸣器、电池各有一个实现文件；按键文件同时负责 GPIO 初始化、中断、读取和消抖。OpenCPU 启动任务按构建清单逐个初始化所需器件，选择器检查器件引脚冲突与配置；`indicator.c` 只计算声光时序。两个产品的器件与源码仍保持隔离。功耗接口暂集中在 system_interface.h，产品决定休眠条件，ML307Y 适配负责工作锁；电池文件读取内部 VBAT，不占用接 LED 的 ADC1。

## 构建

从 SDK 根目录运行：

```powershell
python project/tools/build_product.py alarm_button
python project/tools/build_product.py template_test
python project/tools/check_artifacts.py
```

构建工具串行执行配套底包与应用步骤，保存各产品日志、ELF、MAP、app.img、mimgx 和 release.json。输出分别位于：

- 报警：out/products/alarm_button/alarm_board_v1/ml307y/
- 模板：out/products/template_test/virtual/ml307y/
- 配套底包：out/project-base/<底包内容标识>/

每个产品独立对象目录、SCons 签名缓存、生成配置及固件名；底包可按内容标识复用。build_product.py 的进程锁保护共享厂商生成步骤，切勿同时手工启动其他厂商构建。中断留下锁时先检查锁内 PID 确认进程退出，再移除该锁。

**默认报警包仍需实板与平台验收。**按用户指定的原理图，按键初始化直接尝试模组物理 26 脚的 IOMUX 和 CM GPIO26，并在唤醒未核验时由前台轮询；原理图将该脚标为 RSV，能否读取实体按键必须上板确认。物理 96 脚 LED 暂时禁用，不进行复用、初始化或输出。账号、密钥和平台约定尚未配置；模板包只输出启动诊断，不控制硬件。

报警包上电后 UART0 先输出 `UART0 ready`。若 26 脚 IOMUX、GPIO 初始化或首次读取失败，会先输出带 SDK 返回码的 `alarm-key-*` 诊断，再输出 `alarm-key-init`；后续器件、存储、网络及报警任务不会启动。按键初始化成功后输出 `alarm key ready`；其他器件就绪后平台会分别输出 `task alarm-ui created` 和 `task alarm-worker created`。任务创建日志只证明 RTOS 接受创建请求，实体按键触发、任务持续运行及复位原因仍需实机观察。

## 报警行为

- 每次 30 ms 消抖后的有效按下排队一个独立保存请求；前台立即开始提示。
- 0～1 秒常亮静音，1～4 秒按 250 ms 开／250 ms 关同步闪鸣，共六个周期。
- 前四秒不受提前确认打断。四秒仍未成功则继续常亮，成功立即灭，最晚在第 34 秒灭。
- 未确认记录仍保留并间隔重试。稳定释放后重按新增记录，提示关联最新请求，旧事件确认不结束新提示。
- 后台完成持久化后才发送；只有匹配铠湾 0xFF/0x00 业务回执且队列删除成功才通知前台完成。MQTT PUBACK 仅是传输结果。
- 无待保存／待确认事件、提示结束、按键释放且无本地故障，才具备产品休眠条件；板唤醒未核验时始终保持工作。

前台只采样按键、更新声光、投递消息；后台独占文件、协议和 MQTT。先亮灯与真正持久化完成有时间差：保存成功前立即掉电的新按键无法保证保留；不得把 RAM 排队成功称为已落盘。

## 回归与扩展

```powershell
python project/tools/run_host_tests.py --sim
python project/tools/run_host_tests.py --cc "<本机原生 gcc、clang 或 tcc 的完整路径>"
python -m unittest discover -s project/tests -p "test_*.py" -v
```

`--sim` 使用工程自带的 RV64 编译器和模拟器执行 C 用例，不需另装原生编译器；它验证软件行为，不代表实机 GPIO、串口、功耗或复位测试。主机测试的 AES 参考源码及配套头文件仅位于 tests/vendor，不参与固件。固件使用当前 SDK 的 mbedTLS 3.6.4 头文件和当前底包，编译时检查 AES 上下文 ABI。

新增产品请看 [完整示例](docs/new_product_guide.md)，上板前请看 [硬件与平台核验项](docs/hardware_and_platform.md)，已执行测试及边界见 [迁移验收记录](docs/migration_validation.md)。
