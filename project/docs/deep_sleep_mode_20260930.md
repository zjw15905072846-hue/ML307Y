# SDK DEEP 离线验证交付记录

2026-09-30。软件实现、模拟回归、交叉编译和包一致性检查完成；未烧录、未采集 COM12、未测量新包电流。原 LIGHT 约 1.51mA 仅作为用户提供的对照值，不代表已核验测试条件。

## 实现与边界

公开 DEEP 配置允许底层 Sleep，仍限制 RAM 下电 Deepsleep。本轮不绕过底包、不改变任务实时性属性，保留 RAM、UART0、按键、原报警节奏、持久化及失败重试。普通构建默认为 LIGHT；模板拒绝显式 DEEP。私有账号配置及原离线业务代码已与基线逐字节核对，未修改。

```text
初始化并持有工作锁
    ↓
选择 LIGHT／DEEP → 注册状态回调 → 设置模式 → 读回核对
    ├─ 非法值／调用失败／模式不符 → 报错并保留工作锁
    └─ 成功
         ↓
注册、心跳、报警和存储按原规则执行
         ↓
报警已确认并持久删除、存储结束、声光结束且按键释放？
    否 → 继续处理并保持工作锁
    是
    ↓
停止 MQTT 自动重连 → 断开 MQTT → 关闭射频并核对
    失败 → 保持工作锁并重试
    成功
    ↓
再次检查按键及新业务
    有 → 恢复业务并持锁
    无
    ↓
前后台释放工作锁 → SDK 判断是否实际进入 Sleep
    ↓
按键／心跳期限／失败重试期限唤醒
    ↓
恢复网络及业务 → 完成后返回上述休眠检查
```

初始化预期诊断：`[project][sleep-mode] requested=2 readback=2 stage=get result=0`。LIGHT 对应 1。回调只记录状态，不直接打印。此诊断证明配置设置及读回一致，不证明实际进入 Sleep。

## 构建与交付

执行顺序为六十秒报警 DEEP、默认 LIGHT 模板、一小时报警 DEEP；最终报警输出为一小时。两份报警总包均包含配套底包，不应与旧应用或其他底包混搭。

```powershell
python project/tools/build_product.py alarm_button --sleep-mode deep --provision project/private/offline_standby_short_trial.h
python project/tools/build_product.py template_test
python project/tools/build_product.py alarm_button --sleep-mode deep --provision project/private/offline_standby_trial.h
```

- [一小时 DEEP 总包](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/deep-mode-20260930/ML307Y_offline_DEEP_1h.mimgx)
- [六十秒 DEEP 总包](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/deep-mode-20260930/ML307Y_offline_DEEP_60s.mimgx)
- [原一小时 LIGHT 回退包](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/offline-standby-20260930-100342/ML307Y_offline_trial_1h.mimgx)

SHA-256：

```text
e2ae3cf0437400b098018fd2c4c5b54d3c97ee6d9f7068bfc504045fc6e3c438  ML307Y_offline_DEEP_1h.elf
d5332fa0cce0e26141ef259c3ac0f7cfa25428f3514532890727598b19a4f87a  ML307Y_offline_DEEP_1h.mimgx
25058eb547e88f3dfe8859a509f177d4e7633d3a21ba381390f87c4a6ccc8f7c  ML307Y_offline_DEEP_60s.elf
bd6befb293475433e048a38e98106e002c90e75ffefad98f101906744265b0d3  ML307Y_offline_DEEP_60s.mimgx
```

## 软件验证

- 测试先行：模式选择测试在旧实现下失败；平台测试因尚未实现新接口而编译失败。实现后通过。
- 16 组 RV64 模拟 C 回归及配置变体通过，包括新增 DEEP 编译变体。覆盖模式设置失败、读回失败、不一致、非法模式和回调配置失败时不解锁。
- 40 项 Python 测试通过，包括默认 LIGHT、显式 DEEP、非法模式及模板 DEEP 拒绝。首次全量检查因新底包尚未生成导致四项构建图检查失败；重建底包后重跑全部通过。
- 既有离线回归继续通过：报警与存储失败阻止休眠、业务回执及提示时序、心跳一小时／八小时与回卷、重连不重复注册、通知溢出及停止恢复等。模拟覆盖不代替实板竞态测试。
- 底包、两份报警应用及模板交叉编译完成。产物检查验证入口、产品隔离、生成配置、缓存、双 bin 哈希配套。
- 独立底包与两份总包中的底包单次解压后，分别与底包 ELF 对应机器码逐字节比较通过。此项是底包机器码校验；应用另以 ELF 入口及产物哈希校验。
- 实际 ELF 调试宏确认：60s 为 60000ms，1h 为一小时计算式；两者离线开关为 1、模式为 DEEP。模板为 LIGHT。

- [C 回归日志](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/deep-mode-20260930/c-tests.log:1)
- [Python 回归日志](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/deep-mode-20260930/python-tests.log:1)
- [产物检查](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/deep-mode-20260930/artifacts.log:1)
- [机器码比较](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/deep-mode-20260930/machine-code-check.txt:1)
- [实际编译配置](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/deep-mode-20260930/compiled-config.txt:1)
- [哈希清单](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/out/deep-mode-20260930/sha256.txt:1)

## 可能的问题与待执行实板测试

1. 配置成功仍可能被其他底包锁阻止 Sleep：结合休眠状态和功耗波形核对，不凭静默或释放锁认定休眠。
2. 更深档位可能暴露按键／计时唤醒差异：连续至少三轮真实休眠后按键报警和定时心跳；覆盖入睡瞬间按下、抖动、长按及释放重按。
3. 网络恢复或平台确认失败可能维持高电流：验证未确认报警继续重试，控制业务两分钟退出、离线十五分钟后重试，按键可提前唤醒。
4. 使用六十秒包验证周期后切回一小时包；验证报警不重置心跳成功时间、同次运行不重复注册。
5. 明确分析仪通道、供电电压、UART／USB接线及测量窗口，在相同条件比较 LIGHT／DEEP 的平均电流、周期电量和按键至平台确认延迟。不承诺低于 1.51mA。

## 代码索引

- [默认档位宏](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/inc/ml307y/ml307y_port.h:5)
- [ml307y_configure_sleep：设置、读回和诊断](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/system_port.c:528)
- [ml307y_system_create：持锁后配置](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/src/ml307y/system_port.c:605)
- [构建参数 --sleep-mode](/D:/keil5/ML307Y/ML307Y-DL_OpenCPU_1.0.0.2609141255_rel/project/tools/build_product.py:17)
