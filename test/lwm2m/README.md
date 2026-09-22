# LWM2M 测试使用说明书

本文档说明 `test/lwm2m` 测试模块的使用方法。该模块用于测试 `cm_lwm2m.h` 中定义的全部 LWM2M 接口，支持 OneNET、CTWing、DMP、华为云等多平台配置切换。

---

## 1. 概述

本测试模块将 LWM2M 各操作映射为串口 CLI 子命令，做到：

- **接口全覆盖**：`cm_lwm2m.h` 中 16 个接口（create/delete/add_obj/del_obj/discover/open/update/close/notify_packing/notify/read_rsp/write_rsp/execute_rsp/param_rsp/observe_rsp/set_version）均有对应命令；
- **配置可调**：服务器地址、平台类型、endpoint 模式、认证码、PSK、生命周期等均可运行时配置；
- **多平台支持**：内置 OneNET / CTWing 预设参数，一键切换；
- **回调可控**：8 个回调（read/write/execute/observe/discover/params/event/notify）全部实现，支持「自动应答」与「手动应答」两种模式，便于测试 `*_rsp` 接口；
- **关键字输入**：`pack` 命令的 type/ctype 支持关键字（如 `float`、`tlv`）或数字（如 `4`），无需查表；
- **一键自测**：`autotest` 命令在独立线程中执行连通性冒烟测试，自动等待 PDP 就绪并验证完整链路。

## 2. 文件结构

```
test/lwm2m/
├── SConscript              # 构建脚本
├── inc/
│   └── cm_demo_lwm2m.h     # 对外头文件，声明 cm_test_lwm2m
├── src/
│   └── cm_demo_lwm2m.c     # 测试主体
├── README.md               # 本文档
└── lwm2m.ini               # 串口自动化测试脚本
```

## 3. 编译集成

本模块已在以下位置接入工程，编译时自动包含：

1. `test/SConscript` 的 `build_list` 中已添加 `'lwm2m'`；
2. `test/demo_main/src/cm_demo_main.c` 已包含 `cm_demo_lwm2m.h` 并注册命令：
   ```c
   {"lwm2m", "lwm2m <cmd> [<param>...]", true, NULL, cm_test_lwm2m}
   ```
3. `test/demo_main/SConscript` 的 `public_incs` 已包含 `../lwm2m/inc`；
4. `kernel/export/xy_export.list` 已导出全部 `cm_lwm2m_*` 符号，无需改动。

## 4. 命令总览

在串口终端输入 `lwm2m help` 可查看完整用法。所有命令以 `lwm2m` 开头，命令名不区分大小写。

### 4.1 配置类命令

> `lwm2m cfg`（不带子命令）等效于 `lwm2m cfg show`，直接显示当前配置。
> 若设备已创建（`lwm2m create` 已执行），修改配置会打印 warning 提示需 `delete` + `create` 才能生效。

| 命令 | 说明 |
|------|------|
| `lwm2m cfg platform <0/1/2/3/10>` | 设置目标平台：0=OneNET 1=CTWing 2=DMP 3=华为云 10=其他 |
| `lwm2m cfg host <ip[:port]>` | 服务器地址，port 省略时默认 5683 |
| `lwm2m cfg flag <flag>` | 标志位：bit0=bootstrap服务器，bit1=禁止monitor |
| `lwm2m cfg pattern <0-4>` | endpoint name 模式（详见 cm_lwm2m.h） |
| `lwm2m cfg epname <name>` | 自定义 endpoint name（仅 platform=10 生效） |
| `lwm2m cfg auth [code]` | 登录认证码；不带 code 则清除 |
| `lwm2m cfg psk [psk] [pskid]` | DTLS PSK 与 PSKID；不带参数则清除 |
| `lwm2m cfg autoupdate <0/1>` | 是否开启自动 update |
| `lwm2m cfg preset <onenet/ctwing>` | 加载预设参数 |
| `lwm2m cfg show` | 打印当前配置（敏感信息掩码显示） |

### 4.2 设备/对象操作命令

| 命令 | 对应接口 | 说明 |
|------|----------|------|
| `lwm2m create` | `cm_lwm2m_create` | 创建 LWM2M 实例；若使用默认配置会提示建议先 preset |
| `lwm2m delete` | `cm_lwm2m_delete` | 删除 LWM2M 实例 |
| `lwm2m addobj <objid> <inscount> <bitmap>` | `cm_lwm2m_add_obj` | 添加 object，bitmap 如 `10011` 表示第0、3、4个实例可用 |
| `lwm2m delobj <objid>` | `cm_lwm2m_del_obj` | 删除 object |
| `lwm2m discover <objid> <r1,r2,...>` | `cm_lwm2m_discover` | 设置 object 资源列表 |

