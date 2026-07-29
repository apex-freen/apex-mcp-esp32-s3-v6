<p align="center">
  <img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License">
  <img src="https://img.shields.io/badge/ESP--IDF-v6.X-green.svg" alt="ESP-IDF">
  <img src="https://img.shields.io/badge/Platform-ESP32--S3%20%7C%20ESP32--C3-orange.svg" alt="Platform">
  <img src="https://img.shields.io/badge/MCP-Standard-purple.svg" alt="MCP">
  <img src="https://img.shields.io/badge/MCP_Spec-2026.07-blue.svg" alt="MCP Spec">
  <img src="https://img.shields.io/badge/Protocol-MQTT%20%2B%20JSON--RPC%202.0-lightgrey.svg" alt="Protocol">
</p>

<h1 align="center">APEX — Universal Agent Device Framework</h1>

<p align="center">
  <em>Turn an ESP32 chip into a "smart device core" that any MCP-compatible AI agent can control directly.<br>Fully compliant with the latest MCP specification (July 2026).</em>
</p>

<p align="center">
  <a href="#-what-is-apex">What</a> ·
  <a href="#-architecture">Architecture</a> ·
  <a href="#-quick-start">Quick Start</a> ·
  <a href="#-sub-projects">Projects</a> ·
  <a href="#-extensibility">Extend</a> ·
  <a href="https://github.com/apex-freen/apex-esp32-s3-v6">GitHub</a>
</p>

---

[中文文档](./README_ZH.md)

---

## 📖 What is APEX?

