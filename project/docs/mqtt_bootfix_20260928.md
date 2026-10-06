# 2026-09-28：MQTT 联调包无法启动的打包回归修复

用户本次提供的旧存储包日志确认：初始化完成、调度持续运行、按键后快照写入及读回均成功。旧包的 `alarm-worker error=-10` 是云配置未就绪，不能据此判定死机。用户此前已确认灯光和声音正常。

本次找到并修复底包重复压缩缺陷。新包已通过构建与机器码核验；截至本记录创建时，尚未获得新包实板启动日志和平台业务回执。

## 根因与证据

底包 objcopy 原始输出与压缩输出原先共用目录。SDK 压缩器原地改写输入，增量构建可再次压缩已经压缩过的文件。设备启动只解压一次，得到的仍是压缩数据，而非应执行的机器码。

旧正常包与本次修复前坏包的底包 ELF 相同，但实际烧录段不同。逐段解压结果如下：

| 段 | ELF 机器码字节数 | 旧正常包解压字节数 | 坏包解压字节数 |
| --- | ---: | ---: | ---: |
| AP SYSRAM | 27160 | 27160，逐字节一致 | 14656，不一致 |
| AP PSRAM | 638424 | 638424，逐字节一致 | 397568，不一致 |

两包原有 HMAC 均正确，因此单独验证文件哈希或下载 PASS 无法发现该缺陷。详见[新旧包解压证据](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-regression-20260928/decompression-evidence.json:1)。

## 修改定位

| 工程相对路径、文件名 | 函数或配置段 | 修改 |
| --- | --- | --- |
| [SConscript-k](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/SConscript-k:60) | step4 段拷贝 | 原始机器码独立保存在 origin_bins |
| [tools/scons/KernelTools.py](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/tools/scons/KernelTools.py:91) | generate_img | 每次从原始机器码复制后压缩；压缩、打包、分区检查失败均返回失败 |
| [project/build/artifacts.py](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/build/artifacts.py:60) | verify_ap_image | 验证段完整性，解压一次后逐字节核对机器码 |
| [project/build/artifacts.py](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/build/artifacts.py:116) | verify_base_machine_code | 从 ELF 还原预期段内容；检查底包及最终烧录包 |
| [project/tests/test_kernel_packaging.py](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/tests/test_kernel_packaging.py:62) | KernelPackagingTests | 实际调用 SDK 压缩及打包，覆盖重复压缩、工具失败、损坏包 |
| [project/tests/test_selection.py](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/tests/test_selection.py:151) | test_foreign_base_and_corrupted_exports | 原有哈希配套单元测试隔离机器码校验；机器码由新增测试覆盖 |
| [project/src/ml307y/product_start.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/product_start.c:60) | product_boot_initialize | 启动标识更新为 mqtt-bootfix-20260928 |
| [AGENTS.md](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/AGENTS.md:81) | 本次用户更正规则 | 以正常旧包为基线，明确增量打包及机器码核验要求 |

按三批保存修改前基线，每批最多三份源文件或文档。按键、LED、蜂鸣器和存储业务代码未在本次修复中修改。私有平台配置与已批准的明文联调版本哈希一致；继续保留本机 IMEI Topic、厂商标识 4872、12 小时心跳、未知电量 0xFF、注册及上报打印。

## 验证结果与边界

- 修复前先复现：原始机器码被改写、第二次打包内容变化、工具错误被漏报；修复后均通过。
- 新的解压检查放过旧正常包，拒绝修复前的坏包，即使其 HMAC 完整正确。
- 36 项 Python 测试通过；12 组 RV64 C 模拟测试及额外的明文 MQTT、蜂鸣器宏覆盖测试通过。模拟测试仍有既有 mock 原子宏重定义告警，生产构建没有这些告警。
- 报警和模板分别完成底包、应用交叉编译及包核验，生产构建均零告警、零错误。
- 同一产品同一参数连续增量构建：AP 镜像、ELF、接口导出表及三个原始段哈希完全一致。跨产品构建时 ELF 调试元数据会因编译宏不同而变化，AP 实际内容及接口地址保持一致。
- 最终烧录包中的底包完成一次解压后，与本次配套 ELF 机器码一致。记录见[核验清单](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-regression-20260928/verification.json:1)和[重复构建核验](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-regression-20260928/repeat-verification.json:1)。

## 本次交付与实板验收

烧录[ML307Y_alarm_button_mqtt_bootfix_20260928.mimgx](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/mqtt-regression-20260928/ML307Y_alarm_button_mqtt_bootfix_20260928.mimgx)。大小 10443211 字节，SHA-256：

`92870ce745e774d5f3447a722dd0c0f72a42d9efd8902b033aea25b5b525042c`

本次启动标识：`[project] diagnostic=mqtt-bootfix-20260928`。

后续实板测试覆盖：

1. 冷启动：COM12、115200/8N1 收到上述标识和初始化完成，确认两个任务及按键持续执行。
2. 按键和存储：多次按下，灯光、鸣叫节奏正确，每次新增记录且写入读回成功；断电后待确认记录仍保留。
3. MQTT：确认联网、订阅完成、打印注册报文及本机 IMEI Topic；收到平台注册成功业务回执后，再确认心跳和新按键报警上报。
4. 回执边界：仅 MQTT 入队成功或 PUBACK 不删除报警；只有匹配的 FF/00 业务成功回执才能完成记录。断线重连及失败重试保留事件。

平台是否接受明文报文、未知电量 0xFF 和配对下行 Topic，仍以真实平台回执为准。此次未启用协议第四部分 AES，factoryCode 不阻塞本轮明文发送。任何没有有效时间的历史记录不伪造时间补报。
