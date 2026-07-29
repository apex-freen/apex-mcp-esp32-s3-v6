<p align="center">
  <img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License">
  <img src="https://img.shields.io/badge/ESP--IDF-v6.X-green.svg" alt="ESP-IDF">
  <img src="https://img.shields.io/badge/Platform-ESP32--S3-orange.svg" alt="Platform">
  <img src="https://img.shields.io/badge/MCP-Standard-purple.svg" alt="MCP">
  <img src="https://img.shields.io/badge/MCP_Spec-2026.07-blue.svg" alt="MCP Spec">
  <img src="https://img.shields.io/badge/Protocol-MQTT%20%2B%20JSON--RPC%202.0-lightgrey.svg" alt="Protocol">
</p>

<h1 align="center">apex-esp32-s3-v6-basic</h1>

<p align="center">
  <em>APEX 通用智能体设备框架 — ESP32-S3 基础参考实现<br>写一个 handler，注册一条指令，你的 AI 智能体就多一个技能。</em>
</p>

<p align="center">
  <a href="#-这是什么">是什么</a> ·
  <a href="#-核心能力">能力</a> ·
  <a href="#-架构">架构</a> ·
  <a href="#-快速开始">快速开始</a> ·
  <a href="#-内置指令">内置指令</a> ·
  <a href="#-开发你自己的指令">开发指南</a> ·
  <a href="./README.md">English</a>
</p>

---

## 📖 这是什么？

