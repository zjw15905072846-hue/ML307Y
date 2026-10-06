# MQTT 回调后复位修复记录

## 现象与证据

用户提供的 14:14 日志显示 mqtt-poll-atomic-ok、PDP 激活、连接请求返回 0，随后 mqtt-connack 输出截断并再次启动，上电原因值为 2。用户已确认没有手动复位。

当前底包的 cm_free 调用 osMemoryFreeDebug；该函数在参数为 NULL 时进入异常处理，不具有 libc free(NULL) 的无操作语义。反汇编摘录保存在 out/mqtt-null-free-20260928/sdk-free-evidence.txt。

MQTT 连接、订阅和发布确认事件不分配 payload；旧实现仍在正常消费、过期丢弃、队列满丢弃时无条件释放该字段。此缺陷与连接回调后的复位位置吻合；尚未取得新包实板运行日志，不能宣称所有复位原因已排除。

## 本次修改

- project/src/ml307y/mqtt_port.c：三处事件 payload 释放先检查非空，保留接收缓冲回收、订阅门控、断线重连及业务回执规则。
- project/tests/mocks/cm_mem.c：模拟 SDK 拒绝 NULL 释放，避免测试使用 libc 语义掩盖缺陷。原代码首先触发断言，修复后同一测试通过。
- project/src/ml307y/product_start.c：启动标记改为 mqtt-freefix-20260928。
- AGENTS.md：记录 SDK 空指针释放限制与回归要求。

注册、心跳、报警仍打印实际 MQTT 发送缓冲、类别、序号、Topic 和入队结果。保留明文联调、未知电量 0xFF、每 12 小时业务心跳和本机 IMEI Topic；不打印密码或 AES 密钥。

## 验证结果

- 12 组 RV64 模拟器 C 测试及额外明文运行时、蜂鸣器宏覆盖测试通过。测试模拟头仍有既存原子宏重定义警告。
- 38 项 Python 测试通过。
- alarm_button 和 template_test 构建成功，产物隔离与校验通过。
- 最终烧录包内底包单次解压后与 ELF 机器码逐字节一致。
- 输出固件标记、明文模式和报文打印标记已核验。
- 实板持续运行、平台注册成功和报警业务确认尚待验证。

## 交付与实板用例

固件：out/mqtt-null-free-20260928/ML307Y_alarm_button_mqtt_freefix_20260928.mimgx

SHA256：8a743ac499773d8ddef1f2d17ad0dac29e4ed39f7b75782ab29d61ebbb22cce3

1. 烧入该包，在 COM12 以 115200/8N1 记录完整上电日志；确认 mqtt-freefix-20260928，连续观察至少 2 分钟，检查是否仍重复启动。
2. 检查 CONNACK、SUBACK 的完整返回结果，随后检查 type=registration 的实际 Topic、完整 data 和入队结果；平台业务回执单独确认。
3. 多次按键，检查声光、存储、type=alarm 上报及匹配 FF/00 回执；没有匹配回执时记录必须保留。
4. 断网重连后检查再次订阅、重试和按键响应，不能把 MQTT PUBACK 当作报警删除依据。

若平台拒绝明文或下行 Topic 不匹配，上报可能没有业务回执；应依据真实平台返回继续联调，不能伪造确认或删除未确认事件。
