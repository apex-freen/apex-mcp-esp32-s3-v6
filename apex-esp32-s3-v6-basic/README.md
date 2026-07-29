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
  <em>The reference ESP32-S3 firmware for the APEX Universal Agent Device Framework.<br>Write a handler, expose a tool. Your AI agents get a new skill.</em>
</p>

<p align="center">
  <a href="#-features">Features</a> ·
  <a href="#-architecture">Architecture</a> ·
  <a href="#-quick-start">Quick Start</a> ·
  <a href="#-built-in-commands">Commands</a> ·
  <a href="#-writing-your-own-commands">Dev Guide</a> ·
  <a href="./README_ZH.md">中文文档</a>
</p>

---

## 📖 What is this?

`apex-esp32-s3-v6-basic` is the **reference implementation** of the APEX device firmware framework for **ESP32-S3**, built against the **latest MCP protocol specification (July 2026)**. It is part of the [apex_mcp_bridge](https://github.com/apex-freen/apex-mcp-bridge) ecosystem — the hardware-side firmware layer, working with [service-plugins](https://github.com/apex-freen/service-plugins) to form the full AI-to-hardware stack.

This project gives you a **complete, production-ready firmware base** that:

- Connects to the apex_mcp_bridge via **encrypted MQTT**
- Exposes device capabilities as **MCP (Model Context Protocol) tools**
- Provides an **event-driven command execution engine** with built-in safety mechanisms
- Ships with **sync and async command examples** as a starting point for your own modules

> **The promise:** copy this project, write your hardware handler, register it — and your ESP32 device is instantly controllable by ChatGPT, Claude, Gemini, Doubao, and any MCP-compatible AI agent. **Even better: you don't have to write the handler yourself.** The framework's standardized templates and constraints are designed so that any AI agent can generate the handler code for you. Just describe what your device should do.

---

## ✨ Features

### Core Framework (16 modules)

| Module | Role |
|--------|------|
| `apex_core` | Framework entry point & event bus & watchdog |
| `apex_event` | Global event loop with sticky event replay |
| `apex_config` | NVS-based configuration manager |
| `apex_network` | Wi-Fi AP+STA dual-mode with auto-failover |
| `apex_mqtt` | MQTT 3-channel communication (command/response/notify) |
| `apex_crypto` | AES-CCM-128 encryption with timestamp anti-replay |
| `apex_cmd_executor` | **Command dispatch engine** (the heart of the framework) |
| `apex_webserver` | Embedded HTTP config portal |
| `apex_ota_update` | Dual-partition OTA firmware upgrade |
| `apex_power_down` / `apex_power_up` | Power management |
| `apex_reset` / `apex_restart` | Factory reset / Reboot |
| `apex_stop` | Force-stop persistent actions |
| `apex_get_state` | Query device status |
| `apex_notify` | Device event notifications (JSON-RPC 2.0) |

### Safety Mechanisms

| Mechanism | Trigger | Behavior |
|-----------|---------|----------|
| **Dedup** | Same `msg_id` within 2s | Reject with `APEX_ERR_DUPLICATE` |
| **Circuit Breaker** | 5 consecutive failures | Block command for 5 min (`APEX_ERR_DEGRADED`) |
| **Anti-Replay** | Timestamp outside 30s window | Reject + log |
| **Watchdog** | Handler exceeds 30s | Automatic device reset |
| **Permission Guard** | Command without proper `role` | Reject |

### Example Modules

| Module | Type | Description |
|--------|------|-------------|
| `sync_add` | Sync | Takes two integers, returns sum immediately |
| `async_add` | Async | Takes two integers, returns sum after 100s delay (simulates long-running ops) |

### 🤖 AI-Ready: Template-Driven Code Generation

The framework's true power is that **you don't need to write code**. Because:

- The **communication layer** (WiFi, MQTT, AES-CCM encryption) is fully baked in
- The **control layer** (command dispatch, event bus, safety mechanisms) is fully baked in
- The **template constraints** (handler registration pattern, parameter schemas) are standardized

Any MCP-compatible AI agent can generate a working handler within these constraints. All you do is describe what the device should do:

```
"Make an LED blink handler that takes a GPIO pin number
 and a delay in milliseconds as parameters"
```

The AI generates the `apex_cmd_handler_t` struct, `json_schema`,
and the execute function — exactly matching the framework's contract.
Register it, rebuild, and your device gains the new capability.

---

## 🏗️ Architecture

### Layered Design

```
┌──────────────────────────────────────────┐
│                  main.c                   │  ← Entry point
├──────────────────────────────────────────┤
│              apex_frame/                  │  ← Core layer (16 modules)
│  ┌────────────────────────────────────┐  │
│  │  apex_cmd_executor                 │  │  ← Command dispatch engine
│  │  ┌──────────────────────────────┐  │  │
│  │  │ Decrypt → Dedup → Match      │  │  │
│  │  │ → Lock → Circuit Break       │  │  │
│  │  │ → Execute → Encrypt & Send   │  │  │
│  │  └──────────────────────────────┘  │  │
│  │  ┌──────────────────────────────┐  │  │
│  │  │ Built-in commands:           │  │  │
│  │  │ getInfo│otaUpdate│getState    │  │  │
│  │  │ stop│powerDown│powerUp       ���  │  │
│  │  │ reSet│reStart                │  │  │
│  │  └──────────────────────────────┘  │  │
│  └────────────────────────────────────┘  │
├──────────────────────────────────────────┤
│             components/                   │  ← Application layer
│  sync_add / async_add                     │     Your custom modules go here
├──────────────────────────────────────────┤
│              common/                      │  ← Shared utilities
│  utils (UUID, crypto helpers, HTTP, JSON) │
└──────────────────────────────────────────┘
```

### Data Flow

```
AI Agent (MCP Client)
    │ tools/call
    ▼
apex_mcp_bridge
    │ AES-CCM encrypted MQTT → apex/{device_id}/command
    ▼
ESP32-S3
    │ Base64 decode → AES-CCM decrypt → JSON parse
    ▼
apex_cmd_executor
    │ Dedup → Match → Lock → Circuit Break → Execute handler
    ▼
Your Handler (sync_add / async_add / your_module)
    │ Return APEX_OK / APEX_ASYNC_OK / error
    ▼
Encrypt → MQTT → apex/{device_id}/response
    │
    ▼
apex_mcp_bridge → MCP response → AI Agent
```

---

## 🚀 Quick Start

### Prerequisites

- [ESP-IDF v6.X](https://docs.espressif.com/projects/esp-idf/)
- ESP32-S3 development board

### Build & Flash

```bash
cd apex-esp32-s3-v6-basic
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

> Replace `/dev/ttyUSB0` with your serial port (`COM3` on Windows, `/dev/cu.usbserial-*` on macOS).

### First-Time Setup

1. Power on the device — it starts an AP hotspot named **`APEX-XXXX`**
2. Connect your phone to the hotspot (password: `12345678`)
3. Open `http://192.168.4.1` in a browser
4. Configure your WiFi SSID and password, plus the apex_mcp_bridge broker address
5. The device reboots, connects to your router, and the AP turns off after 5 minutes

---

## 📋 Built-in Commands

These are automatically registered by the framework:

| `cmd_key` | Function | Description | `flags` |
|-----------|----------|-------------|---------|
| `getInfo` | Device Info | Returns capability list (MCP `tools/list`) | `ALWAYS_ALLOWED` |
| `getState` | Get State | Returns current device state & running commands | `ALWAYS_ALLOWED` |
| `otaUpdate` | OTA Update | Triggers a remote firmware upgrade | `EXCLUSIVE` |
| `stop` | Stop | Force-stops a persistent action | `FORCE` |
| `powerDown` | Power Down | Shuts down the device | `FORCE` |
| `powerUp` | Power Up | Wakes the device | `ALWAYS_ALLOWED` |
| `reSet` | Factory Reset | Resets all config to defaults | `FORCE` |
| `reStart` | Restart | Reboots the device | `FORCE` |

### Example: `getInfo` response

When an AI agent calls `tools/list`, the hub queries `getInfo` on the device. The framework auto-generates JSON Schema for all registered commands:

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

## 🛠️ Writing Your Own Commands

Every custom module follows the same 3-step pattern:

1. **Define parameters** with `function_param_desc_t`
2. **Implement the handler** (sync, async, or persistent)
3. **Register** in `init()` with `apex_cmd_register()`

### Pattern A: Sync Handler (Most Common)

For instant operations: calculations, state queries, simple GPIO toggles.

```c
#include "apex_cmd_executor.h"

static const char *CMD_KEY = "my_led";

static const function_param_desc_t params[] = {
    {.key = "state", .type = "bool", .description = "Turn LED on or off"},
};

static int led_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    cJSON *state = cJSON_GetObjectItem(params, "state");
    if (!cJSON_IsBool(state)) return APEX_ERR_PARAM;

    gpio_set_level(LED_GPIO, state->valueint ? 1 : 0);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "led", state->valueint);
    *res_data = data;

    return APEX_OK;  // Framework auto-sends response + unlocks
}

void my_led_init(void)
{
    static char schema[1024];
    build_function_param_desc_json(params, 1, schema, sizeof(schema));

    apex_cmd_entry_t entry = {
        .cmd_key = CMD_KEY,
        .function_name = "LED Control",
        .function_desc = "Turn the onboard LED on or off",
        .function_params = schema,
        .role = "user",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_PARALLEL,
        .handler = led_handler,
    };
    apex_cmd_register(entry);
}
```

**Lifecycle:**
```
register → receive cmd → lock slot → handler returns APEX_OK
  → framework sends success response + unlocks → done
```

---

### Pattern B: Async Handler

For long-running operations: OTA updates, network requests, sensor sampling.

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct {
    int param;
    char msg_id[48];  // ⚠️ MUST save for apex_cmd_finish()
} my_ctx_t;

static void async_task(void *arg)
{
    my_ctx_t *ctx = (my_ctx_t *)arg;

    // ... do time-consuming work ...
    vTaskDelay(pdMS_TO_TICKS(5000));

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "status", "done");
    apex_cmd_finish(ctx->msg_id, APEX_OK, data);  // Unlock + send response
    free(ctx);
    vTaskDelete(NULL);
}

static int my_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    my_ctx_t *ctx = calloc(1, sizeof(my_ctx_t));
    strlcpy(ctx->msg_id, msg_id, sizeof(ctx->msg_id));

    xTaskCreate(async_task, "async", 4096, ctx, 5, NULL);
    return APEX_ASYNC_OK;  // Framework holds lock, sends "processing"
}
```

**Lifecycle:**
```
register → receive cmd → lock slot → handler creates FreeRTOS task → returns APEX_ASYNC_OK
  → framework sends "processing" (holds lock)
  → background task finishes → calls apex_cmd_finish()
  → framework sends completed response + unlocks
