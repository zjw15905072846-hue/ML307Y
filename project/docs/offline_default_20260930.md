# 默认离线待机开启交付

2026-09-30。按本次批准方案，仅开启报警默认离线待机并更新流程文档；保留现有 DEEP、八小时心跳及本轮开始前的所有改动。私有账户文件未修改。项目及前次报警固件已保存本地基线，基线含私有配置，不作为公开交付物。

## 当前固件实际配置

```text
ALARM_BUTTON_OFFLINE_STANDBY 1
ALARM_BUTTON_SLEEP_ENABLED 1
ML307Y_SLEEP_MODE CM_PM_SLEEP_MODE_DEEP
ALARM_BUTTON_HEARTBEAT_HOURS 8U
```

以上从交付 ELF 的编译宏核对；普通构建自动使用已有报警私有配置，没有额外使用验证配置覆盖周期。

```text
注册确认 → 首次心跳及业务确认
    ↓
全部报警确认并持久删除、存储完成、声光结束且按键释放
    ↓
停止 MQTT 自动重连 → 断开 MQTT → 关闭射频并读回核对
    失败 → 保持工作锁并重试
    成功
    ↓
复查新按键／业务
    有 → 恢复业务
    无
    ↓
释放前后台工作锁，允许 SDK DEEP
    ↓
按键／八小时心跳期限／失败后十五分钟期限唤醒
    ↓
恢复网络，继续业务，完成后返回休眠检查
```

未确认报警继续保持唤醒和重试。无报警时控制业务尝试两分钟，失败关网成功后十五分钟重试。心跳从上次成功业务确认计时，报警不重置；同次运行重连不重新注册。UART0 与 RAM 保留。

## 软件验证

- 41 项 Python 回归通过。
- 17 组 RV64 模拟 C 回归及配置变体通过，涵盖关网、失败预算、计时回卷、关网按键竞态、一小时／八小时心跳、业务回执、存储故障及 MQTT 事件溢出。
- 报警与模板交叉编译成功。全产品产物检查最初发现模板引用旧底包，重建模板后报警和模板配套检查全部通过。
- 独立底包及交付总包内底包单次解压后，与配套底包 ELF 机器码逐字节比较通过；应用另校验入口、产品隔离及哈希。
- 配置与流程之外的既有文件和私有配置与本轮基线一致。

## 交付与哈希

[八小时 DEEP 离线固件](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-default-20260930/ML307Y_offline_DEEP_8h.mimgx)

```text
6ccac83c6cb4ef3e2fd11db3e5a981db0dc48c323550fe3cabde1f559fad3c46  ML307Y_offline_DEEP_8h.elf
91342cec237c4e59c898c12266822b24ce9f085c9738da04bdf9239e4940efe9  ML307Y_offline_DEEP_8h.mimgx
```

- [默认离线开关](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/inc/alarm_button/product_config.h:24)
- [离线状态机 alarm_network_allow_processing](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_runtime.c:1058)
- [模式设置 ml307y_configure_sleep](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/system_port.c:673)
- [编译配置记录](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-default-20260930/compiled-config.txt:1)
- [Python 回归](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-default-20260930/python-tests.log:1)
- [C 回归](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-default-20260930/c-tests.log:1)
- [最终产物检查](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-default-20260930/artifacts-final.log:1)
- [机器码比较](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-default-20260930/machine-code-check.txt:1)

## 实板边界及可能问题

尚未烧录本包、观察 COM12 或测量电流。模式配置及工作锁释放不能代替真实休眠证据。

- 关网失败、未确认报警和保存／删除失败仍可能高电流：验证这些状态保持工作锁且记录不丢失。
- 验证休眠前后按下、关网瞬间按下、抖动、长按和释放重按，连续至少三轮实际休眠后仍能报警。
- 验证定时恢复、注册不重复、报警不推迟心跳以及失败十五分钟重试；八小时配置需实际等待或另行构建短周期测试包。
- 离线期间平台无法即时下行；按键上传等待联网恢复，测量按键到平台确认延迟。
- 明确供电和分析仪通道，在相同 UART／USB接线及窗口比较平均电流与周期电量；不承诺固定电流或低于原约1.51mA。
