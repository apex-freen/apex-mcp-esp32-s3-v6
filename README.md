# APEX 通用智能体设备开发框架

> **本项目：`apex-esp32-s3-v6`** —— APEX 框架的 ESP32-S3 + IDF v6.X 参考实现

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v6.X-green.svg)](https://docs.espressif.com/projects/esp-idf/)
[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-orange.svg)](https://www.espressif.com/en/products/socs/esp32-s3)

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
┌─────────────────────────────────────────────────────────────────────────┐
│                           用户交互层                                       │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                                │
│  │  豆包 App │  │ 千问 App │  │ 元宝 App │  ← 用户通过 AI App 发送语音/文字指令 │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘                                │
└───────┼─────────────┼─────────────┼───────────────────────────────────────┘
        │             │             │
        └─────────────┴─────────────┘
                      │
        ┌─────────────▼─────────────┐
        │      云端智能体服务         │  ← AI 理解用户意图，转换为标准化设备指令
        │   (Agent Cloud Service)   │
        └─────────────┬─────────────┘
                      │ MQTT over TLS
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
┌─────────────────────────────────────────────────────────────────────────┐
│                           用户交互层                                       │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                                │
│  │  豆包 App │  │ 千问 App │  │ 元宝 App │  ← 用户通过 AI App 发现本地设备   │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘                                │
└───────┼─────────────┼─────────────┼───────────────────────────────────────┘
        │             │             │
        └─────────────┴─────────────┘
                      │
        ┌─────────────▼─────────────┐
        │      本地局域网 (WiFi)      │  ← 设备 AP/STA 模式，App 直连设备热点    │
        │    (Local Network)        │     或同一局域网内自动发现               │
        └─────────────┬─────────────┘
                      │ HTTP / WebSocket
        ┌─────────────▼─────────────┐
        │      设备端 (ESP32-S3)     │
        │  ┌─────────────────────┐  │
        │  │    Web Server       │  │  ← 本地 HTTP 服务 (默认 192.168.4.1)   │
        │  │   (本地智能体入口)    │  │     提供设备配置、指令下发、状态查询     │
        │  └─────────────────────┘  │
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
│   │   ├── include/
│   │   │   └── apex_cmd_executor.h
│   │   ├── apex_cmd_executor.c
│   │   └── CMakeLists.txt
│   ├── apex_wifi_mgr/                  # WiFi 管理模块 — AP/STA 双模式自适应
│   ├── apex_mqtt/                      # MQTT 通信模块 — 设备与云端数据交互
│   ├── apex_ota_update/                # OTA 远程升级模块 — 固件远程更新
│   ├── apex_power/                     # 电源管理模块 — 开机、关机、电源状态管控
│   ├── apex_reset/                     # 设备重置、重启管理模块
│   └── apex_web_server/                # 嵌入式 Web 服务器 — 默认 http://192.168.4.1
│
├── common/                             # 通用基础组件（基础层）
│   ├── cjson/                          # JSON 解析与封装库
│   ├── crypto/                         # 数据加密解密模块
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

---

## 核心功能

| 功能 | 说明 |
|------|------|
| **WiFi 双模管理** | 支持 AP/STA 混合模式，具备自动网络切换策略 |
| | • AP+STA 联网成功 → 5 分钟后自动关闭 AP，仅保留 STA |
| | • STA 断网离线 → 1 分钟后自动开启 AP+STA 双模，支持本地配网 |
| **Web 本地服务** | 内置 HTTP 网页服务，支持本地 IP 访问设备配置与管理 |
| **MQTT 远程通信** | 依托 MQTT 协议实现设备与云端长连接，支持指令下发、状态上报 |
| **OTA 远程升级** | 支持无拆机远程固件升级，实现设备在线迭代 |
| **设备电源管控** | 支持开机、关机、重启、状态复位等设备基础管控能力 |
| **事件驱动调度** | 全业务基于事件驱动机制运行，状态流转、指令调度、任务执行解耦高效 |

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
2. 手机连接该热点，访问 `http://192.168.4.1`
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
              │                │                  │
      ┌───────┴───────┐        │           ┌──────┴──────┐
      │               │        │           │ 框架自动：   │
      ▼               ▼        │           │ 发错误响应   │
  is_persistent   is_persistent │           │ + unlock    │
  = false         = true      │           └─────────────┘
  │               │        │
  ▼               ▼        ▼
框架自动：      框架：      框架：
发结果响应      发"成功"    发"processing"
+ unlock       保持锁定     保持锁定
               (等待stop)   (需主动调用 apex_cmd_finish)
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
│    指令解析      │ ← apex_cmd_executor 解析 JSON 指令报文
│                 │ ← 提取 function_key、msg_id、function_params
└────────┬────────┘
         ▼
┌─────────────────┐
│   指令匹配查找    │ ← apex_cmd_find_entry 匹配全局指令注册表
│                 │ ← 无匹配则返回 APEX_ERR_NOT_FOUND
└────────┬────────┘
         ▼
┌─────────────────┐
│ 设备状态与槽位管理 │ ← apex_state_lock 锁定设备运行状态
│                 │ ← 校验待机状态、空闲指令槽位，防止冲突
└────────┬────────┘
         ▼
┌─────────────────┐
│  多模式指令执行   │ ← 调用对应业务 handler 执行逻辑
│                 │ ← 根据返回值区分：同步 / 异步 / 持久化任务
└────────┬────────┘
         ▼
┌─────────────────┐
│   分级响应推送    │ ← 同步：自动推送成功/错误响应并解锁
│                 │ ← 异步：先推送 processing，完成后推送 completed
│                 │ ← 所有响应统一加密后回传
└─────────────────┘
```

### 核心数据结构

| 结构体 | 说明 |
|--------|------|
| `apex_cmd_entry_t` | 指令条目结构体，存储单条指令的标识、名称、描述、参数规范、权限、运行标识、处理函数等全量配置 |
| `apex_state_manager_t` | 设备状态管理器，全局跟踪设备运行状态、当前执行指令、槽位占用情况 |
| `s_cmd_table` | 全局指令注册表，存储所有已注册的业务指令，为指令匹配提供数据源 |

---

## 开发示例

### 模式 A：同步计算（最常用）

适用场景：可瞬间完成的参数查询、简单计算、状态设置等轻量业务。

```c
#define TAG "MY_FEATURE"
#include "esp_log.h"
#include "apex_cmd_executor.h"
#include "apex_my_feature.h"
#include "cJSON.h"

#define FUNCTION_KEY  "readSensor"
#define KEY_PARAM_A   "sensor_id"
#define PARAM_SCHEMA  "{\"" KEY_PARAM_A "\":\"string\"}"

static int read_sensor_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    // 步骤 1: 参数合法性校验
    cJSON *item = cJSON_GetObjectItem(params, KEY_PARAM_A);
    if (!cJSON_IsString(item)) {
        ESP_LOGW(TAG, "缺少参数或参数类型错误: %s", KEY_PARAM_A);
        return APEX_ERR_PARAM;   // 框架自动返回错误响应并解锁
    }

    // 步骤 2: 执行业务逻辑
    float value = read_sensor_by_id(item->valuestring);

    // 步骤 3: 构造业务返回数据
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "temperature", value);

    // 步骤 4: 回传结果数据
    *res_data = data;

    // 步骤 5: 返回同步成功状态
    // 框架自动推送成功响应 + 自动解锁指令槽位
    return APEX_OK;
}

