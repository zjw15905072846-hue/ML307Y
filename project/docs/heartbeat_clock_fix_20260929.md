# 心跳确认后重复发送修复

用户日志显示首次心跳于 11:17:36 确认，一小时后的 12:17:37 发送心跳序号 11 并确认，但随即额外发送序号 12。因此周期生效，缺陷是确认后重复发送。

## 原因与修复

```text
后台取得时间 now
    ↓
处理 MQTT 下行
    ↓
心跳业务成功回调以较新时间更新 last_heartbeat
    ↓
旧实现仍用较旧的 now 减去 last_heartbeat
    ↓
无符号差值变大，误判周期到期
    ↓
额外发送一条心跳

修复：处理 MQTT 事件后重新获取 now
    ↓
使用最新时间判断周期
    ↓
刚确认成功时不再重复发送
    ↓
下一完整周期到期才发送下一条心跳
```

生产修改仅为 `alarm_process_background()` 在 `transport->poll()` 后刷新时间，保留无符号差值对真实计时回卷的支持。位置：[project/src/alarm_button/alarm_runtime.c:921](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/alarm_button/alarm_runtime.c:921)。

复现测试 `test_heartbeat_callback_clock()` 让下行回调中的时钟前进 5ms，并走真实业务回执解析；分别测试普通时刻与 UINT32 回卷，验证当次只发送一次、下一周期前不发送、到期再次发送。位置：[project/tests/alarm/test_runtime.c:855](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/tests/alarm/test_runtime.c:855)。

## 验证证据

- 修改前复现重复发送断言失败：[red.log](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/heartbeat-clock-fix-20260929/red.log:1)。
- 修复后 15 组 RV64 模拟器 C 测试及配置变体通过，包含一小时、八小时和回卷验证：[c-tests.log](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/heartbeat-clock-fix-20260929/c-tests.log:1)。模拟器不是原生主机 C 执行，也不是硬件验证；既有测试原子宏重定义警告仍保留。
- 38 项 Python 主机检查通过：[python-tests.log](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/heartbeat-clock-fix-20260929/python-tests.log:1)。
- 报警固件交叉构建通过：[build.log](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/heartbeat-clock-fix-20260929/build.log:1)。模板复用已有产物，本次未重建模板。
- 报警与模板配套检查通过；报警构建打包已执行底包及最终烧录包单次解压后与 ELF 机器码逐字节比较：[artifacts.log](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/heartbeat-clock-fix-20260929/artifacts.log:1)。
- 已保存当前项目基线和旧固件，保留此前电池采样等改动；私有配置与基线逐字节一致。基线散列：[baseline-hash.json](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/heartbeat-clock-fix-20260929/baseline-hash.json:1)。本轮源码与测试增量：[changes.diff](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/heartbeat-clock-fix-20260929/changes.diff:1)。

## 交付与实板待验

[一小时心跳时间修复包](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/heartbeat-clock-fix-20260929/ML307Y_alarm_button_heartbeat_clock_fix_1h.mimgx)（二进制无行号）。

SHA-256：`1e9e429960f9cd0cdfa927130af0362389147679bc1c92e7d6998d2c531d6ecc`。

当前尚未烧录此包。实板验证应记录同次启动、首次心跳成功后至少一小时的完整日志，确认到期只出现一条正常心跳且收到业务成功回执；随后检查下一期限、期间按键报警以及断线恢复。业务回执缺失时重试仍为预期行为，不能将其一概禁止。平台页面显示问题仍需以设备、时间范围及刷新结果单独核对。