**APEX** is an open-source **ESP32 agent-device firmware framework** — the hardware-side foundation of the [apex_mcp_bridge](https://github.com/apex-freen/apex-mcp-bridge) ecosystem, working together with [service-plugins](https://github.com/apex-freen/service-plugins) to form the full AI-to-hardware stack.

> In short: build your hardware product. That's it. No apps. No cloud backends. No MCP protocol knowledge required. **You don't even need to write code yourself** — the framework provides standardized templates that constrain the structure. Just describe your device's behavior to any MCP-compatible AI agent, it can generate the handler code for you, register it, and your device is instantly controllable — no matter which platform or model you use.

### How it works

```
You say "Turn on the living room light"
  → AI agent understands intent
    → Sends MCP command to the hub
      → MQTT encrypted transmission to ESP32
        → Device executes → sends response
          → AI app tells you "Done"
```

### What APEX provides

| Role | Description |
|------|-------------|
| 🔌 **Hardware Framework** | Complete firmware base for ESP32-S3 / ESP32-C3 chips |
| 🌉 **Protocol Bridge** | MQTT ↔ MCP seamless translation; device speaks MQTT, AI sees MCP |
| 🛡️ **Secure Executor** | AES-CCM encryption, timestamp anti-replay, circuit breaker, dedup |
| 🤖 **AI-Generated Code** | Standardized templates enable AI agents to generate device handlers — no manual coding required |
| 📦 **Developer Platform** | Event-driven + modular architecture, business modules are self-contained |

---

## ✨ Why APEX?

| Pain Point | APEX Solution |
|-------------|---------------|
| IoT platform gatekeeping | **Zero barriers** — no platform approval needed |
| Must build your own app | **Zero app dev** — connects to any MCP-compatible AI agent directly |
| Multi-platform duplication | **Write once** — unified MCP protocol, all AI platforms compatible |
| Steep learning curve | **Zero manual coding** — standardized templates; AI agents generate device handlers for you |
| Repetitive boilerplate | **Batteries included** — WiFi, MQTT, encryption, OTA all baked into the framework |

### Open Source Highlights

- ✅ **MIT Licensed** — fully open source, commercial use welcome
- ✅ **MCP First-Class** — native Model Context Protocol support
- ✅ **5-Layer Security** — encryption + anti-replay + circuit breaker + dedup + permission levels
- ✅ **Production-Ready** — ESP-IDF v6.X + FreeRTOS + dual-partition OTA
- ✅ **AI-Ready Templates** — standardized handler patterns let any AI agent generate device code
- ✅ **Zero Ops Cost** — fully local architecture, data never leaves your network

---

## 🏗️ Architecture

```
                         AI Agents
             (any MCP-compatible AI application)
                              │
                    MCP (Streamable HTTP)
                              │
              ┌───────────────┴───────────────┐
              │        apex_mcp_bridge (Bridge)       │
              │   Token Auth · RBAC · Rate Lim │
              └───────────────┬───────────────┘
                              │
                MQTT (AES-CCM Encrypted)
                              │
              ┌───────────────┴───────────────┐
              │     Device (This Project)      │
              │  ┌─────────────────────────┐  │
              │  │   apex_cmd_executor     │  │
              │  │   Command Dispatcher     │  │
              │  │   Decrypt→Dedup→CB→Exec  │  │
              │  └─────────────────────────┘  │
              │  ┌─────────────────────────┐  │
              │  │   Business Modules       │  │
              │  │   GPIO · PWM · I2C · SPI │  │
              │  └─────────────────────────┘  │
              │  ┌─────────────────────────┐  │
              │  │  WiFi · MQTT · OTA · Web│  │
              │  └─────────────────────────┘  │
              └───────────────────────────────┘
                              │
                Peripherals (Sensors · Motors · Lights · Relays...)
```

---

## 📦 Tech Stack

| Layer | Tech | Notes |
|-------|------|-------|
| **Chip** | ESP32-S3 / ESP32-C3 | 160MHz, PSRAM, Wi-Fi + BLE |
| **Framework** | ESP-IDF v6.X | Espressif official IoT SDK |
| **RTOS** | FreeRTOS | Preemptive multi-tasking |
| **Build** | CMake | Modular build system |
| **Transport** | MQTT over TLS | 3 channels: Command / Response / Notify |
| **Crypto** | AES-CCM-128 | PSA Crypto API |
| **Protocol** | MCP (JSON-RPC 2.0) | AI agent standard control protocol |
| **Data** | JSON (cJSON) | Command params, responses, schema generation |
| **Storage** | NVS + SPIFFS | Config persistence + file system |
| **Update** | Dual-partition OTA | Seamless firmware upgrades |

---

## 🚀 Quick Start

### 1. Prerequisites

```bash
# Install ESP-IDF v6.X
# See: https://docs.espressif.com/projects/esp-idf/

# Clone the repo
git clone https://github.com/apex-freen/apex-esp32-s3-v6.git
cd apex-esp32-s3-v6
```

### 2. Pick a sub-project

```bash
cd apex-esp32-s3-v6-basic      # Basic version (recommended starting point)
```

### 3. Build & Flash

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

### 4. First-Time Setup

Power on the device. It automatically starts an AP hotspot `APEX-XXXX` (password: `12345678`). Connect your phone and visit `http://192.168.4.1` to configure WiFi. Once connected, the AP turns off after 5 minutes.

---

## 📂 Project Structure

```
apex-esp32-s3-v6/                    # Repository root
├── apex-esp32-s3-v6-basic/          # Sub-project: Basic reference implementation
│   ├── apex_frame/                  #   Core framework layer (16 modules)
│   ├── components/                  #   Example modules (sync_add / async_add)
│   ├── common/                      #   Shared utilities
│   ├── main/                        #   Entry point
│   ├── README.md                    #   English docs
│   └── README_ZH.md                 #   Chinese docs
├── LICENSE                          # MIT
├── README.md                        # This file (English)
└── README_ZH.md                     # Chinese docs
```

### Sub-projects

| Sub-project | Description | For |
|-------------|-------------|-----|
| [`apex-esp32-s3-v6-basic`](./apex-esp32-s3-v6-basic/) | Basic — full framework + sync/async examples | Learning & starting new projects |

> More sub-projects (lighting control, sensor data collection, motor driver, etc.) are in development.

---

## 🔧 Extensibility

APEX is designed to be **chip-agnostic and platform-agnostic**:

| Currently | Easily Extendable To |
|-----------|---------------------|
| ESP32-S3 + IDF v6.X | ESP32-C3 / C6 / H2 / P4 and beyond |
| Wi-Fi + MQTT | BLE, Zigbee, Thread, LoRa |
| MCP protocol | More AI platforms as they adopt MCP |

---

## 👥 Community

| Channel | Info |
|---------|------|
| Discord | `#` (coming soon) |
| QQ Group | 882419824 (join keyword: `APEX智能体设备`) |

## 🧩 Ecosystem Repos

| Project | Role | Repo |
|---------|------|------|
| **apex_mcp_bridge** | Core — AI agent ↔ hardware gateway | [github.com/apex-freen/apex-mcp-bridge](https://github.com/apex-freen/apex-mcp-bridge) |
| **service-plugins** | Extensible plugin framework | [github.com/apex-freen/service-plugins](https://github.com/apex-freen/service-plugins) |
| **apex-esp32-s3-v6** | Hardware firmware — ESP32-S3 (this repo) | [github.com/apex-freen/apex-esp32-s3-v6](https://github.com/apex-freen/apex-esp32-s3-v6) |
| **apex-esp32-c3-v6** | Hardware firmware — ESP32-C3 | [github.com/apex-freen/apex-esp32-c3-v6](https://github.com/apex-freen/apex-esp32-c3-v6) |

---

## 📄 License

[MIT](LICENSE) © 2026 apex-freen

---

<p align="center">
  <em>Every hardware product, effortlessly controlled by AI agents.</em>
</p>