```

---

### Pattern C: Persistent Action (Stoppable)

For continuous operations that need explicit stop: motor rotation, LED breathing, audio playback.

```c
static int motor_stop(cJSON *params, const char *msg_id, cJSON **res_data)
{
    motor_hardware_stop();
    apex_cmd_finish(msg_id, APEX_OK, NULL);  // ⚠️ Must call finish in stop handler
    return APEX_OK;
}

static int motor_start(cJSON *params, const char *msg_id, cJSON **res_data)
{
    int speed = 50;
    cJSON *s = cJSON_GetObjectItem(params, "speed");
    if (cJSON_IsNumber(s)) speed = s->valueint;

    motor_hardware_start(speed);
    return APEX_OK;  // Persistent: framework holds lock after OK
}

void motor_init(void)
{
    apex_cmd_entry_t entry = {
        .cmd_key = "motorRun",
        .function_name = "Motor Control",
        .function_desc = "Start motor continuously, stop via stop command",
        .function_params = "{\"speed\":{\"type\":\"integer\"}}",
        .flags = APEX_CMD_FLAG_EXCLUSIVE,  // Exclusive: no parallel use
        .handler = motor_start,
        .is_persistent = true,              // ⚠️ Must set true
        .stop_handler = motor_stop,          // ⚠️ Must provide stop handler
    };
    apex_cmd_register(entry);
}
```

**Lifecycle:**
```
receive start → lock slot → handler starts motor → returns APEX_OK
  → framework sends success (holds lock)
  → device keeps running
  → receive stop → stop_handler stops motor → calls apex_cmd_finish() → unlocks
