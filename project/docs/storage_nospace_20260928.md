# 报警快照写入负 28：定位与修复验证

日期：2026-09-28；本轮只处理当前 D 盘工程。保存失败的定位与修复沿用用户已批准的范围。

## 修复前实板事实

用户提供的 09:49:30～09:50:00 日志对应 09:41:52 编译版本：三个器件初始化成功，前后台任务创建成功；slot 0 恢复了 1980 字节，slot 1 随后写入并读回 1980 字节成功。下一次覆盖 slot 0 时，写接口返回 -28，向业务层传播为 -2。其后 scheduler alive 的 tick 继续递增到 30054，说明该窗口中调度仍在运行。

用户随后明确确认：按键触发后“灯和声音都正常”。这是该版固件的实板声光确认；不等同于示波器测频或新存储修复包已上板。

原始记录：[用户提供的 UART0 日志](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/storage-nospace-20260928/user-uart0-094930.log:1)。

## 已确认与待验证的原因

- 已从配套底包 ELF 的 LittleFS 枚举确认 LFS_ERR_NOSPC=-28，分配器耗尽路径也直接返回 -28；cm_fs_write → xy_fwrite → fs_write → lfs_file_write 保留此错误。它与业务层 -10（云配置未启用/不完整）是两个问题。
- 配套 ELF 的 working_fs_len 为 32768，user_fs_flash_len 为 0；分区初始化把 working_fs_len 除以 4096 作为块数。证据：[SDK 文件系统核验](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/storage-nospace-20260928/sdk-filesystem-evidence.json:1)。后续 10:10～10:11 实板日志也确认 total=32768，容量变化见下节。
- 针对旧槽覆写的机制：LittleFS 的截断先修改打开文件状态，旧目录记录在提交前仍可能占用原数据块。未同步截断就写新内容，在没有备用块时会失败。容量模型复现了与实板相同的 slot=0、result=-28、bytes=0/1980；实板是否仅由此机制导致，须看修复包运行及容量日志。

## 修改范围与安全边界

| 工程相对路径与文件名 | 函数 | 修改 |
|---|---|---|
| [project/src/ml307y/file_port.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/file_port.c:144) | ml307y_file_write | 打开非活动槽后，先同步截断，再写新镜像；原有写满、同步、关闭、回读校验保留 |
| [project/src/ml307y/file_port.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/file_port.c:47) | ml307y_file_trace_space | 记录截断前后 free/total 和查询返回码，容量查询仅用于诊断 |
| [project/tests/alarm/test_file_port.c](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/tests/alarm/test_file_port.c:222) | test_full_volume_snapshot_reuse | 覆盖旧块未释放、真正满卷、截断同步失败和写入失败，检查活动槽与代数不被错误更新 |

不删除文件、不清空报警队列、不格式化文件系统、不扩大或搬动 Flash 分区。复用的是双快照算法已经选定的非活动槽；当前有效槽直到新快照读回验证通过才切换。失败仍明确返回，不能靠忽略 -28 伪装成功。

## 软件验证

- 修改前，使用保存的旧生产代码与同一测试运行，复现 -28 和 0/1980；修改后同一容量模型与失败保护测试通过。见[失败复现](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/storage-nospace-20260928/file-port-red.log:1)、[定向测试通过](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/storage-nospace-20260928/file-port-green.log:1)。测试模拟分配约束，未执行厂家 LittleFS 指令或真实 Flash。
- 12 组 RV64 模拟器 C 回归及蜂鸣器宏覆盖测试通过，见[C 回归日志](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/storage-nospace-20260928/c-tests.log:1)。测试桩仍有既有原子宏重定义警告；本次未改动该桩，实际固件编译无此警告。
- 30 项 Python 检查通过；报警固件以用户私有配置重建成功；底包、应用、接口表及模板产品隔离的配套检查通过。固件编译日志无 warning/error 诊断。
- 本次没有修改底包，配套标识仍为 722b56135a0f6fb6874a；MQTT/AES 参数、12 小时心跳及 4000 Hz、50% 蜂鸣器宏保持原值。

## 新烧录包

[ML307Y_alarm_button.mimgx](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/products/alarm_button/alarm_board_v1/ml307y/image/ML307Y_alarm_button.mimgx)

生成时间：2026-09-28 10:03:05；SHA-256：`8d0ed63ff4868aef907c3c3fb95721afcd180bfe8e35c3127f7f64639303b9ec`。

启动横幅的日期来自启动文件的编译时间；该文件本次未修改，增量构建可能仍显示 09:41:52。用包哈希及新增 space-before / space-after-truncate 日志核对本次版本，不只凭启动日期判断是否烧入新包。

## 上板验收与剩余问题

1. 使用本次配套包，保留原存储数据，上电后连续按下并稳定释放至少五次；两槽应交替出现 write-complete 与 read-complete，均为 1980/1980。
2. 观察 space-before 与 space-after-truncate 的 free/total；若仍有 -28 或 truncate-sync 错误，保留同次上电完整日志，继续判断真实容量及目录元数据占用，不清理未知文件。
3. 至少完成一次正常保存后再断电重启，确认能恢复最新完整快照；写入/同步中断后的恢复仍需专门实板掉电测试。
4. error=-10 仍表示云配置条件未满足，等待厂商标识、厂商码和平台样例确认；不能把它当成本次文件写入失败。
5. scheduler alive 只打印六次，之后静默是当前诊断设计；是否持续正常仍需按键响应、日志和长期实测判断。

## 10:10～10:11 连续保存实板结果

用户随后提供了包含新增容量诊断的[UART0 日志](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/storage-nospace-20260928/user-uart0-101046.log:1)。启动时两槽 probe result=1，表示文件均不存在；随后首次初始化保存和后续五次保存均成功，槽顺序为 0、1、0、1、0、1。六次 write-complete 与六次 read-complete 都返回 0，字节数均为 1980/1980，没有 -28 或 -2。

板上 total 为 32768 字节。后四次覆写前 free=0，同步截断后均变为 4096，随后完整写入并读回成功。这为旧槽复用时释放空间的修复提供了实板证据；本次只验收该段连续保存，不代表无限次写入或 Flash 寿命验证。

启动两槽缺失的原因尚不明确，本段不是“已有报警断电后恢复”的证据。下一步须保持当前固件和文件系统，完成一次保存后正常断电再上电，核对已有槽 probe=0、完整读取及随后按键保存；写入途中掉电仍需单独测试。

alarm-worker error=-10 仍来自云配置未满足，MQTT 登录凭据已提供，厂商标识、32 字节厂商码与平台协议样例仍待确认。六次 scheduler alive 后停止该周期打印是既定行为，本段后续存储日志仍持续到 10:11:48。

当前结论：初始化与声光已有实板反馈，连续交替保存已通过本段实板验证；正常断电恢复、写入中断电保护、长期运行、MQTT 真实上报和匹配业务回执闭环仍未完成。本次仅更新证据记录，没有改动固件或重新构建。