### 4.3 连接管理命令

| 命令 | 对应接口 | 说明 |
|------|----------|------|
| `lwm2m open [timeout] [lifetime]` | `cm_lwm2m_open` | 登录平台，单位秒，默认 30 / 86400 |
| `lwm2m update <lifetime> <withobj>` | `cm_lwm2m_update` | 更新生命周期，withobj=1 同时更新 object |
| `lwm2m close` | `cm_lwm2m_close` | 退出登录 |

### 4.4 数据上报命令

| 命令 | 对应接口 | 说明 |
|------|----------|------|
| `lwm2m pack <obj> <ins> <res> <type> <value> [ctype]` | `cm_lwm2m_notify_packing` | 组包，可多次调用 |
| `lwm2m notify [mid]` | `cm_lwm2m_notify` | 上报已组包数据；mid 省略时自动递增，`-1` 为非响应模式（无 ack） |

**type（数据类型）**：支持关键字或数字

| 关键字 | 数字 | 说明 |
|--------|------|------|
| `string` | 1 | 字符串 |
| `opaque` | 2 | 不透明（16进制字符串，模组转 ASCII） |
| `int` | 3 | 整型 |
| `float` | 4 | 浮点型 |
| `bool` | 5 | 布尔型 |
| `hexstr` | 6 | hex 字符串（模组转 ASCII 后以 string 上报） |

**ctype（编码格式）**：支持关键字或数字

| 关键字 | 数字 | 说明 |
|--------|------|------|
| `default` | 0 | 平台默认 |
| `text` | 1 | TEXT 格式 |
| `link` | 2 | LINK 格式 |
| `opaque` | 3 | OPAQUE 格式 |
| `tlv` | 4 | TLV 格式 |
| `json` | 5 | JSON 格式 |

> 注意：仅 TLV 格式支持多次组包，其他格式需单次完成。组包数据总量建议不超过 1000 字节，超出时会打印 warning。

### 4.5 应答类命令

> 当 `autorsp=0`（关闭自动应答）时，需手动调用以下命令响应平台下行请求。mid 可用 `lwm2m state` 查询缓存的最近一次操作信息。

| 命令 | 对应接口 |
|------|----------|
| `lwm2m readrsp <mid> <result> [obj ins res type value]` | `cm_lwm2m_read_rsp` |
| `lwm2m writersp <mid> <result>` | `cm_lwm2m_write_rsp` |
| `lwm2m execrsp <mid> <result>` | `cm_lwm2m_execute_rsp` |
| `lwm2m paramrsp <mid> <result>` | `cm_lwm2m_param_rsp` |
| `lwm2m observersp <mid> <result>` | `cm_lwm2m_observe_rsp` |

`readrsp` 的可选参数 `type` 同样支持关键字（见 4.4）。省略可选参数时使用最近一次回调缓存的 obj/ins/res，自动回 `type=string value="ok"`。

**result（结果码）**：参考 `cm_lwm2m_result_e`，常用：1=Content OK 2=Changed 11=Bad Request 13=Not Found

### 4.6 辅助命令

| 命令 | 说明 |
|------|------|
| `lwm2m version [ver]` | `cm_lwm2m_set_version`，设置版本号（最长 15 字节）；不带参数时显示当前版本 |
| `lwm2m autorsp [0/1]` | 切换回调自动应答开关，默认 1；不带参数时显示当前状态 |
| `lwm2m state` | 显示连接状态（可读字符串）、实例句柄、最近一次平台下行操作 mid/obj/ins/res |
| `lwm2m autotest` | 在独立线程跑连通性冒烟测试 |
| `lwm2m help` | 打印用法 |

## 5. 回调说明

模块实现了全部 8 个回调，行为如下：

| 回调 | 行为 |
|------|------|
| `onEvent` | 打印事件名（数字转可读字符串），更新连接状态，跟踪 notify 成功/失败 |
| `onNotify` | 打印上报响应 mid，标记 notify 已确认 |
| `onRead` | 打印 obj/ins/res，缓存 mid；autorsp=1 时自动回 `read_rsp` |
| `onWrite` | 打印写入数据，缓存 mid；autorsp=1 时自动回 `write_rsp` |
| `onExec` | 打印执行参数，缓存 mid；autorsp=1 时自动回 `execute_rsp` |
| `onObserve` | 打印订阅/取消订阅，缓存 mid；autorsp=1 时自动回 `observe_rsp` |
| `onDiscover` | 打印 discover 请求，缓存 mid |
| `onParams` | 打印策略参数，缓存 mid；autorsp=1 时自动回 `param_rsp` |