void apex_my_feature_init(void)
{
    apex_cmd_entry_t entry = {
        .cmd_key         = FUNCTION_KEY,
        .function_name   = "读取传感器",
        .function_desc   = "根据 sensor_id 读取设备温度值",
        .function_params = PARAM_SCHEMA,
        .role            = "user",
        .version         = "1.0.0",
        .flags           = APEX_CMD_FLAG_PARALLEL,   // 支持并行执行
        .handler         = read_sensor_handler,
        .is_persistent   = false,                     // 非持久化指令
        .stop_handler    = NULL,
    };
    apex_cmd_register(entry);
    ESP_LOGI(TAG, "模块注册成功: %s", FUNCTION_KEY);
}
```

**指令生命周期**：
```
模块注册指令 → 接收指令 → 锁定指令状态 → 执行同步 handler
    → 返回 APEX_OK → 框架自动推送成功响应 {"code":0,"result":{...}}
    → 自动解锁槽位，指令执行结束
```

---

### 模式 B：异步任务处理

适用场景：耗时较长的业务操作，如 OTA 升级、长时间运算、网络请求等。

```c
#define TAG "ASYNC_DEMO"
#include "esp_log.h"
#include "apex_cmd_executor.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct {
    char msg_id[64];
    char param[128];
} my_async_ctx_t;

static void my_async_task(void *arg)
{
    my_async_ctx_t *ctx = (my_async_ctx_t *)arg;

    // 模拟耗时业务操作
    vTaskDelay(pdMS_TO_TICKS(5000));
    int result = do_complex_work(ctx->param);

    // 构造返回结果
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "output", result);

    // 统一收尾：推送完成响应 + 解锁指令槽位
    apex_cmd_finish(ctx->msg_id, APEX_OK, data);

    free(ctx);
    vTaskDelete(NULL);
}

static int my_async_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    my_async_ctx_t *ctx = calloc(1, sizeof(my_async_ctx_t));
    if (!ctx) {
        ESP_LOGE(TAG, "异步上下文内存分配失败");
        return APEX_ERR_SYS;
    }

    strlcpy(ctx->msg_id, msg_id, sizeof(ctx->msg_id));
    strlcpy(ctx->param, "demo_param", sizeof(ctx->param));

    BaseType_t ret = xTaskCreate(my_async_task, "my_async_task", 4096, ctx, 5, NULL);
    if (ret != pdPASS) {
        free(ctx);
        ESP_LOGE(TAG, "异步任务创建失败");
        return APEX_ERR_SYS;
    }

    // 返回异步就绪状态，框架维持锁定
    return APEX_ASYNC_OK;
}
```

**指令生命周期**：
```
接收指令 → 锁定槽位 → handler 创建异步任务 → 返回 APEX_ASYNC_OK
    → 框架推送 processing 响应（不解锁）→ 后台任务执行业务逻辑
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

- [ ] `PARAM_SCHEMA` 定义规范（格式为 `{"key":"type"}`，无参数时填空对象 `{}`）
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
| 消费级物联网芯片 | 工业级 MCU、ARM Cortex-M 系列等 |
| WiFi 通信 | 增加 BLE、Zigbee、Thread、LoRa 等协议支持 |
| 豆包 App 对接 | 千问、元宝、ChatGPT、Claude 等更多 AI 平台 |

### 标准开发流程

开发者新增自定义业务模块/指令，遵循以下统一标准流程：

1. 在 `components/` 或 `apex_frame/` 目录下新建独立模块目录
2. 参照官方同步/异步示例，实现业务 handler 处理逻辑
3. 编写模块初始化函数，完成指令注册至全局指令表
4. 在 `main.c` 中完成模块初始化调用（需在 `apex_cmd_executor_init` 之后执行）
5. 依据上线检查清单校验规范，编译测试上线

---

## 已落地能力

✅ **豆包 App 智能体控制** — 已完整实现对接，用户可通过豆包 App 直接控制 APEX 设备

---

## License

[MIT](LICENSE)

---

> **APEX** — 让每一款硬件产品，都能被 AI 智能体轻松控制。