`apex-esp32-s3-v6-basic` 是 APEX 通用智能体设备框架基于 **ESP32-S3** 的 **基础参考实现**，基于**最新 MCP 协议规范（2026 年 7 月）**构建，属于 [apex_mcp_bridge（智能体工具中枢）](https://gitee.com/freen/apex-mcp-bridge) 生态系统中的硬件端，与 [service-plugins（插件框架）](https://gitee.com/freen/service-plugins) 共同构成完整的 AI 到硬件的技术栈。

这个项目提供了一套**完整的、可量产的固件底座**：

- 通过 **加密 MQTT** 与 apex_mcp_bridge 智能体工具中枢通信
- 将设备能力暴露为 **MCP (Model Context Protocol) 工具**
- 内置 **事件驱动的指令执行引擎**，含多重安全防护
- 附带 **同步/异步指令示例**，作为你开发自己模块的起点

> **一句话承诺**：拷贝这个项目，写你的硬件 handler，注册上去——你的 ESP32 设备就立刻能被任何支持 MCP 协议的智能体应用控制。**更进一步：你不需要自己写 handler。** 框架的标准化模板与约束就是为此设计的——任何智能体都能替你生成 handler 代码。你只需要描述你的设备要做什么。

---

## ✨ 核心能力

### 16 个核心框架模块

| 模块 | 职责 |
|------|------|
| `apex_core` | 框架总入口 & 事件总线 & 看门狗 |
| `apex_event` | 全局事件循环（含粘性事件回溯机制） |
| `apex_config` | 基于 NVS 的系统配置管理 |
| `apex_network` | Wi-Fi AP+STA 双模 + 自动切换策略 |
| `apex_mqtt` | MQTT 三通道通信（command/response/notify） |
| `apex_crypto` | AES-CCM-128 加密 + 时间戳防重放 |
| `apex_cmd_executor` | ⭐ **指令调度中枢**（框架核心） |
| `apex_webserver` | 嵌入式 Web 配置门户 |
| `apex_ota_update` | 双分区 OTA 无缝升级 |
| `apex_power_down` / `apex_power_up` | 电源管理 |
| `apex_reset` / `apex_restart` | 恢复出厂 / 重启 |
| `apex_stop` | 强制停止持续动作 |
| `apex_get_state` | 获取设备当前状态 |
| `apex_notify` | 设备主动事件通知（JSON-RPC 2.0） |

### 5 重安全防护

| 机制 | 触发条件 | 行为 |
|------|---------|------|
| **指令去重** | 同一 `msg_id` 2 秒内重复到达 | 拒收（`APEX_ERR_DUPLICATE`） |
| **熔断降级** | 连续失败 ≥ 5 次 | 该指令被熔断 5 分钟（`APEX_ERR_DEGRADED`） |
| **防重放** | 时间戳超出 ±30 秒窗口 | 拒收 + 记录日志 |
| **看门狗** | handler 执行超过 30 秒 | 设备自动复位 |
| **权限分级** | 无权限指令调用 | 拒收 |

### 示例模块

| 模块 | 类型 | 说明 |
|------|------|------|
| `sync_add` | 同步 | 接收两个整数，立即返回加法结果 |
| `async_add` | 异步 | 接收两个整数，延迟 100 秒后返回结果（模拟耗时操作） |

### 🤖 AI 就绪：模板驱动的代码生成

这个框架的真正威力在于：**你不需要自己写代码**。因为：

- **通信层**（WiFi、MQTT、AES-CCM 加密）已经完整内置
- **控制层**（指令调度、事件总线、安全机制）已经完整内置
- **模板约束**（handler 注册模式、参数 schema）已经标准化

任何支持 MCP 协议的智能体都能在这些约束范围内生成可用的 handler。你只需要描述设备该做什么：

```
"做一个 LED 闪烁的 handler，参数是 GPIO 引脚号和闪烁间隔毫秒数"
```

智能体会自动生成 `apex_cmd_handler_t` 结构体、`json_schema`
以及执行函数——完全匹配框架的契约。
注册、编译，你的设备就获得了新能力。

---

## 🏗️ 架构

### 分层设计

```
┌──────────────────────────────────────────┐
│                  main.c                   │  ← 程序入口
├──────────────────────────────────────────┤
│              apex_frame/                  │  ← 核心层（16 个模块）
│  ┌────────────────────────────────────┐  │
│  │  apex_cmd_executor                 │  │  ← 指令调度中枢
│  │  ┌──────────────────────────────┐  │  │
│  │  │ 解密 → 去重 → 匹配 → 锁定    │  │  │
│  │  │ → 熔断检查 → 执行 → 加密回传 │  │  │
│  │  └──────────────────────────────┘  │  │
│  │  ┌──────────────────────────────┐  │  │
│  │  │ 内置指令：                     │  │  │
│  │  │ getInfo│otaUpdate│getState    │  │  │
│  │  │ stop│powerDown│powerUp       │  │  │
│  │  │ reSet│reStart                │  │  │
│  │  └──────────────────────────────┘  │  │
│  └────────────────────────────────────┘  │
├──────────────────────────────────────────┤
│             components/                   │  ← 应用层
│  sync_add / async_add                     │     你的自定义模块放这里
├──────────────────────────────────────────┤
│              common/                      │  ← 基础层
│  utils (UUID、加密工具、HTTP、JSON)       │
└──────────────────────────────────────────┘
```

### 数据流

```
AI 智能体 (MCP Client)
    │ tools/call
    ▼
apex_mcp_bridge 智能体工具中枢
    │ AES-CCM 加密 MQTT → apex/{device_id}/command
    ▼
ESP32-S3
    │ Base64 解码 → AES-CCM 解密 → JSON 解析
    ▼
apex_cmd_executor
    │ 去重 → 匹配 → 锁定 → 熔断检查 → 调用 handler
    ▼
你的 Handler (sync_add / async_add / 自定义模块)
    │ 返回 APEX_OK / APEX_ASYNC_OK / 错误码
    ▼
加密 → MQTT → apex/{device_id}/response
    │
    ▼
apex_mcp_bridge → MCP 响应 → AI 智能体
```

---

## 🚀 快速开始

### 前置条件

- [ESP-IDF v6.X](https://docs.espressif.com/projects/esp-idf/)
- 一块 ESP32-S3 开发板

### 编译 & 烧录

```bash
cd apex-esp32-s3-v6-basic
idf.py set-target esp32s3
idf.py build
idf.py -p COM3 flash monitor
```

> 将 `COM3` 替换为你的串口号（Windows: `COM3`，Linux: `/dev/ttyUSB0`，macOS: `/dev/cu.usbserial-*`）。

### 首次配网

1. 设备上电后自动开启 AP 热点 **`APEX-XXXX`**（密码：`12345678`）
2. 手机连接该热点
3. 浏览器打开 `http://192.168.4.1`
4. 配置你的 WiFi 名称、密码，以及 apex_mcp_bridge 智能体工具中枢地址
5. 设备重启后自动连接路由器，5 分钟后 AP ��动关闭

---

## 📋 内置指令

以下指令在框架初始化时自动注册：

| `cmd_key` | 功能 | 描述 | `flags` |
|-----------|------|------|---------|
| `getInfo` | 获取设备信息 | 返回能力清单（MCP `tools/list`） | `ALWAYS_ALLOWED` |
| `getState` | 获取状态 | 返回当前设备状态和运行中指令 | `ALWAYS_ALLOWED` |
| `otaUpdate` | OTA 升级 | 触发远程固件升级 | `EXCLUSIVE` |
| `stop` | 停止 | 强制停止持续动作 | `FORCE` |
| `powerDown` | 关机 | 关闭设备 | `FORCE` |
| `powerUp` | 开机 | 唤醒设备 | `ALWAYS_ALLOWED` |
| `reSet` | 恢复出厂 | 重置所有配置 | `FORCE` |
| `reStart` | 重启 | 重启设备 | `FORCE` |

### 示例：`getInfo` 的响应

当 AI 智能体调用 `tools/list` 时，中控会向设备查询 `getInfo`。框架会为所有已注册指令自动生成 JSON Schema：

```json
{
  "tools": [
    {
      "name": "sync_add",
      "description": "同步加法：接收两个参数，立即返回计算结果",
      "inputSchema": {
        "type": "object",
        "properties": {
          "add": { "type": "integer", "minimum": 0, "maximum": 100, "default": 50 },
          "adder": { "type": "integer", "minimum": 0, "maximum": 200 }
        },
        "required": ["adder"]
      }
    }
  ]
}
```

---

## 🛠️ 开发你自己的指令

每个自定义模块遵循相同的三步模式：

1. **定义参数** — 用 `function_param_desc_t`
2. **实现 handler** — 同步 / 异步 / 持久化
3. **注册** — 在 `init()` 中调用 `apex_cmd_register()`

### 模式 A：同步 Handler（最常用）

适用于瞬时操作：计算、状态查询、GPIO 简单控制。

```c
#include "apex_cmd_executor.h"

static const char *CMD_KEY = "my_led";

static const function_param_desc_t params[] = {
    {.key = "state", .type = "bool", .description = "LED 开关状态"},
};

static int led_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    cJSON *state = cJSON_GetObjectItem(params, "state");
    if (!cJSON_IsBool(state)) return APEX_ERR_PARAM;

    gpio_set_level(LED_GPIO, state->valueint ? 1 : 0);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "led", state->valueint);
    *res_data = data;

    return APEX_OK;  // 框架自动发响应 + 解锁
}

void my_led_init(void)
{
    static char schema[1024];
    build_function_param_desc_json(params, 1, schema, sizeof(schema));

    apex_cmd_entry_t entry = {
        .cmd_key = CMD_KEY,
        .function_name = "LED 控制",
        .function_desc = "打开或关闭板载 LED 灯",
        .function_params = schema,
        .role = "user",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_PARALLEL,
        .handler = led_handler,
    };
    apex_cmd_register(entry);
}
```

**生命周期：**
```
注册 → 收到指令 → 锁定槽位 → handler 返回 APEX_OK
  → 框架自动发成功响应 + 解锁 → 结束
```

---

### 模式 B：异步 Handler

适用于耗时操作：OTA 升级、网络请求、传感器采样。

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct {
    int param;
    char msg_id[48];  // ⚠️ 必须保存，用于 apex_cmd_finish()
} my_ctx_t;

static void async_task(void *arg)
{
    my_ctx_t *ctx = (my_ctx_t *)arg;

    // ... 执行耗时操作 ...
    vTaskDelay(pdMS_TO_TICKS(5000));

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "status", "done");
    apex_cmd_finish(ctx->msg_id, APEX_OK, data);  // 解锁 + 发响应
    free(ctx);
    vTaskDelete(NULL);
}

static int my_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    my_ctx_t *ctx = calloc(1, sizeof(my_ctx_t));
    strlcpy(ctx->msg_id, msg_id, sizeof(ctx->msg_id));

    xTaskCreate(async_task, "async", 4096, ctx, 5, NULL);
    return APEX_ASYNC_OK;  // 框架维持锁定，发送 "processing"
}
```

**生命周期：**
```
注册 → 收到指令 → 锁定槽位 → handler 创建 FreeRTOS 任务 → 返回 APEX_ASYNC_OK
  → 框架发 "processing（处理中）"（维持锁定）
  → 后台任务完成 → 调用 apex_cmd_finish()
  → 框架发完成响应 + 解锁
