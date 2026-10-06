# 注册和周期心跳重连修复（2026-09-29）

本次批准行为：每次上电注册成功后立即发送首次心跳，之后从最近心跳业务成功回执起按配置周期计时。普通 MQTT 重连重新订阅下行，但不清除本次运行已成功的注册状态，不额外触发心跳。未确认业务继续重试。

## 实板问题证据

修复前 COM12 连续采集了三轮：10:04:00、10:07:20、10:10:41 开始 SDK 自动重连，约 25 秒后连接和订阅恢复，随后注册/心跳序号分别为 9/10、11/12、13/14，均收到平台业务成功回执。间隔约 200.3、201.0 秒。没有新的启动打印，连接代次与序号持续递增。

[修复前三轮完整串口日志](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/reconnect-analysis-20260929/com12-long-100009.log:1)。截图此前也出现约 381 秒间隔；不能把本次 201 秒视为所有场景固定周期。

已确认重复上报由重连重置业务状态触发；旧日志只报告“正在重连”，没有给出最初断线原因。保活配置不一致是已发现的配置问题，修正后是否消除实际掉线仍需新包实测。

## 修改定位

| 工程相对路径、文件与准确行号 | 所在函数或配置段 | 行为 |
|---|---|---|
| [project/src/alarm_button/alarm_runtime.c:505](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_runtime.c:505) | alarm_on_cloud_state | 保留已确认注册、未完成心跳和原计时起点；断线取消失效传输等待 |
| [project/src/alarm_button/alarm_runtime.c:786](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_runtime.c:786) | alarm_handle_front_request | 根据最新心跳确认时间复核前台通知，丢弃已过时的心跳唤醒请求 |
| [project/src/ml307y/mqtt_port.c:411](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/mqtt_port.c:411) | ml307y_start | 使用同一 keepalive_seconds 配置 CONNECT 及 SDK PING 周期；PING 配置失败返回错误 |
| [project/private/alarm_cloud.h:15](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/private/alarm_cloud.h:15) | ALARM_BUTTON_HEARTBEAT_HOURS | 保留当前 1U；改为 8U 后重新构建、烧录即为八小时。仅更新过时注释，所有私有宏值保持不变 |
| [project/tests/alarm/test_runtime.c:674](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/tests/alarm/test_runtime.c:674) | test_reconnect_heartbeat | 三次重连无额外上报、期限前后、回卷、旧通知、未确认心跳重连及超时重试 |

```text
上电
├─ 连接 → 下行 SUBACK → 注册 → 注册业务确认
├─ 首次心跳 → 心跳业务确认 → 开始周期计时
└─ 运行
   ├─ 周期未到 → 不发业务心跳
   ├─ 周期到达 → 心跳 → 业务确认后更新下一期限
   ├─ 按键报警 → 原消抖、声光、持久保存和业务确认规则
   └─ 意外断线
      ├─ MQTT 重连 → 重新订阅 → SUBACK 后允许业务发送
      ├─ 尚未注册成功 → 继续注册重试
      ├─ 已注册成功且心跳未到期 → 保留状态，继续等待
      ├─ 心跳到期或未完成 → 补发或重试心跳
      └─ 待确认报警 → 保留记录并按原规则继续重试
```

## 基线、复现与验证

- 修改前保存项目快照及 SHA-256，旧固件和发布回执也已备份：[基线清单](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/reconnect-fix-baseline-20260929-101829/manifest.json:1)。按每批最多三份源文件实施。
- 先失败再修复：[重连状态复现](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/reconnect-analysis-20260929/red-test_runtime.log:1)、[PING 配置失败处理复现](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/reconnect-analysis-20260929/red-test_mqtt_port.log:1)、[旧唤醒通知重复心跳复现](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/reconnect-analysis-20260929/red-stale-wake.log:1)。
- 14 组 RV64 模拟器 C 回归通过，并通过明文、一小时、八小时运行时及蜂鸣器宏覆盖变体：[C 回归日志](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/reconnect-analysis-20260929/regression-c.log:1)。属于模拟器回归，不是原生主机 C 执行或实板验证；保留既有测试原子宏重定义警告。
- 38 项 Python 主机检查通过：[Python 日志](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/reconnect-analysis-20260929/regression-python.log:1)。
- 报警及模板交叉构建通过：[报警构建](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/reconnect-analysis-20260929/build-alarm.log:1)、[模板构建](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/reconnect-analysis-20260929/build-template.log:1)。
- 产品隔离、双 bin 配套及底包和最终烧录包单次解压后与 ELF 机器码逐字节比较通过：[产物检查](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/reconnect-analysis-20260929/artifacts.log:1)。
- 本轮相对已保存基线的变化：[增量差异](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/reconnect-analysis-20260929/approved-change.diff:1)。保留原有休眠工作，不回滚用户已有改动。

## 固件与验收边界

[一小时心跳修复烧录包](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/reconnect-analysis-20260929/ML307Y_alarm_button_reconnect_fix_1h.mimgx)（二进制文件无行号）。

SHA-256：`f2f1f7f8d827d52022ab98eb744c995b992942091ac9c2ebce675724f5ce288a`，10448216 字节，配套底包标识 `c7b11ec45f724c1fb972`。[核验清单](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/reconnect-analysis-20260929/verification.json:1)。

当前新包尚未烧录，不能用旧包三轮日志宣称修复完成。等待用户烧录并释放 COM12 后：

1. 记录同次启动的注册与首次心跳业务回执；持续观察至少三个旧故障周期。为覆盖已见的 381 秒间隔，建议不少于 20 分钟。
2. 如仍发生网络重连，确认重新订阅后没有多余注册或未到期心跳；原始断线原因仍需进一步诊断。
3. 验证心跳业务确认后一小时到期只发送心跳。八小时配置已模拟通过，但真实八小时运行尚未执行。
4. 验证离线跨期限恢复后补发、业务回执超时继续重试；重连不丢失待确认报警。保留原匹配业务回执及持久删除要求，PUBACK 不代替业务确认。
5. 如果平台会在断线后主动清除业务注册，需要平台给出明确的失效返回码再制定恢复规则；本轮按用户批准策略保留本次运行已确认注册，不猜测协议状态码。

未改变协议字节、Topic、账号、报警节奏及 GPIO/PWM 配置；未承诺物理休眠、电流或原始断线根因已由本次源码检查证明。