## 6. 使用示例

### 6.1 OneNET 完整流程（手动逐步）

```
lwm2m cfg preset onenet
lwm2m cfg show
lwm2m create
lwm2m addobj 3303 1 1
lwm2m discover 3303 5700,5601
lwm2m open 30 86400
# 等待回调打印 "[LWM2M][LWM2M] event:6(REG_SUCCESS)"
# 也可用 lwm2m state 查看注册状态
lwm2m pack 3303 0 5700 float 25.6 tlv
lwm2m notify 1
lwm2m close
lwm2m delete
```

### 6.2 CTWing 物模型上报

```
lwm2m cfg preset ctwing
lwm2m create
lwm2m addobj 19 1 1
lwm2m discover 19 0
lwm2m open 30 86400
# 等待注册成功
lwm2m pack 19 0 0 string {"serviceId":"4","longitude":30.34} text
lwm2m notify 1
lwm2m close
lwm2m delete
```

### 6.3 一键自测

```
lwm2m cfg preset onenet
lwm2m autotest
```

`autotest` 是连通性冒烟测试，自动执行 6 个步骤，每步打印 PASS/FAIL：

1. 等待 PDP 激活就绪（最多 60s）
2. 创建 LWM2M 实例
3. 添加默认 object（OneNET: 3303, CTWing: 19）
4. discover 资源
5. 登录平台并等待注册成功
6. 上报 1 条数据并等待 ack

完成后自动 close + delete 清理资源。若某步失败则跳过后续步骤直接清理。

> autotest 仅验证连通性，不演示完整业务上报。完整上报流程请参考 6.1/6.2 手动示例。

### 6.4 测试手动应答接口

```
lwm2m cfg preset onenet
lwm2m autorsp 0          # 关闭自动应答
lwm2m create
lwm2m addobj 3303 1 1
lwm2m open 30 86400
# 平台下发 read/write/execute 后，回调会打印 mid
lwm2m state              # 查看连接状态和缓存的最近一次下行 mid/obj/ins/res
lwm2m writersp <mid> 2   # 手动响应 write
lwm2m readrsp <mid> 1 3303 0 5700 float 26.5   # 手动响应 read
```

### 6.5 多次组包上报（TLV）

```
lwm2m pack 3303 0 5700 float 25.6 tlv
lwm2m pack 3303 0 5601 int 100 tlv
lwm2m notify 1
```

## 7. 平台参数参考

| 平台 | platform | 默认 host | pattern | flag |
|------|----------|-----------|---------|------|
| OneNET | 0 | 183.230.40.39 | 2 | 3 |
| CTWing | 1 | 221.229.214.202 | 1 | 0 |
| DMP | 2 | (需用户提供) | 1 | 0 |
| 华为云 | 3 | (需用户提供) | 1 | 0 |
| 其他 | 10 | (需用户提供) | 自定义 epname | 0 |

> 不同平台的资源编码格式（content_type）支持情况不同，请与平台确认。OneNET 开发时支持 TLV，CTWing 物模型常用 TEXT/JSON。

## 8. 错误码

接口返回值参考 `cm_lwm2m_erroc_e`：

| 错误码 | 含义 |
|--------|------|
| 0 | 成功 |
| 100 | 未知错误 |
| 601 | 语法/句法错误 |
| 602 | 设备未登录或登录中 |
| 651 | 操作不被允许 |
| 652 | Uplink Busy |
| 653 | 资源操作错误 |

## 9. 注意事项

1. **网络就绪**：执行 `create/open` 前请确保 PDP 已激活（`autotest` 会自动等待）。
2. **配置生效**：设备已创建后修改配置不会立即生效，需先 `lwm2m delete` 再 `lwm2m create`。修改配置时会打印 warning 提示。
3. **命令不阻塞**：`open/update/close` 等为异步接口，操作结果通过 `onEvent` 回调体现，请勿在同一流程中连续调用导致状态冲突。
4. **mid 递增**：响应模式下，同一 mid 在短时间内可能被平台过滤，`notify` 默认采用递增 mid。`mid=-1` 为非响应模式（无 ack 回调）。
5. **生命周期**：`lifetime` 建议 >= 60s，自动 update 功能方生效。
6. **关键字输入**：`pack` 和 `readrsp` 的 `type` 参数、`pack` 的 `ctype` 参数均支持关键字（如 `float`、`tlv`）或数字（如 `4`），两种方式等效。
7. **敏感信息**：`cfg show` 中 PSK、auth_code、PSKID 以掩码显示（`****(len=N)`），不泄露实际内容。
8. **资源释放**：测试完成后调用 `close` 再 `delete`，避免资源泄漏。