```

---

## 🔒 MQTT 3-Channel Communication

| Channel | Topic | Direction | Format |
|---------|-------|-----------|--------|
| **Command** | `apex/{device_id}/command` | Hub → Device | JSON |
| **Response** | `apex/{device_id}/response` | Device → Hub | AES-CCM encrypted JSON |
| **Notify** | `apex/{device_id}/notify` | Device → Hub | JSON-RPC 2.0 Notification (encrypted) |

> **Command + Response** form a request-reply pair. **Notify** is a fire-and-forget push from the device (alarms, state changes, task completion).

---

## 📂 Project Structure

```
apex-esp32-s3-v6-basic/
├── main/
│   ├── main.c                    # Entry point: init framework → register modules
│   └── CMakeLists.txt
│
├── apex_frame/                   # Core framework (DO NOT modify)
│   ├── apex_core/                #   Framework entry & event bus & watchdog
│   ├── apex_event/               #   Global event loop (sticky events)
│   ├── apex_config/              #   Configuration management (NVS)
│   ├── apex_network/             #   Wi-Fi AP+STA dual-mode manager
│   ├── apex_mqtt/                #   MQTT client (3 channels)
│   ├── apex_crypto/              #   AES-CCM-128 encryption
│   ├── apex_cmd_executor/        #   ⭐ Command dispatch engine
│   ├── apex_webserver/           #   Web configuration portal
│   ├── apex_ota_update/          #   OTA firmware upgrade
│   ├── apex_power_down/          #   Power down
│   ├── apex_power_up/            #   Power up
│   ├── apex_reset/               #   Factory reset
│   ├── apex_restart/             #   Reboot
│   ├── apex_stop/                #   Force-stop persistent actions
│   ├── apex_get_state/           #   Query device state
│   └── apex_notify/              #   Device event notifications
│
├── common/
│   └── utils/                    # UUID, crypto helpers, HTTP, JSON utilities
│
├── components/                   # Your modules go here
│   ├── sync_add/                 #   Example: sync command
│   └── async_add/                #   Example: async command
│
├── partitions.csv                # OTA dual-partition layout
├── sdkconfig                     # ESP-IDF project config
└��─ CMakeLists.txt                # Top-level CMake
```

---

## ⚙️ Configuration

### Default Settings

| Setting | Default Value |
|---------|---------------|
| Wi-Fi STA SSID | `APEX_STA` |
| Wi-Fi STA Password | `apex123` |
| Wi-Fi AP SSID | `APEX-XXXX` (dynamic, based on MAC) |
| Wi-Fi AP Password | `12345678` |
| MQTT Broker | `agent-plat.local:1883` |
| Device Password | `apex123` |
| Web Portal Login | `12345678` |

All settings can be changed via the web portal at `http://192.168.4.1` during first-time setup, or at the device's LAN IP afterwards.

