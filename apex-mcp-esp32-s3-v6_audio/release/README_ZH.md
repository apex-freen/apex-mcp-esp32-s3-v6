# APEX MCP ESP32-S3 V6 音频开发板 · 固件烧录指南

![Version](https://img.shields.io/badge/Firmware-v1.0.0-blue)
![Chip](https://img.shields.io/badge/Chip-ESP32--S3-green)
![Flash](https://img.shields.io/badge/Flash-16MB--minimum-orange)
![PSRAM](https://img.shields.io/badge/PSRAM-Octal%208--line-important)

> 本固件面向 APEX MCP ESP32-S3 V6 音频开发板，集成 DLNA DMR 音频播放、SMB 网络流媒体、MQTT 远程控制、OTA 升级等功能。
>
> 🎯 **面向终端用户，零安装即可完成烧录** —— 推荐使用 [ESP Launchpad 网页工具](#二通过-esp-launchpad-在线烧录推荐)。

---

## 目录

- [一、硬件要求](#一硬件要求)
- [二、通过 ESP Launchpad 在线烧录（推荐）](#二通过-esp-launchpad-在线烧录推荐)
- [三、通过 Flash Download Tool 离线烧录](#三通过-flash-download-tool-离线烧录)
- [四、通过 esptool 命令行烧录（高级用户）](#四通过-esptool-命令行烧录高级用户)
- [五、烧录后验证](#五烧录后验证)
- [六、故障排查](#六故障排查)
- [七、固件文件说明](#七固件文件说明)

---

## 一、硬件要求

烧录前请确认你的开发板满足以下**全部**硬件条件，否则固件无法正常启动。

| 项目 | 要求 | 说明 |
|:---:|:---|:---|
| **芯片** | ESP32-S3 | ❌ 不兼容 ESP32 / S2 / C3 / C6 / H2 等其它型号 |
| **Flash** | ≥ 16 MB | 分区表已占用 16 MB 空间，8 MB 及以下无法启动 |
| **PSRAM** | **Octal 八线 PSRAM** | ✅ 模组型号末尾带 **V** 后缀：`N16R8V`、`N32R8V` 等<br>❌ 无 V 后缀（Quad PSRAM）会启动失败并报错 `external RAM init failed` |

### ✅ 兼容模组清单

- `ESP32-S3-WROOM-1 N16R8V`
- `ESP32-S3-WROOM-2 N16R8V`
- `ESP32-S3-DevKitM-1`（M 系列板载 Octal PSRAM）
- `ESP32-S3-DEVKITS V1.2`（V 版本 Octal）

### ❌ 不兼容模组

- `ESP32-S3-WROOM-1 N16R8`（无 V = Quad PSRAM）
- `ESP32-S3-WROOM-1 N8R8`（Flash 仅 8 MB）
- `ESP32-S3-WROOM-1 N8R2`（Flash 8 MB + PSRAM 2 MB）

---

## 二、通过 ESP Launchpad 在线烧录（推荐）

**终端用户首选方式。** 不需要安装任何软件、不需要下载驱动工具包，只需一台装有 Chrome 或 Edge 浏览器的电脑。

### 2.1 准备工作

1. 一根 **USB 数据线**（注意：部分廉价线仅支持充电，无法通信）
2. 一块满足 [硬件要求](#一硬件要求) 的 ESP32-S3 开发板
3. 浏览器：**Chrome 89+** / **Edge 89+** / 任意 Chromium 内核浏览器
   > ❌ Firefox、Safari 不支持 WebSerial API，无法使用。

### 2.2 烧录步骤

#### Step 1 · 进入下载模式

大部分 ESP32-S3 开发板带有自动下载电路，可跳过此步。若后续烧录提示连接失败，请返回执行：

```
① 按住板上 BOOT 按钮不放
② 快速按一下 EN / RST 按钮（复位）
③ 松开 BOOT 按钮
→ 设备已进入下载模式
```

#### Step 2 · 打开 ESP Launchpad

在浏览器中访问：

👉 **https://espressif.github.io/esp-launchpad/**

#### Step 3 · 切换到 DIY 模式并加载固件

1. 页面顶部点击 **DIY** 标签页（不是默认的 Quick Start）
2. **ESP Chipset Type** 下拉选择 **`ESP32-S3`**
3. 点击 **`Add File`** 按钮
4. 选择本目录下的固件文件：**`apex_mcp_esp32-s3-v6-audio.bin`**
5. **Flash Address** 保持默认值：**`0x0`**

> 说明：`apex_mcp_esp32-s3-v6-audio.bin` 为已合并的完整固件，包含 Bootloader、Partition Table、OTA Data 和 App，因此烧录到起始地址 `0x0` 即可。
>
> Flash 工作模式（DIO）、频率（80 MHz）、容量（16 MB）等参数已写入固件文件头部，Launchpad 会自动识别，无需手动设置。

#### Step 4 · 连接并烧录

1. 页面顶部点击 **`Connect`** 按钮
2. 浏览器弹出"选择串口"对话框，选中你的开发板对应的串口设备（通常显示为 `CP210x USB to UART Bridge` 或 `USB-SERIAL CH340`），点击连接
3. **Flashing baud rate** 保持默认 `921600`（若烧录失败可降低为 `460800` 或 `115200`）
4. 点击 **`Flash`** 按钮开始烧录
5. 等待进度条走完（1.86 MB 固件约需 30-60 秒），显示 `Flashing complete` 即完成

#### Step 5 · 启动固件

按一次开发板上的 **EN / RST** 复位按钮，固件开始运行。

---

## 三、通过 Flash Download Tool 离线烧录

适用于无网络环境或无法使用浏览器 WebSerial 的场景。使用乐鑫官方 Windows 桌面工具。

### 3.1 下载工具

从乐鑫官网下载 Flash Download Tool：

👉 https://www.espressif.com/en/support/download/other-tools

选择 **Flash Download Tools**（Windows 版，约 30 MB，免安装）。

### 3.2 烧录步骤

1. 解压并运行 `flash_download_tool_x.x.x.exe`
2. 弹出选择页：
   - **ChipType**：`ESP32-S3`
   - **WorkMode**：`Develop`
   - **LoadMode**：`UART`
   - 点击 **OK**
3. 在 SPI 下载页配置：
   | # | 勾选 | 文件 | 地址 |
   |---|:---:|:---|:---:|
   | 1 | ☑ | `apex_mcp_esp32-s3-v6-audio.bin` | `0x0` |
4. 右下角配置：
   - **SPI SPEED**：`80 MHz`
   - **SPI MODE**：`DIO`
   - **FLASH SIZE**：`16 MB`
   - **BAUD**：`921600`
   - **COM**：选择开发板对应的串口
5. 先点 **ERASE** 擦除整片 Flash（约 10 秒）
6. 擦除完成后点 **START** 开始烧录
7. 显示 `FINISH` 即完成，按 RST 启动

---

## 四、通过 esptool 命令行烧录（高级用户）

适用于已安装 ESP-IDF 或 Python + esptool 环境的开发者。

### 4.1 环境要求

- Python 3.8+
- 已安装 esptool：`pip install esptool`

### 4.2 烧录命令

```powershell
# Windows PowerShell
esptool.py --chip esp32s3 `
  --port COM5 --baud 921600 `
  --before default-reset --after hard-reset `
  write_flash -z `
  --flash-mode dio --flash-freq 80m --flash-size 16MB `
  0x0 apex_mcp_esp32-s3-v6-audio.bin
```

```bash
# Linux / macOS
esptool.py --chip esp32s3 \
  --port /dev/ttyUSB0 --baud 921600 \
  --before default-reset --after hard-reset \
  write_flash -z \
  --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 apex_mcp_esp32-s3-v6-audio.bin
```

> 将 `COM5`（Windows）或 `/dev/ttyUSB0`（Linux/macOS）替换为实际串口。

---

## 五、烧录后验证

### 5.1 串口日志检查

使用任意串口监视器（PuTTY、MobaXterm、Arduino IDE、VS Code Serial Monitor 均可）：

| 参数 | 值 |
|:---|:---|
| 波特率 | 115200 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验位 | None |

按 RST 复位后，正常启动日志开头应包含类似内容：

```
I (29) boot: ESP-IDF v5.x 2nd stage bootloader
I (xxx) boot:  ... chip revision 0.x
I (xxx) esp_psram: Found 8MB PSRAM
I (xxx) apex_core: APEX MCP system initialized
I (xxx) apex_network: WiFi connecting to SSID: ...
```

关键检查点：
- ✅ 未出现 `external RAM init failed` → PSRAM 正常
- ✅ 未出现 `partition table error` → Flash 容量足够
- ✅ 正常进入 `apex_core` 初始化流程

### 5.2 功能测试

固件启动成功后，可按以下项目快速验证：

| 功能 | 验证方法 |
|:---|:---|
| DLNA DMR | 手机端 BubbleUPNP / Windows 媒体播放器 → 能否发现设备并推送音频 |
| Web 配置 | 浏览器访问设备 IP → 能否打开配置页面（默认端口 80） |
| MQTT | 设备是否按预期连接配置的 Broker |

---

## 六、故障排查

### ❌ ESP Launchpad 连接时看不到串口

**原因 1**：未安装 USB 转串口驱动。
- **CP210x 驱动**：搜索 `Silicon Labs CP210x USB to UART Bridge Driver`
- **CH340 驱动**：搜索 `CH341SER Windows Driver`

**原因 2**：使用了仅充电的 USB 线。
- 更换标注"数据线"的 USB 线，优先使用 USB 2.0 端口。

### ❌ 烧录报 `Failed to connect to ESP32-S3`

- 重新执行 [进入下载模式](#step-1--进入下载模式) 步骤
- 降低波特率：在 Launchpad 里将 Flashing Baud 改为 `460800` 甚至 `115200`
- 换一根更短的 USB 数据线（≤ 1m 为佳）

### ❌ 启动后日志报 `failed to initialize external RAM`

- **根本原因**：你的模组使用的是 Quad PSRAM（型号末尾无 V 后缀），与本固件的 Octal PSRAM 编译配置不兼容
- **解决方案**：更换为 N16R8V 等带 V 后缀的 Octal PSRAM 模组

### ❌ 启动后日志报 `partition table error` 或 `image has invalid magic`

- **根本原因**：Flash 容量 < 16MB
- **解决方案**：更换为至少 16MB Flash 的模组

### ❌ 烧录后运行旧固件（未被覆盖）

- 请在 Flash Download Tool 中先点 **ERASE** 擦除，再 START
- 或在 esptool 中增加 `--erase-all` 参数（注意：会清除 NVS 和所有用户配置）

### ❌ 串口一直输出乱码

- 检查波特率是否为 **115200**
- 若改为 115200 后仍乱码，可能是 Flash Mode/Flash Size 配置错误
- 重新烧录并确认参数：DIO / 80 MHz / 16 MB

---

## 七、固件文件说明

### 目录结构

```
release/
├─ apex_mcp_esp32-s3-v6-audio.bin   # ✅ 合并后的完整固件（烧录到 0x0）
├─ readme_zh.md                      # 本文件（中文版）
├─ readme_en.md                      # 英文版
└─ esp-launchpad/
    ├─ apex-mcp-esp32s3.toml         # ESP Launchpad Publish 模式配置（厂商用）
    └─ README.md                     # TOML 配置使用说明（厂商用）
```

### 固件参数一览

| 参数 | 值 |
|:---|:---|
| 文件名 | `apex_mcp_esp32-s3-v6-audio.bin` |
| 大小 | 约 1.86 MB |
| 烧录地址 | `0x0` |
| Flash Mode | `DIO` |
| Flash Freq | `80 MHz` |
| Flash Size | `16 MB` |
| PSRAM | Octal 8-line, 80 MHz |
| 控制台波特率 | `115200` |
| 分区方案 | OTA 双区（ota_0 / ota_1 各 4 MB）+ SPIFFS storage 8 MB |

---

## 八、获取帮助

- 固件更新、变更记录：请查看对应版本的 Release 说明
- 硬件相关问题：联系模组/开发板供应商确认芯片规格
- 功能使用问题：查阅 APEX MCP 项目文档或联系技术支持