```

---

### 模式 C：持久化动作（可停止）

适用于需要显式停止的连续操作：电机转动、LED 呼吸灯、音频播放。

```c
static int motor_stop(cJSON *params, const char *msg_id, cJSON **res_data)
{
    motor_hardware_stop();
    apex_cmd_finish(msg_id, APEX_OK, NULL);  // ⚠️ 停止回调中必须调用 finish
    return APEX_OK;
}

static int motor_start(cJSON *params, const char *msg_id, cJSON **res_data)
{
    int speed = 50;
    cJSON *s = cJSON_GetObjectItem(params, "speed");
    if (cJSON_IsNumber(s)) speed = s->valueint;

    motor_hardware_start(speed);
    return APEX_OK;  // 持久化指令：返回 OK 后框架维持锁定
}

void motor_init(void)
{
    apex_cmd_entry_t entry = {
        .cmd_key = "motorRun",
        .function_name = "电机控制",
        .function_desc = "启动电机持续运转，通过 stop 指令终止",
        .function_params = "{\"speed\":{\"type\":\"integer\"}}",
        .flags = APEX_CMD_FLAG_EXCLUSIVE,  // 独占模式
        .handler = motor_start,
        .is_persistent = true,              // ⚠️ 必须设为 true
        .stop_handler = motor_stop,          // ⚠️ 必须提供停止回调
    };
    apex_cmd_register(entry);
}
```

**生命周期：**
```
收到启动指令 → 锁定槽位 → handler 启动物理设备 → 返回 APEX_OK
  → 框架发成功响应（维持锁定）→ 设备持续运行
  → 收到 stop 指令 → 执行停止回调、关闭硬件
  → 调用 apex_cmd_finish() 解锁 → 结束
