<p align="center">
  <img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License">
  <img src="https://img.shields.io/badge/ESP--IDF-v6.X-green.svg" alt="ESP-IDF">
  <img src="https://img.shields.io/badge/Platform-ESP32--S3%20%7C%20ESP32--C3-orange.svg" alt="Platform">
  <img src="https://img.shields.io/badge/MCP-Standard-purple.svg" alt="MCP">
  <img src="https://img.shields.io/badge/MCP_Spec-2026.07-blue.svg" alt="MCP Spec">
  <img src="https://img.shields.io/badge/Protocol-MQTT%20%2B%20JSON--RPC%202.0-lightgrey.svg" alt="Protocol">
</p>

<h1 align="center">APEX — 通用智能体设备开发框架</h1>

<p align="center">
  <em>将 ESP32 芯片变成一颗任何支持 MCP 协议的智能体都能直接控制的「智能设备芯」<br>全面支持最新 MCP 协议规范（2026 年 7 月）</em>
</p>

<p align="center">
  <a href="#-apex-是什么">是什么</a> ·
  <a href="#-系统架构">架构</a> ·
  <a href="#-快速开始">快速开始</a> ·
  <a href="#-子项目">子项目</a> ·
  <a href="#-扩展性">扩展</a> ·
  <a href="https://gitee.com/freen/apex-esp32-s3-v6">Gitee</a>
</p>

---

[English](./README.md)

---

## 📖 APEX 是什么？

