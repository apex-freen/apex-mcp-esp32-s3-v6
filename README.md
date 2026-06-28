# APEX 通用智能体设备开发框架

> **`apex-esp32-s3-v6`** — 同时支持 **MQTT 原生协议** 与 **标准化 MCP (JSON-RPC 2.0)** 的智能体设备框架
> 配合中控平台 (agent-plat)，将 ESP32-S3 设备无缝包装为 MCP Server
> 只要支持 MCP 协议，一键接入，完成指令下发、中控权限校验与指令安全校验
>
> **云端智能体**：豆包 / 千问 / 元宝 / Kimi / 文心一言 / DeepSeek / ChatGPT / Claude / Gemini
> **本地客户端**：Trae / Cursor / Windsurf / 龙虾 / VS Code + Copilot / Claude Desktop

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v6.X-green.svg)](https://docs.espressif.com/projects/esp-idf/)
[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-orange.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![MCP](https://img.shields.io/badge/MCP-Standard-purple.svg)](https://modelcontextprotocol.io/)
[![Protocol](https://img.shields.io/badge/Protocol-MQTT%20%2B%20JSON--RPC%202.0-blue.svg)]()

---

## 目录

- [APEX 是什么？](#apex-是什么)
- [为什么选择 APEX？](#为什么选择-apex)
- [系统架构](#系统架构)
  - [云端智能体服务模式](#云端智能体服务模式)
  - [本地智能体服务模式](#本地智能体服务模式)
- [技术栈](#技术栈)
- [项目目录结构](#项目目录结构)
- [核心功能](#核心功能)
- [快速开始](#快速开始)
- [模块开发指南](#模块开发指南)
- [指令执行引擎详解](#指令执行引擎详解)
- [开发示例](#开发示例)
- [新指令上线检查清单](#新指令上线检查清单)
- [项目定位与扩展性](#项目定位与扩展性)

---

## APEX 是什么？

**APEX** 是一个**通用智能体设备开发框架**，旨在让硬件开发者和中小设备厂商，通过本地智能体中控系统，快速将自有硬件产品对接主流 AI App（如豆包、千问、元宝等）。

> **通俗地说**：你只需专注做好自己的硬件产品，无需开发 App、无需搭建团队、无需面对物联网平台的审核壁垒 —— 你的产品可以直接被 AI 智能体控制。

**当前项目 `apex-esp32-s3-v6`** 是 APEX 框架基于 **ESP32-S3** 芯片、**ESP-IDF v6.X** 的参考实现。项目采用模块化架构，扩展性良好，可快速调整适配 ESP32-C 系列或其他支持 IDF v6.X 的芯片平台。

---
### 官方QQ群

入群答案：`APEX智能体设备`

| 群名称 | 群号 |
|--------|------|
| 官方1群 | 882419824 |

## 为什么选择 APEX？

| 痛点 | APEX 解决方案 |
|------|--------------|
| 物联网平台审核繁琐 | **零壁垒接入**：无需平台审核，产品通过国家及销售渠道标准即可 |
| 需要自建 App 和团队 | **零 App 开发**：直接对接豆包、千问、元宝等顶流 AI App |
| 多平台适配成本高 | **一次开发，多端可用**：统一指令协议，AI App 原生支持 |
| 技术栈复杂、学习曲线陡 | **标准化框架**：事件驱动 + 模块化设计，快速上手量产 |

### 核心优势

1. **🚫 没有物联网壁垒** — 不需要接入传统物联网平台的冗长审核流程
2. **📱 无需自建 App** — 直接对接主流 AI 智能体应用，用户即用即控
3. **🔧 专注产品本身** — 开发者只需关注硬件功能实现，框架处理通信、升级、配网等全部基础设施
4. **📦 模块化可扩展** — 分层架构，业务模块独立封装，便于功能迭代与复用

---

## 系统架构

APEX 框架支持两种智能体服务模式，满足不同场景下的设备控制需求：

### 云端智能体服务模式

适用于需要远程控制、跨网络访问的场景，设备通过 MQTT 与云端智能体服务保持长连接。

```
┌────────────────────────────────────────────────────────────────────────────┐
│                           用户交互层                                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                                  │
│  │  豆包 App │  │ 千问 App │  │  元宝 App │  ← 用户通过 AI App 发送语音/文字指令 │
│  └──────────┘  └──────────┘  └──────────┘                                  │
└────────────────────────────────────────────────────────────────────────────┘
                      │← AI 理解用户意图，转换为标准化设备指令
        ┌─────────────▼─────────────┐
        │   agent-plat.com 指令转发   │
        └─────────────┬─────────────┘
                      │
        ┌─────────────▼─────────────┐
        │                           │  ← 隔离密钥与敏感信息
        │      智能体中控平台设备      │    智能体权限验证
        │                           │    指令锁 / 指令优先级验证
        └─────────────┬─────────────┘
                      │ MQTT
        ┌─────────────▼─────────────┐
        │      设备端 (ESP32-S3)     │
        │  ┌─────────────────────┐  │
        │  │   apex_cmd_executor │  │  ← 指令解密、解析、分发、执行、响应
        │  │    (指令调度中枢)     │  │
        │  └─────────────────────┘  │
        │  ┌─────────────────────┐  │
        │  │    业务模块组件       │  │  ← 电机控制、传感器读取、LED 驱动等
        │  │   (Components)      │  │
        │  └─────────────────────┘  │
        │  ┌─────────────────────┐  │
        │  │  WiFi / MQTT / OTA  │  │  ← 网络连接、远程通信、固件升级
        │  │   (Infrastructure)  │  │
        │  └─────────────────────┘  │
        └───────────────────────────┘
                      │
        ┌─────────────▼─────────────┐
        │        硬件外设            │  ← GPIO、I2C、SPI、PWM、ADC 等
        │    (Peripherals)          │
        └───────────────────────────┘
```

**流程说明**：
1. 用户在豆包/千问/元宝 App 中通过自然语言发出控制指令（如"打开客厅的灯"）
2. 云端智能体服务解析用户意图，转换为 APEX 标准指令格式，通过 MQTT 下发至设备
3. 设备端 `apex_cmd_executor` 接收加密指令，解密、解析后匹配对应业务 handler
4. 业务模块执行硬件操作，结果通过 MQTT 回传云端，App 实时反馈执行状态

---

### 本地智能体服务模式

适用于局域网内控制、隐私敏感或断网场景，设备内置 Web 服务提供本地智能体交互能力。

```

        ┌───────────────────────────┐
        │    本地智能体服务系统        │  ← 本地 Agent 引擎，意图识别，指令下发
        │   (Local Agent Service)   │
        └─────────────┬─────────────┘
                      │
        ┌─────────────▼─────────────┐
        │                           │  ← 隔离密钥与敏感信息
        │     智能体中控平台系统设备    │    智能体权限验证
        │                           │    指令锁 / 指令优先级验证
        └─────────────┬─────────────┘
                      │ MQTT
        ┌─────────────▼─────────────┐
        │      设备端 (ESP32-S3)     │
        │  ┌─────────────────────┐  │
        │  │   apex_cmd_executor │  │  ← 本地指令解析与调度（与云端共用核心）   │
        │  │    (指令调度中枢)     │  │
        │  └─────────────────────┘  │
        │  ┌─────────────────────┐  │
        │  │    业务模块组件       │  │  ← 电机控制、传感器读取、LED 驱动等     │
        │  │   (Components)      │  │
        │  └─────────────────────┘  │
        │  ┌─────────────────────┐  │
        │  │  WiFi Manager (AP)  │  │  ← AP 模式配网，或 STA 连接路由器       │
        │  └─────────────────────┘  │
        └───────────────────────────┘
                      │
        ┌─────────────▼─────────────┐
        │        硬件外设            │  ← GPIO、I2C、SPI、PWM、ADC 等
        │    (Peripherals)          │
        └───────────────────────────┘
```

**流程说明**：
1. 设备上电后默认开启 AP 模式（热点：`APEX-XXXX`），用户手机连接该热点
2. 打开豆包/千问/元宝 App，通过局域网发现功能自动识别附近 APEX 设备
3. App 与设备内置 Web Server 建立本地连接，直接下发指令（无需经过云端）
4. 设备执行指令并实时返回状态，全程数据不离开本地网络，保障隐私安全
5. 配网成功后设备自动切换至 STA 模式，同时保留本地 Web 服务能力

---

## 技术栈

| 层级 | 技术 | 说明 |
|------|------|------|
| 开发语言 | **C** | 标准 C 语言，兼容 ESP-IDF 生态 |
| 核心框架 | **ESP-IDF v6.X** | 乐鑫官方开发框架 |
| 实时系统 | **FreeRTOS** | 任务调度、异步任务管理 |
| 构建系统 | **CMake** | 模块化编译构建 |
| 核心架构 | **事件驱动 + 模块化指令调度** | 状态流转、指令调度、任务执行解耦高效 |
| 通信协议 | **MQTT over TLS** | 云端长连接通信 |
| 本地服务 | **HTTP Web Server** | 嵌入式 Web 服务，本地配置与指令下发 |
| 数据格式 | **JSON (cJSON)** | 指令参数解析、响应数据构造 |
| 安全机制 | **Crypto 加密模块** | 指令通信加密解密 |

---

## 项目目录结构

项目采用**分层模块化设计**，各目录职责独立、边界清晰，彻底实现业务解耦：

```
apex-esp32-s3-v6/
├── main/
│   └── main.c                          # 程序启动入口，统一初始化各模块
│
├── apex_frame/                         # 核心功能模块（核心层）
│   ├── apex_cmd_executor/              # 全局命令执行引擎 — 系统核心调度中心
│   ├── apex_core/                      # 框架总入口 & 事件总线 & Watchdog
│   ├── apex_event/                     # 全局事件循环
│   ├── apex_config/                    # 系统配置管理 (NVS)
│   ├── apex_network/                   # 网络管理 — AP/STA 双模式
│   ├── apex_mqtt/                      # MQTT 通信 — 三通道 (command/response/notify)
│   ├── apex_crypto/                    # 数据加解密 (AES-256)
│   ├── apex_webserver/                 # 嵌入式 Web 服务器
│   ├── apex_ota_update/                # OTA 远程固件升级
│   ├── apex_power_down/ / apex_power_up/  # 电源管理
│   ├── apex_reset/ / apex_restart/     # 设备重置 / 重启
│   ├── apex_stop/                      # 强制停止持续动作
│   ├── apex_get_state/                 # 获取设备当前状态
│   └── apex_notify/                    # 设备事件通知
│
├── common/                             # 通用基础组件（基础层）
│   └── utils/                          # 通用工具函数库
│
├── components/                         # 业务示例组件（应用层）
│   ├── sync_add/                       # 同步指令开发示例
│   └── async_add/                      # 异步指令开发示例
│
└── build/                              # 编译输出目录
```

### 目录职责说明

| 目录 | 层级 | 职责 |
|------|------|------|
| `apex_frame/` | **核心层** | 设备核心能力底座，所有基础功能独立封装为子模块 |
| `common/` | **基础层** | 全局通用工具与依赖库，为业务模块提供基础能力支撑 |
| `components/` | **应用层** | 官方标准开发示例，指导开发者快速开发自定义业务模块 |
| `main/` | **入口层** | 程序启动入口，负责模块初始化顺序管理 |

## MCP 标准化协议架构

`apex-esp32-s3-v6` 在设计之初就将 **MCP (Model Context Protocol)** 作为一等公民。

设备端通过高效的 **MQTT** 长连接与中控通信，中控平台 (agent-plat) 将其包装为标准的 **MCP Server**，对外提供 **Streamable HTTP** 端点。这意味着 **每一台 ESP32-S3 设备都是一台隐形的 MCP Server**，可以被任何 MCP 兼容的 AI 客户端直接发现和调用。

```
AI App (MCP Client)                  中控平台 (MCP Server)                设备端 (MQTT)
─────── Streamable HTTP ────────       ──── MQTT ────
tools/list ──────────→  从缓存返回 tools[]   ←─── getInfo 时上报 tools
tools/call ──────────→  转 {function_key, params}  →  执行 → 加密 response
                                           JSON-RPC 2.0 Notification  ←  apex_cmd_send_notify
```

| 概念 | MCP 标准 | APEX 实现 |
|------|---------|----------|
| 工具发现 | `tools/list` | `getInfo` → `tools` 数组 |
| 工具调用 | `tools/call` | `{function_key, msg_id, function_params}` → handler |
| 参数描述 | `inputSchema` (JSON Schema Draft-07) | `build_function_param_desc_json()` 自动生成 |
| 事件通知 | `notifications` | `apex_cmd_send_notify()` → JSON-RPC 2.0 Notification |
| 传输层 | Streamable HTTP / stdio | MQTT (设备端) → Streamable HTTP (中控端) |

> **一句话**：你的设备说 MQTT，中控帮你翻译成 MCP，AI App 看到的就是一个标准的 MCP Server。开发者只需写 handler，框架自动完成 Schema 生成、协议适配与故障恢复。

---

## 核心功能

| 功能 | 说明 |
|------|------|
| **WiFi 双模管理** | 支持 AP/STA 混合模式，具备自动网络切换策略 |
| | • AP+STA 联网成功 → 5 分钟后自动关闭 AP，仅保留 STA |
| | • STA 断网离线 → 1 分钟后自动开启 AP+STA 双模，支持本地配网 |
| **Web 本地服务** | 内置 HTTP 网页服务，支持本地 IP 访问设备配置与管理 |
| **MQTT 远程通信** | 依托 MQTT 协议实现设备与云端长连接，支持指令下发、状态上报 |
| **设备事件通知** | 设备端主动推送事件到服务端（JSON-RPC 2.0 Notification），覆盖报警/状态变更/任务完成 |
| **OTA 远程升级** | 支持无拆机远程固件升级，实现设备在线迭代 |
| **设备电源管控** | 支持开机、关机、重启、状态复位等设备基础管控能力 |
| **事件驱动调度** | 全业务基于事件驱动机制运行，状态流转、指令调度、任务执行解耦高效 |

### MQTT 三通道通信

设备与服务端之间通过 MQTT 维持三条独立 Topic，分别承载不同的通信语义：

| 通道 | Topic 模式 | 方向 | 语义 | 协议格式 |
|------|-----------|------|------|----------|
| **Command** | `apex/{device_id}/command` | 服务端 → 设备 | 远程指令下发 | JSON |
| **Response** | `apex/{device_id}/response` | 设备 → 服务端 | 指令执行结果的**同步响应** | JSON（加密） |
| **Notify** | `apex/{device_id}/notify` | 设备 → 服务端 | 设备端**主动事件通知** | JSON-RPC 2.0 Notification（加密） |

> Command 和 Response 构成请求-响应对（一问一答）；Notify 是设备独立触发的单向推送（无回复），适合报警、状态变更等场景。

### Notify 使用示例

见 [apex_frame/apex_notify/](apex_frame/apex_notify/) — 演示如何通过 `apex_cmd_send_notify()` 推送 JSON-RPC 2.0 通知。

---

## 快速开始

### 环境准备

1. 安装 [ESP-IDF v6.X](https://docs.espressif.com/projects/esp-idf/)
2. 克隆本项目：
   ```bash
   git clone https://github.com/your-org/apex-esp32-s3-v6.git
   cd apex-esp32-s3-v6
   ```
3. 设置目标芯片：
   ```bash
   idf.py set-target esp32s3
   ```

### 编译与烧录

```bash
idf.py build
idf.py flash
idf.py monitor
```

### 首次配网

1. 设备上电后自动开启 AP 热点（名称：`APEX-XXXX`）
2. 手机连接该热点，访问 `http://192.168.4.1` ，初始密码：`12345678`
3. 在 Web 页面配置 WiFi 名称和密码
4. 设备自动切换 STA 模式并连接路由器，5 分钟后 AP 自动关闭

---

## 模块开发指南

### 文件结构约定

每个业务模块独立建目录，统一文件结构：

```
apex_frame/apex_my_feature/
├── CMakeLists.txt              # 模块构建配置
├── include/
│   └── apex_my_feature.h       # 对外公开结构体、初始化函数声明
└── apex_my_feature.c           # 业务 handler 实现、模块注册入口
```

**CMakeLists.txt 示例**：
```cmake
idf_component_register(
    SRCS "apex_my_feature.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES freertos apex_config apex_cmd_executor
)
```

### 三种 Handler 运行模式

APEX 框架根据 handler 返回值，将指令处理逻辑分为三类：

```
                    ┌─────────────────────┐
                    │  entry->handler()   │
                    └──────────┬──────────┘
                               │
              ┌────────────────┼─────────────────┐
              ▼                ▼                  ▼
             APEX_OK           APEX_ASYNC_OK       负数错误码
              │                 │                  │
      ┌───────┴───────┐         │           ┌──────┴──────┐
      │               │         │           │ 框架自动：    │
      ▼               ▼         │           │ 发错误响应    │
  is_persistent   is_persistent │           │ + unlock    │
  = false         = true        │           └─────────────┘
  │               │             │
  ▼               ▼             ▼
框架自动：      框架：          框架：
发结果响应      发"成功"        发"processing"
+ unlock       保持锁定        保持锁定
               (等待stop)    (需主动调用 apex_cmd_finish)
```

---

## 指令执行引擎详解

`apex_cmd_executor` 是整个项目的**指令调度核心、业务执行中枢**。所有云端/本地指令均通过该模块完成**解密 → 解析 → 分发 → 执行 → 响应**全流程。

### 完整指令处理流程

```
┌─────────────────┐
│  指令接收与解密   │ ← apex_process_incoming_cmd 接收 MQTT/HTTP 加密指令
│  decrypt_and_split│ ← 完成指令解密、数据拆分
└────────┬────────┘
         ▼
┌─────────────────┐
│   重复指令拦截    │ ← 同一 msg_id 在 2 秒窗口内重复到达直接拒绝 (APEX_ERR_DUPLICATE)
└────────┬────────┘
         ▼
┌─────────────────┐
│    指令解析      │ ← apex_cmd_executor 解析 JSON 指令报文
│                 │ ← 提取 function_key、msg_id、function_params
└────────┬────────┘
         ▼
┌─────────────────┐
│   指令匹配查找    │ ← apex_cmd_find_entry 匹配全局指令注册表
│                 │ ← 无匹配则返回 APEX_ERR_NOT_FOUND + notify
└────────┬────────┘
         ▼
┌─────────────────┐
│ 设备状态与槽位管理 │ ← apex_state_lock 锁定设备运行状态
│                 │ ← 校验待机状态、空闲指令槽位，防止冲突
└────────┬────────┘
         ▼
┌─────────────────┐
│   熔断降级检查    │ ← 连续失败 5 次自动熔断 5 分钟 (APEX_ERR_DEGRADED)
│                 │ ← 成功立即清零失败计数
└────────┬────────┘
         ▼
┌─────────────────┐
│  多模式指令执行   │ ← 调用对应业务 handler 执行逻辑
│                 │ ← 根据返回值区分：同步 / 异步 / 持久化任务
│                 │ ← 失败时自动 notify 服务端 (event: command_failed)
└────────┬────────┘
         ▼
┌─────────────────┐
│   分级响应推送    │ ← 同步：自动推送成功/错误响应并解锁
│                 │ ← 异步：先推送 processing，完成后推送 completed
│                 │ ← 所有响应统一加密后回传
└─────────────────┘
```

### 内置防护机制

| 机制 | 触发条件 | 行为 | 通知服务端 |
|------|---------|------|:---:|
| **指令去重** | 同一 `msg_id` 2 秒内重复到达 | 返回 `APEX_ERR_DUPLICATE`，不执行 | — |
| **故障计数** | handler 返回负值错误码 | 累计连续失败次数 | ✅ `command_failed` |
| **熔断降级** | 连续失败 ≥ 5 次 | 该指令 5 分钟内直接返回 `APEX_ERR_DEGRADED` | ✅ `degraded` |
| **降级恢复** | 5 分钟到期 | 自动清零失败计数，恢复正常 | — |
| **看门狗** | handler 执行超过 30 秒 | 设备自动复位（ESP-IDF 原生 IWDT 兜底） | — |

### 核心数据结构

| 结构体 | 说明 |
|--------|------|
| `apex_cmd_entry_t` | 指令条目结构体，存储单条指令的标识、名称、描述、参数规范、权限、运行标识、处理函数等全量配置 |
| `apex_state_manager_t` | 设备状态管理器，全局跟踪设备运行状态、当前执行指令、槽位占用情况 |
| `s_cmd_table` | 全局指令注册表，存储所有已注册的业务指令，为指令匹配提供数据源 |

`apex_cmd_entry_t` 完整字段一览：

| 字段 | 类型 | 必填 | 说明 |
|------|------|:---:|------|
| `cmd_key` | `const char *` | ✅ | 指令标识（如 `"sync_add"`） |
| `function_name` | `const char *` | ✅ | 中文展示名 |
| `function_desc` | `const char *` | ✅ | 功能描述（无参时建议备注"无需参数"） |
| `function_params` | `const char *` | ✅ | JSON Schema 字符串（由 `build_function_param_desc_json()` 生成） |
| `role` | `const char *` | ✅ | 权限角色：`"admin"` / `"user"` |
| `version` | `const char *` | ✅ | 版本号 |
| `flags` | `apex_cmd_flag_t` | ✅ | 并发属性：`PARALLEL` / `EXCLUSIVE` / `FORCE` / `ALWAYS_ALLOWED` |
| `handler` | `apex_cmd_handler_t` | ✅ | 指令执行函数 |
| `is_persistent` | `bool` | | 持久化动作标记（需配套 `stop_handler`） |
| `stop_handler` | `apex_stop_handler_t` | | 停止回调（`is_persistent=true` 时必填） |
| `notify_handler` | `apex_notify_handler_t` | | 设备事件通知回调（可选），在 handler 中主动调用 `apex_cmd_send_notify()` |

#### 参数描述结构体 `function_param_desc_t`

定义每个指令参数的元信息，由 `build_function_param_desc_json()` 转换为 **JSON Schema Draft-07** 格式（兼容 MCP `inputSchema`）：

```
源码定义                    输出 JSON Schema
────────────────────────────────────────────────
.key   = "add"          →  "add": { ... }
.type   = "int"          →  "type": "integer"
.type   = "float"        →  "type": "number"
.type   = "bool"         →  "type": "boolean"
.has_min / .min_val      →  "minimum": 0
.has_max / .max_val      →  "maximum": 100
.has_multipleOf / .multipleOf_val →  "multipleOf": 1
.enum_vals               →  "enum": ["cool","heat"]
.description             →  "description": "..."
.unit                    →  合并到 description  (不输出独立字段)
                             例: "第一个加数（单位：celsius）"
.has_default=0           →  列入 "required" 数组 (必填)
```

> `.unit` 字段保留在源码中作为结构化数据，构建 JSON 时自动拼接到 `description` 后面（`"xxx（单位：yyy）"`），不输出独立的 `"unit"` 键，确保输出为纯 JSON Schema 标准格式。

---

## 开发示例

### 模式 A：同步计算（最常用）

适用场景：可瞬间完成的参数查询、简单计算、状态设置等轻量业务。

> 对应源码：[components/sync_add/sync_add.c](components/sync_add/sync_add.c)

```c
#define TAG "SYNC_ADD_LOG"
#include "esp_log.h"
#include "apex_cmd_executor.h"
#include "sync_add.h"

// ============================================================================
// 1. 常量与参数定义区
// ============================================================================
static const char *FUNCTION_KEY = "sync_add";

#define KEY_PARAM_A "add"
#define KEY_PARAM_B "adder"
static const function_param_desc_t function_params[] = {
    {.key = KEY_PARAM_A, .type = "int", .description = "第一个加数",
     .has_min = 1, .min_val = 0,
     .has_max = 1, .max_val = 100, .has_multipleOf = 1, .multipleOf_val = 1,
     .unit = "celsius", .has_default = 1, .default_val = 50},
    {.key = KEY_PARAM_B, .type = "int", .description = "第二个加数",
     .has_min = 1, .min_val = 0, .has_max = 1, .max_val = 200},
};

// ============================================================================
// 2. 同步 Handler（直接在当前线程执行，不创建新任务）
// ============================================================================
static int sync_add_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    ESP_LOGI(TAG, "收到同步加法请求 [ID: %s]", msg_id);

    // 步骤 1: 参数校验
    if (params == NULL || !cJSON_IsObject(params)) {
        ESP_LOGW(TAG, "参数格式错误，期望一个 JSON Object");
        return APEX_ERR_PARAM;   // 框架自动返回错误响应并解锁
    }
    cJSON *item_a = cJSON_GetObjectItem(params, KEY_PARAM_A);
    cJSON *item_b = cJSON_GetObjectItem(params, KEY_PARAM_B);
    if (!cJSON_IsNumber(item_a) || !cJSON_IsNumber(item_b)) {
        ESP_LOGW(TAG, "参数缺失或类型错误: 需要 %s(int) 和 %s(int)", KEY_PARAM_A, KEY_PARAM_B);
        return APEX_ERR_PARAM;
    }

    // 步骤 2: 执行业务逻辑
    int sum = item_a->valueint + item_b->valueint;

    // 步骤 3: 构造结果，框架自动包装并加密发送
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "result", sum);
    *res_data = data;

    return APEX_OK;   // 同步完成，框架自动发结果 + 解锁
}

// ============================================================================
// 3. 组件注册入口
// ============================================================================
void sync_add_init(void)
{
    static char function_params_json_buf[1024];
    int count = sizeof(function_params) / sizeof(function_params[0]);
    build_function_param_desc_json(function_params, count, function_params_json_buf, sizeof(function_params_json_buf));

    apex_cmd_entry_t entry = {
        .cmd_key         = FUNCTION_KEY,
        .function_name   = "同步加法计算",
        .function_desc   = "同步加法：接收两个参数，立即返回计算结果",
        .function_params = function_params_json_buf,
        .role            = "user",
        .version         = "1.0.0",
        .flags           = APEX_CMD_FLAG_PARALLEL,   // 并行指令，不锁定系统
        .handler         = sync_add_handler,
        .is_persistent   = false,                     // 非持久化动作
        .stop_handler    = NULL,
    };
    apex_cmd_register(entry);
    ESP_LOGI(TAG, "组件注册成功: %s (v%s)", entry.cmd_key, entry.version);
}
```

**指令生命周期**：
```
组件注册 → 接收指令 → 锁定槽位 → 执行 sync_add_handler
    → 返回 APEX_OK → 框架自动推送成功响应 {"code":0,"result":{...}}
    → 自动解锁槽位，指令结束
```

---

### 模式 B：异步任务处理

适用场景：耗时较长的业务操作，如 OTA 升级、长时间运算、网络请求等。

> 对应源码：[components/async_add/async_add.c](components/async_add/async_add.c)

```c
#define TAG "ASYNC_ADD_LOG"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include "apex_cmd_executor.h"
#include "async_add.h"

// ============================================================================
// 1. 常量与参数定义区
// ============================================================================
static const char *FUNCTION_KEY = "async_add";

#define KEY_PARAM_A "add"
#define KEY_PARAM_B "adder"
static const function_param_desc_t function_params[] = {
    {.key = KEY_PARAM_A, .type = "int", .description = "第一个加数",
     .has_min = 1, .min_val = 0,
     .has_max = 1, .max_val = 100, .has_multipleOf = 1, .multipleOf_val = 1,
     .unit = "celsius", .has_default = 1, .default_val = 50},
    {.key = KEY_PARAM_B, .type = "int", .description = "第二个加数",
     .has_min = 1, .min_val = 0, .has_max = 1, .max_val = 200},
};

// ============================================================================
// 2. 异步任务上下文结构体
// ============================================================================
typedef struct {
    int a, b;
    char msg_id[48]; // 【必须】保存原始请求的唯一标识，用于异步回调
} add_async_ctx_t;

// ============================================================================
// 3. 异步后台任务（真正的耗时逻辑在这里）
// ============================================================================
static void add_async_task(void *pvParameters)
{
    add_async_ctx_t *ctx = (add_async_ctx_t *)pvParameters;
    ESP_LOGI(TAG, "异步计算开始: %d + %d (ID: %s)", ctx->a, ctx->b, ctx->msg_id);

    // 模拟耗时操作（读取传感器、等待外设、网络请求等）
    vTaskDelay(pdMS_TO_TICKS(100000)); // 100秒后才返回，用于测试异步中间状态
    int result_value = ctx->a + ctx->b;

    // 回传最终结果并释放状态机
    cJSON *res_data = cJSON_CreateObject();
    cJSON_AddNumberToObject(res_data, "result", result_value);
    apex_cmd_finish(ctx->msg_id, APEX_OK, res_data); // 自动发响应 + 解锁槽位
    free(ctx);
    vTaskDelete(NULL);
}

// ============================================================================
// 4. Handler（运行在 Executor 线程上下文）
// ============================================================================
static int apex_cmd_add_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    ESP_LOGI(TAG, "收到 %s 指令，准备解析参数", FUNCTION_KEY);

    if (params == NULL || !cJSON_IsObject(params)) {
        ESP_LOGW(TAG, "参数格式错误，期望一个 JSON Object");
        return APEX_ERR_PARAM;
    }
    cJSON *item_a = cJSON_GetObjectItem(params, KEY_PARAM_A);
    cJSON *item_b = cJSON_GetObjectItem(params, KEY_PARAM_B);
    if (!cJSON_IsNumber(item_a) || !cJSON_IsNumber(item_b)) {
        ESP_LOGW(TAG, "参数缺失或类型错误: 需要 %s(int) 和 %s(int)", KEY_PARAM_A, KEY_PARAM_B);
        return APEX_ERR_PARAM;
    }

    // 分配并填充异步上下文
    add_async_ctx_t *ctx = calloc(1, sizeof(add_async_ctx_t));
    if (!ctx) { ESP_LOGE(TAG, "内存分配失败"); return APEX_ERR_SYS; }
    ctx->a = item_a->valueint;
    ctx->b = item_b->valueint;
    if (msg_id != NULL)
        strlcpy(ctx->msg_id, msg_id, sizeof(ctx->msg_id));

    // 创建 FreeRTOS 异步任务
    BaseType_t ret = xTaskCreate(add_async_task, "task_add", 4096, ctx, 5, NULL);
    if (ret != pdPASS) { free(ctx); return APEX_ERR_SYS; }

    return APEX_ASYNC_OK;   // 框架维持锁定，等待异步回调
}

// ============================================================================
// 5. 组件注册入口
// ============================================================================
void async_add_init(void)
{
    static char function_params_json_buf[1024];
    int count = sizeof(function_params) / sizeof(function_params[0]);
    build_function_param_desc_json(function_params, count, function_params_json_buf, sizeof(function_params_json_buf));

    apex_cmd_entry_t entry = {
        .cmd_key         = FUNCTION_KEY,
        .function_name   = "异步加法计算",
        .function_desc   = "模拟耗时操作：接收两个参数，延迟100秒后返回加法结果",
        .function_params = function_params_json_buf,
        .role            = "user",
        .version         = "1.0.1",
        .handler         = apex_cmd_add_handler,
    };
    apex_cmd_register(entry);
    ESP_LOGI(TAG, "组件注册成功: %s (v%s)", entry.cmd_key, entry.version);
}
```

**指令生命周期**：
```
组件注册 → 接收指令 → 锁定槽位 → handler 创建异步任务 → 返回 APEX_ASYNC_OK
    → 框架推送 processing 响应（不解锁）→ 后台任务执行业务
    → 调用 apex_cmd_finish → 推送 completed 响应 + 解锁槽位，任务结束
```

---

### 模式 C：持久化动作（可停止）

适用场景：需要持续运行、依赖外部 stop 指令终止的业务，如电机转动、LED 呼吸灯、音频播放等。

```c
#define TAG "MOTOR_CTRL"
#include "esp_log.h"
#include "apex_cmd_executor.h"
#include "cJSON.h"

static int motor_stop_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    ESP_LOGI(TAG, "收到电机停止指令，执行停止逻辑");

    motor_hardware_stop();

    // 核心：主动收尾解锁，释放系统槽位
    apex_cmd_finish(msg_id, APEX_OK, NULL);

    return APEX_OK;
}

static int motor_start_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    int speed = 0;
    cJSON *speed_item = cJSON_GetObjectItem(params, "speed");
    if (cJSON_IsNumber(speed_item)) {
        speed = speed_item->valueint;
    }

    motor_hardware_start(speed);

    // 持久化指令返回 OK，框架维持锁定，等待 stop 指令
    return APEX_OK;
}

void motor_module_init(void)
{
    apex_cmd_entry_t entry = {
        .cmd_key         = "motorRun",
        .function_name   = "电机控制",
        .function_desc   = "启动电机持续运转，需通过 stop 指令终止",
        .function_params = "{\"speed\":\"int\",\"direction\":\"string\"}",
        .role            = "user",
        .version         = "1.0.0",
        .flags           = APEX_CMD_FLAG_EXCLUSIVE,  // 独占模式
        .handler         = motor_start_handler,
        .is_persistent   = true,                      // 开启持久化模式
        .stop_handler    = motor_stop_handler,        // 绑定停止回调
    };
    apex_cmd_register(entry);
    ESP_LOGI(TAG, "电机控制模块注册成功");
}
```

**指令生命周期**：
```
接收启动指令 → 锁定槽位 → handler 启动硬件设备 → 返回 APEX_OK
    → 框架推送成功响应（维持锁定）→ 设备持续运行
    → 接收 stop 指令 → 执行停止回调、关闭硬件
    → 调用 apex_cmd_finish 解锁，流程结束
```

---

## 新指令上线检查清单

新增模块/指令后，注册上线前逐项核对：

- [ ] `function_param_desc_t` 数组定义规范，调用 `build_function_param_desc_json()` 生成 JSON Schema
- [ ] KEY 宏定义与 `function_params` 中的 `.key` 字段一一对应
- [ ] `function_desc` 描述准确、简洁，清晰说明指令功能
- [ ] `role` 权限配置正确（`admin` / `user`）
- [ ] `flags` 匹配业务场景（`PARALLEL` / `EXCLUSIVE` / `FORCE` / `ALWAYS_ALLOWED`）
- [ ] `is_persistent` 配置与实际业务行为一致
- [ ] 若 `is_persistent=true`，已完整实现 `stop_handler` 停止回调
- [ ] 异步任务已正确保存全局唯一 `msg_id`
- [ ] 异步任务结束前，必调用 `apex_cmd_finish` 完成收尾解锁
- [ ] 模块 `init` 函数调用时机正确（在 `apex_cmd_executor_init()` 之后执行）

### Flags 配置决策规则

| 场景 | 配置 |
|------|------|
| 只读查询、不修改设备状态 | `APEX_CMD_FLAG_ALWAYS_ALLOWED` |
| 普通计算/设置、无资源冲突 | `APEX_CMD_FLAG_PARALLEL` |
| 独占操作、禁止干扰（OTA、硬件初始化） | `APEX_CMD_FLAG_EXCLUSIVE` |
| 紧急操作（急停、重启、断电保护） | `APEX_CMD_FLAG_FORCE` |

---

## 项目定位与扩展性

### 当前项目定位

`apex-esp32-s3-v6` 是 **APEX 通用智能体设备开发框架** 的参考实现之一，专为以下场景设计：

- **智能家居终端** — 灯具、插座、窗帘电机、环境传感器等
- **无线控制设备** — 遥控器、门禁、报警器等
- **远程监控设备** — 摄像头伴侣设备、数据采集终端等

### 框架扩展性

APEX 框架设计为**芯片无关、平台无关**的通用架构：

| 当前实现 | 可扩展目标 |
|----------|-----------|
| ESP32-S3 + IDF v6.X | ESP32-C3 / C6 / H2 / P4 等全系列芯片 |
| WiFi / MQTT | 增加 BLE、Zigbee、Thread、LoRa 等协议支持 |
| MCP 协议对接 | 千问、元宝、ChatGPT、Claude 等更多 AI 平台通过 MCP 协议接入 |

### 标准开发流程

开发者新增自定义业务模块/指令，遵循以下统一标准流程：

1. 在 `components/` 或 `apex_frame/` 目录下新建独立模块目录
2. 参照官方同步/异步示例，实现业务 handler 处理逻辑
3. 编写模块初始化函数，完成指令注册至全局指令表
4. 在 `main.c` 中完成模块初始化调用（需在 `apex_cmd_executor_init` 之后执行）
5. 依据上线检查清单校验规范，编译测试上线

---

## 已落地能力

✅ **MCP 智能体控制** — 支持标准 MCP (Model Context Protocol) 协议，可对接各类兼容 MCP 的智能体（如豆包 App、Claude Desktop 等），实现设备远程控制

✅ **豆包 App 智能体控制** — 已完整实现对接，用户可通过豆包 App 直接控制 APEX 设备

---

## License

[MIT](LICENSE)

---

> **APEX** — 让每一款硬件产品，都能被 AI 智能体轻松控制。