```

---

## 🔒 MQTT 三通道通信

| 通道 | Topic | 方向 | 格式 |
|------|-------|------|------|
| **Command** | `apex/{device_id}/command` | 中控 → 设备 | JSON |
| **Response** | `apex/{device_id}/response` | 设备 → 中控 | AES-CCM 加密 JSON |
| **Notify** | `apex/{device_id}/notify` | 设备 → 中控 | JSON-RPC 2.0 Notification（加密） |

> Command + Response 构成请求-响应对。Notify 是设备单向主动推送，适用于报警、状态变更等场景。

---

## 📂 项目结构

```
apex-esp32-s3-v6-basic/
├── main/
│   ├── main.c                    # 程序入口：初始化框架 → 注册模块
│   └── CMakeLists.txt
│
├── apex_frame/                   # 核心框架层（请勿修改）
│   ├── apex_core/                #   框架总入口 & 事件总线 & 看门狗
│   ├── apex_event/               #   全局事件循环（粘性事件）
│   ├── apex_config/              #   系统配置管理 (NVS)
│   ├── apex_network/             #   Wi-Fi AP+STA 双模管理
│   ├── apex_mqtt/                #   MQTT 客户端（三通道）
│   ├── apex_crypto/              #   AES-CCM-128 加解密
│   ├── apex_cmd_executor/        #   ⭐ 指令调度中枢
│   ├── apex_webserver/           #   Web 配置门户
│   ├── apex_ota_update/          #   OTA 固件升级
│   ├── apex_power_down/          #   关机
│   ├── apex_power_up/            #   开机
│   ├── apex_reset/               #   恢复出厂
│   ├── apex_restart/             #   重启
│   ├── apex_stop/                #   强制停止持续动作
│   ├── apex_get_state/           #   获取设备状态
│   └── apex_notify/              #   设备事件通知
│
├── common/
│   └── utils/                    # UUID、加密工具、HTTP、JSON 工具
│
├── components/                   # 你的模块放这里
│   ├── sync_add/                 #   示例：同步指令
│   └── async_add/                #   示例：异步指令
│
├── partitions.csv                # OTA 双分区布局
├── sdkconfig                     # ESP-IDF 工程配置
└── CMakeLists.txt                # 顶层 CMake
```

---

## ⚙️ 默认配置

| 配置项 | 默认值 |
|--------|--------|
| Wi-Fi STA SSID | `APEX_STA` |
| Wi-Fi STA 密码 | `apex123` |
| Wi-Fi AP SSID | `APEX-XXXX`（基于 MAC 动态生成） |
| Wi-Fi AP 密码 | `12345678` |
| MQTT Broker | `agent-plat.local:1883` |
| 设备密码 | `apex123` |
| Web 门户登录密码 | `12345678` |

所有配置可通过 Web 门户（首次配网时访问 `http://192.168.4.1`，配网后通过设备 LAN IP 访问）进行修改。