### Partition Table

| Name | Size | Type | Purpose |
|------|------|------|---------|
| `nvs` | 128 KB | data | Configuration storage |
| `otadata` | 8 KB | data | OTA state metadata |
| `phy_init` | 4 KB | data | PHY calibration |
| `ota_0` | 4 MB | app | Firmware slot A |
| `ota_1` | 4 MB | app | Firmware slot B |
| `storage` | 8 MB | spiffs | File storage |

---

## 🧪 Command Concurrency Flags

Every command must pick one flag:

| Flag | Behavior | When to Use |
|------|----------|-------------|
| `ALWAYS_ALLOWED` | Never blocks, runs anytime | Read-only queries (`getInfo`, `getState`) |
| `PARALLEL` | Can run alongside other commands | Stateless operations (`sync_add`) |
| `EXCLUSIVE` | Locks the system, one at a time | Hardware-critical ops (OTA, motor init) |
| `FORCE` | Overrides everything, emergency priority | E-stop, restart, power-down |

---

## ✅ Command Checklist

Before registering a new command, verify:

- [ ] `function_param_desc_t` array is defined correctly
- [ ] `build_function_param_desc_json()` is called to generate JSON Schema
- [ ] Parameter keys match between definition and handler
- [ ] `function_desc` is clear and describes the command's purpose
- [ ] `role` is correctly set (`"admin"` or `"user"`)
- [ ] `flags` match the operational semantics
- [ ] If `is_persistent = true`: `stop_handler` is implemented and calls `apex_cmd_finish()`
- [ ] For async: `msg_id` is saved in context struct and passed to `apex_cmd_finish()`
- [ ] `init()` is called **after** `apex_cmd_executor_init()` in `main.c`

---

## 🧩 Ecosystem Repos

This project is the **hardware firmware** layer. The full stack:

| Project | Role | Repo |
|---------|------|------|
| **apex_mcp_bridge** | Core — AI agent ↔ hardware gateway | [github.com/apex-freen/apex-mcp-bridge](https://github.com/apex-freen/apex-mcp-bridge) |
| **service-plugins** | Extensible plugin framework | [github.com/apex-freen/service-plugins](https://github.com/apex-freen/service-plugins) |
| **apex-esp32-s3-v6** | Hardware firmware (this repo) | [github.com/apex-freen/apex-esp32-s3-v6](https://github.com/apex-freen/apex-esp32-s3-v6) |

---

## 📄 License

[MIT](../LICENSE) © 2026 apex-freen

---

<p align="center">
  <em>Write a handler. Register it. Your AI gets a new skill.</em>
</p>