**APEX** 是一个开源的 **ESP32 智能体设备固件框架**，是 [apex_mcp_bridge（智能体工具中枢）](https://gitee.com/freen/apex-mcp-bridge) 生态的硬件端基石，与 [service-plugins（插件框架）](https://gitee.com/freen/service-plugins) 共同构成完整的 AI 到硬件的技术栈。

> 一句话：你只需专注做好自己的硬件产品，无需开发 App、无需搭建云端、无需理解 MCP 协议。**你甚至不需要自己动手写代码**——框架提供了标准化的模板与约束，只需向任何 MCP 智能体描述你的设备功能，智能体就能自动生成 handler 代码并注册，你的设备立刻就能被**任何支持 MCP 协议的智能体应用**控制，不限平台、不限模型。

### 工作方式

```
你说"打开客厅的灯"
  → AI 智能体理解意图
    → MCP 协议下发指令到中控
      → MQTT 加密传输到 ESP32 设备
        → 设备执行 → 响应回传
          → AI App 告诉你"已打开"
```

### 核心定位

| 角色 | 说明 |
|------|------|
| 🔌 **硬件端框架** | 为 ESP32-S3 / ESP32-C3 芯片提供完整的设备固件底座 |
| 🌉 **协议翻译器** | MQTT ↔ MCP 无缝转换，设备说 MQTT，AI 看到的是 MCP |
| 🛡️ **安全执行器** | AES-CCM 加密、时间戳防重放、熔断降级、指令去重 |
| 🤖 **AI 代码生成** | 标准化模板使智能体可直接生成设备 handler——无需手写代码 |
| 📦 **开发者平台** | 事件驱动 + 模块化架构，业务模块独立封装、开箱即用 |

---

## ✨ 为什么选择 APEX？

| 痛点 | APEX 怎么做 |
|------|------------|
| 物联网平台审核繁琐 | **零壁垒** — 不依赖任何物联网平台，产品通过国家标准即可 |
| 必须自建 App | **零 App** — 直接对接任何支持 MCP 协议的智能体应用 |
| 多平台重复开发 | **一次开发** — 统一 MCP 协议，所有 AI 平台通用 |
| 技术栈太复杂 | **零手写代码** — 框架提供标准化模板，智能体替你生成设备 handler |
| 重复造轮子 | **开箱即用** — WiFi、MQTT、加密、OTA 框架已全部内置 |

### 开源亮点

- ✅ **MIT 协议** — 完全开源，商用无忧
- ✅ **MCP 一等公民** — 固件原生兼容 Model Context Protocol 标准
- ✅ **5 重安全防护** — 加密传输 + 防重放 + 熔断降级 + 指令去重 + 权限分级
- ✅ **工业级可靠性** — ESP-IDF v6.X + FreeRTOS + OTA 双分区升级
- ✅ **AI 就绪模板** — 标准化 handler 模式，任何智能体都能直接生成设备代码
- ✅ **零运维成本** — 纯本地架构，数据不出内网，无需云服务器

---

## 🏗️ 系统架构

```
                          AI 智能体
                 (任何支持 MCP 协议的智能体应用)
                              │
                    MCP (Streamable HTTP)
                              │
              ┌───────────────┴───────────────┐
              │      智能体工具中枢 (apex_mcp_bridge)       │
              │    Token 鉴权 · 权限校验 · 限流   │
              └───────────────┬───────────────┘
                              │
                    MQTT (AES-CCM 加密)
                              │
              ┌───────────────┴───────────────┐
              │       设备端 (本项目)            │
              │  ┌─────────────────────────┐  │
              │  │   apex_cmd_executor     │  │
              │  │   指令调度中枢             │  │
              │  │   解密→去重→熔断→执行→响应  │  │
              │  └─────────────────────────┘  │
              │  ┌─────────────────────────┐  │
              │  │   业务模块 (Components)   │  │
              │  │   GPIO · PWM · I2C · SPI │  │
              │  └─────────────────────────┘  │
              │  ┌─────────────────────────┐  │
              │  │   WiFi · MQTT · OTA · Web│  │
              │  └─────────────────────────┘  │
              └───────────────────────────────┘
                              │
                      硬件外设 (传感器 · 电机 · 灯光 · 继电器...)
```

---

## 📦 技术栈

| 层级 | 技术 | 说明 |
|------|------|------|
| **芯片** | ESP32-S3 / ESP32-C3 | 160MHz, PSRAM, Wi-Fi + BLE |
| **框架** | ESP-IDF v6.X | 乐鑫官方 IoT 开发框架 |
| **RTOS** | FreeRTOS | 多任务调度、异步任务管理 |
| **构建** | CMake | 模块化构建系统 |
| **通信** | MQTT over TLS | 三通道：Command / Response / Notify |
| **加密** | AES-CCM-128 | 基于 PSA Crypto API |
| **协议** | MCP (JSON-RPC 2.0) | AI 智能体标准控制协议 |
| **数据** | JSON (cJSON) | 指令参数、响应、Schema 生成 |
| **存储** | NVS + SPIFFS | 配置持久化 + 文件系统 |
| **升级** | OTA 双分区 | 无缝固件升级 |

---

## 🚀 快速开始

### 1. 环境准备

```bash
# 安装 ESP-IDF v6.X
# 参考: https://docs.espressif.com/projects/esp-idf/

# 克隆仓库
git clone https://gitee.com/freen/apex-esp32-s3-v6.git
cd apex-esp32-s3-v6
```

### 2. 选择一个子项目

```bash
cd apex-esp32-s3-v6-basic      # 基础版（推荐入门）
```

### 3. 编译 & 烧录

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

### 4. 首次配网

设备上电后自动开启 AP 热点 `APEX-XXXX`（密码：`12345678`），手机连接后访问 `http://192.168.4.1` 进行 WiFi 配置。配网成功后设备自动连接路由器，5 分钟后 AP 自动关闭。

---

## 📂 项目结构

```
apex-esp32-s3-v6/                    # 仓库根目录
├── apex-esp32-s3-v6-basic/          # 子项目：基础参考实现
│   ├── apex_frame/                  #   核心框架层（16 个模块）
│   ├── components/                  #   业务示例（sync_add / async_add）
│   ├── common/                      #   通用工具库
│   ├── main/                        #   入口
│   ├── README.md                    #   英文文档
│   └── README_ZH.md                 #   中文文档
├── LICENSE                          # MIT
├── README.md                        # 英文文档
└── README_ZH.md                     # 本文档（中文）
```

### 子项目

| 子项目 | 定位 | 适合 |
|--------|------|------|
| [`apex-esp32-s3-v6-basic`](./apex-esp32-s3-v6-basic/) | 基础版 — 完整框架 + 同步/异步示例 | 入门学习 & 新项目起步 |

> 更多子项目（灯光控制、传感器采集、电机驱动等）正在开发中。

---

## 🔧 扩展性

APEX 框架设计为 **芯片无关、平台无关**：

| 当前 | 可扩展 |
|------|--------|
| ESP32-S3 + IDF v6.X | ESP32-C3 / C6 / H2 / P4 等全系列 |
| WiFi + MQTT | BLE、Zigbee、Thread、LoRa |
| MCP 协议 | 对接越来越多 AI 平台 |

---

## 👥 社区

| 渠道 | 信息 |
|------|------|
| QQ 群 | 882419824（入群答案：`APEX智能体设备`） |
| Discord | `#`（即将上线） |

## 🧩 生态仓库

| 项目 | 定位 | 仓库 |
|------|------|------|
| **apex_mcp_bridge** | 核心 — 智能体 ↔ 硬件网关 | [gitee.com/freen/apex-mcp-bridge](https://gitee.com/freen/apex-mcp-bridge) |
| **service-plugins** | 可扩展插件框架 | [gitee.com/freen/service-plugins](https://gitee.com/freen/service-plugins) |
| **apex-esp32-s3-v6** | 底层硬件固件 — ESP32-S3（本项目） | [gitee.com/freen/apex-esp32-s3-v6](https://gitee.com/freen/apex-esp32-s3-v6) |
| **apex-esp32-c3-v6** | 底层硬件固件 — ESP32-C3 | [gitee.com/freen/apex-esp32-c3-v6](https://gitee.com/freen/apex-esp32-c3-v6) |

---

## 📄 License

[MIT](LICENSE) © 2026 apex-freen

---

<p align="center">
  <em>让每一款硬件产品，都能被 AI 智能体轻松控制。</em>
</p>