### 分区表

| 分区名 | 大小 | 类型 | 用途 |
|--------|------|------|------|
| `nvs` | 128 KB | data | 配置存储 |
| `otadata` | 8 KB | data | OTA 状态元数据 |
| `phy_init` | 4 KB | data | PHY 校准 |
| `ota_0` | 4 MB | app | 固件槽 A |
| `ota_1` | 4 MB | app | 固件槽 B |
| `storage` | 8 MB | spiffs | 文件存储 |

---

## 🧪 指令并发属性（Flags）

每条指令必须选择以下标志之一：

| 标志 | 行为 | 适用场景 |
|------|------|---------|
| `ALWAYS_ALLOWED` | 永不阻塞，任何状态可执行 | 只读查询（`getInfo`、`getState`） |
| `PARALLEL` | 可与其他指令并行 | 无状态操作（`sync_add`） |
| `EXCLUSIVE` | 锁定系统，独享执行 | 关键硬件操作（OTA、电机初始化） |
| `FORCE` | 绕过一切，紧急优先 | 急停、重启、断电保护 |

---

## ✅ 新指令上线检查清单

注册新指令前逐项核对：

- [ ] `function_param_desc_t` 数组定义规范
- [ ] 调用了 `build_function_param_desc_json()` 生成 JSON Schema
- [ ] 参数的 KEY 宏定义与 handler 中的取值一一对应
- [ ] `function_desc` 描述准确、简洁
- [ ] `role` 权限配置正确（`"admin"` / `"user"`）
- [ ] `flags` 匹配业务场景
- [ ] 若 `is_persistent = true`：已实现 `stop_handler` 并在其中调用 `apex_cmd_finish()`
- [ ] 异步任务：`msg_id` 已保存到上下文结构体，完成时传给 `apex_cmd_finish()`
- [ ] `init()` 在 `main.c` 中 **`apex_cmd_executor_init()` 之后** 调用

---

## 🧩 生态仓库

本项目是 **底层硬件固件** 层。完整技术栈：

| 项目 | 定位 | 仓库 |
|------|------|------|
| **apex_mcp_bridge** | 核心 — 智能体 ↔ 硬件网关 | [gitee.com/freen/apex-mcp-bridge](https://gitee.com/freen/apex-mcp-bridge) |
| **service-plugins** | 可扩展插件框架 | [gitee.com/freen/service-plugins](https://gitee.com/freen/service-plugins) |
| **apex-esp32-s3-v6** | 底层硬件固件（本项目） | [gitee.com/freen/apex-esp32-s3-v6](https://gitee.com/freen/apex-esp32-s3-v6) |

---

## 📄 License

[MIT](../LICENSE) © 2026 apex-freen

---

<p align="center">
  <em>写一个 handler，注册一条指令，你的 AI 就多一个技能。</em>
</p>
