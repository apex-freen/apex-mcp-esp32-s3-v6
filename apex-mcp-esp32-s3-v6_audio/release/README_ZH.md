# APEX MCP ESP32-S3 V6 音频开发板 · 使用指南

![Version](https://img.shields.io/badge/Firmware-v1.0.0-blue)
![Chip](https://img.shields.io/badge/Chip-ESP32--S3-green)
![Flash](https://img.shields.io/badge/Flash-16MB--minimum-orange)
![PSRAM](https://img.shields.io/badge/PSRAM-Octal%208--line-important)
![DAC](https://img.shields.io/badge/DAC-PCM5102A-blue)
![I2S](https://img.shields.io/badge/Interface-I2S%20Philips-green)

> 本固件面向 APEX MCP ESP32-S3 V6 音频开发板，集成 DLNA DMR 音频播放、SMB 网络流媒体、MQTT 远程控制、OTA 升级等功能。
>
> 🎯 **面向终端用户，零安装即可完成烧录** —— 推荐使用 [ESP Launchpad 网页工具](#二通过-esp-launchpad-在线烧录推荐)。
>
> 🔧 本指南涵盖**固件烧录**与**硬件接线**两部分，按顺序跟做即可完成从拿到固件到播放音频的全过程。

---

## 目录

- [一、硬件要求与清单](#一硬件要求与清单)
- [二、烧录固件](#二烧录固件)
- [三、硬件接线](#三硬件接线)
- [四、烧录后验证](#四烧录后验证)
- [五、故障排查](#五故障排查)
- [六、固件文件说明](#六固件文件说明)

---

## 一、硬件要求与清单

### 1.1 主控板要求

烧录前请确认你的开发板满足以下**全部**硬件条件，否则固件无法正常启动。

| 项目 | 要求 | 说明 |
|:---:|:---|:---|
| **芯片** | ESP32-S3 | ❌ 不兼容 ESP32 / S2 / C3 / C6 / H2 等其它型号 |
| **Flash** | ≥ 16 MB | 分区表已占用 16 MB 空间，8 MB 及以下无法启动 |
| **PSRAM** | **Octal 八线 PSRAM** | ✅ 模组型号末尾带 **V** 后缀：`N16R8V`、`N32R8V` 等<br>❌ 无 V 后缀（Quad PSRAM）会启动失败并报错 `external RAM init failed` |

#### ✅ 兼容模组清单

- `ESP32-S3-WROOM-1 N16R8V`
- `ESP32-S3-WROOM-2 N16R8V`
- `ESP32-S3-DevKitM-1`（M 系列板载 Octal PSRAM）
- `ESP32-S3-DEVKITS V1.2`（V 版本 Octal）

#### ❌ 不兼容模组

- `ESP32-S3-WROOM-1 N16R8`（无 V = Quad PSRAM）
- `ESP32-S3-WROOM-1 N8R8`（Flash 仅 8 MB）
- `ESP32-S3-WROOM-1 N8R2`（Flash 8 MB + PSRAM 2 MB）

### 1.2 硬件清单

| 组件 | 型号 | 说明 |
|:---|:---|:---|
| **主控板** | ESP32-S3-WROOM-1 N16R8V | Octal PSRAM, 16 MB Flash |
| **DAC 模块** | PCM5102A I²S DAC 板（侧边 6 引脚版） | 立体声 24-bit DAC，3.5 mm 耳机口输出 |
| **功放（可选）** | PAM8403 / MAX98357A 等 | PCM5102A 输出为线路电平，**不能直接驱动喇叭** |
| **喇叭** | 4 Ω / 8 Ω 被动喇叭 | 接功放输出 |
| **杜邦线** | 母对母 / 公对母 × 6 根 | 长度建议 ≤ 20 cm，减少高频干扰 |
| **电源** | 5V / 1A USB 供电 | ESP32-S3 与 DAC 模块共用 |

### 1.3 PCM5102A 模块形态说明

本固件配套的 PCM5102A 模块为**侧边 6 引脚版本**：

```
        ┌──────────────────────────┐
        │     PCM5102A 模块       │
        │                          │
        │    [3.5 mm 耳机口]        │
        │                          │
        │    背面：H1L~H4L 跳线    │
        │                          │
[SCK]─┐ │                          │
[BCK]─┤ │                          │
[DIN]─┤ │                          │   ← 侧边 6 引脚
[LRCK]─┤ │                          │
[GND]─┤ │                          │
[VIN]─┘ │                          │
        └──────────────────────────┘
```

- **侧边引脚**：仅 `SCK` `BCK` `DIN` `LRCK` `GND` `VIN` 共 6 个
- **无 XSMT 单独引脚**：静音控制通过背面 H3L 焊接跳线设置
- **音频输出**：3.5 mm 耳机口（无 LOUT/ROUT 侧边引脚）
- **SCK 为侧边引脚**：必须用杜邦线短接到 GND

---

## 二、烧录固件

### 2.1 通过 ESP Launchpad 在线烧录（推荐）

**终端用户首选方式。** 不需要安装任何软件、不需要下载驱动工具包，只需一台装有 Chrome 或 Edge 浏览器的电脑。

#### 准备工作

1. 一根 **USB 数据线**（注意：部分廉价线仅支持充电，无法通信）
2. 一块满足 [硬件要求](#一硬件要求与清单) 的 ESP32-S3 开发板
3. 浏览器：**Chrome 89+** / **Edge 89+** / 任意 Chromium 内核浏览器
   > ❌ Firefox、Safari 不支持 WebSerial API，无法使用。

#### 烧录步骤

**Step 1 · 进入下载模式**

大部分 ESP32-S3 开发板带有自动下载电路，可跳过此步。若后续烧录提示连接失败，请返回执行：

```
① 按住板上 BOOT 按钮不放
② 快速按一下 EN / RST 按钮（复位）
③ 松开 BOOT 按钮
→ 设备已进入下载模式
```

**Step 2 · 打开 ESP Launchpad**

在浏览器中访问：

👉 **https://espressif.github.io/esp-launchpad/**

**Step 3 · 切换到 DIY 模式并加载固件**

1. 页面顶部点击 **DIY** 标签页（不是默认的 Quick Start）
2. **ESP Chipset Type** 下拉选择 **`ESP32-S3`**
3. 点击 **`Add File`** 按钮
4. 选择本目录下的固件文件：**`apex_mcp_esp32-s3-v6-audio.bin`**
5. **Flash Address** 保持默认值：**`0x0`**

> 说明：`apex_mcp_esp32-s3-v6-audio.bin` 为已合并的完整固件，包含 Bootloader、Partition Table、OTA Data 和 App，因此烧录到起始地址 `0x0` 即可。
>
> Flash 工作模式（DIO）、频率（80 MHz）、容量（16 MB）等参数已写入固件文件头部，Launchpad 会自动识别，无需手动设置。

**Step 4 · 连接并烧录**

1. 页面顶部点击 **`Connect`** 按钮
2. 浏览器弹出"选择串口"对话框，选中你的开发板对应的串口设备（通常显示为 `CP210x USB to UART Bridge` 或 `USB-SERIAL CH340`），点击连接
3. **Flashing baud rate** 保持默认 `921600`（若烧录失败可降低为 `460800` 或 `115200`）
4. 点击 **`Flash`** 按钮开始烧录
5. 等待进度条走完（1.86 MB 固件约需 30-60 秒），显示 `Flashing complete` 即完成

**Step 5 · 启动固件**

按一次开发板上的 **EN / RST** 复位按钮，固件开始运行。

---

### 2.2 通过 Flash Download Tool 离线烧录

适用于无网络环境或无法使用浏览器 WebSerial 的场景。使用乐鑫官方 Windows 桌面工具。

#### 下载工具

从乐鑫官网下载 Flash Download Tool：

👉 https://www.espressif.com/en/support/download/other-tools

选择 **Flash Download Tools**（Windows 版，约 30 MB，免安装）。

#### 烧录步骤

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

### 2.3 通过 esptool 命令行烧录（高级用户）

适用于已安装 ESP-IDF 或 Python + esptool 环境的开发者。

#### 环境要求

- Python 3.8+
- 已安装 esptool：`pip install esptool`

#### 烧录命令

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

## 三、硬件接线

烧录固件后，需要把 ESP32-S3 与 PCM5102A DAC 模块按以下方式接线。固件已硬编码引脚分配，**请严格按照本节接线**，否则无声或杂音。

### 3.1 PCM5102A 模块跳线设置（H1L~H4L，关键！）

模块**背面**有 4 组焊接跳线：`H1L`、`H2L`、`H3L`、`H4L`。每组 3 个焊盘（左 / 中 / 右），把中间焊盘焊到某一侧即设定对应电平。

> ⚠️ **不同厂家的模块，"左侧=High"还是"左侧=Low"可能不同**。请以模块背面丝印为准（通常印有 `H` 和 `L` 字样，或 `3.3V` 与 `GND` 字样）。

#### 推荐电平设置（实测验证可用）

| 跳线 | 对应芯片引脚 | 功能 | 电平设置 | 说明 |
|:---:|:---|:---|:---:|:---|
| **H1L** | FLT | 滤波器延迟模式 | **Low** | 正常延迟模式 |
| **H2L** | DEMP | 44.1 kHz 去加重 | **Low** | 关闭去加重 |
| **H3L** | XSMT | 软静音控制 | **High** ⚠️ | **必拉高！否则完全无声** |
| **H4L** | FMT | 音频格式选择 | **Low** | I²S 格式（与本固件 Philips 标准匹配）|

#### ⚠️ H3L 是最常见的"无声"故障原因

由于本模块**没有 XSMT 侧边引脚**，XSMT 电平完全由 H3L 跳线决定。若 H3L 处于 Low，芯片将一直静音——I²S 数据正常输入，但输出端毫无声音。

**解决方法**：把 H3L 的中间焊盘焊到 **High（3.3V）一侧**。

> 不同模块"High 侧"的物理位置可能不同（有的左侧、有的右侧）：
> - 焊接前先用万用表测量两侧焊盘电压
> - 测出 3.3V 的那一侧即为 High 侧
> - 或参考模块背面丝印的 `H` / `3.3V` 标识

#### 实测验证配置（参考）

本固件在以下配置下实测正常工作：

| 跳线 | 实测焊接方向 | 模块丝印对应 |
|:---:|:---|:---|
| H1L | 焊到右侧 | Low（GND） |
| H2L | 焊到右侧 | Low（GND） |
| H3L | 焊到左侧 ⚠️ | High（3.3V） |
| H4L | 焊到右侧 | Low（GND） |

> 以上方向仅作为参考，**请以你模块的实际丝印为准**。

### 3.2 ESP32-S3 与 PCM5102A 接线表

#### 固件已硬编码的 I²S 引脚（[audio_player_shared.h#L34-L36](file:///e:/esp32/apex-mcp-esp32-s3-v6_audio/components/apex_audio/src/audio_player_shared.h#L34-L36)）

| ESP32-S3 GPIO | I²S 功能 | PCM5102A 模块侧边引脚 | 说明 |
|:---:|:---|:---|:---|
| **GPIO 16** | BCK（位时钟） | `BCK` | I²S bit clock |
| **GPIO 17** | WS（字选择） | `LRCK` | 左右声道选择 |
| **GPIO 18** | DOUT（数据输出） | `DIN` | 串行音频数据 |
| —— | MCLK（未使用） | `SCK` | **必须用杜邦线接到 GND** |
| **3.3V / 5V** | 电源 | `VIN` | 模块电源（3.3V 或 5V 见 3.4 节）|
| **GND** | 地 | `GND` | 共地 |

#### 完整接线表（共 6 根线）

| # | ESP32-S3 端 | PCM5102A 模块端 | 说明 |
|:---:|:---|:---|:---|
| 1 | **GPIO 16** | `BCK` | I²S 位时钟 |
| 2 | **GPIO 17** | `LRCK` | I²S 左右选择 |
| 3 | **GPIO 18** | `DIN` | I²S 数据 |
| 4 | **GND** | `SCK` | ⚠️ **SCK 必须接 GND**，启用内部 PLL |
| 5 | **GND** | `GND` | 共地 |
| 6 | **3.3V 或 5V** | `VIN` | 模块电源（见 3.4 节判断）|

> 💡 **接线技巧**：可以把第 4、5 根合并——用一根杜邦线从 ESP32 的 GND 引出，同时接到模块的 `SCK` 和 `GND` 两个引脚（短跳线或双头接）。
>
> H3L 跳线已设置好 High 的话，**不需要再单独接 XSMT 线**。

#### I²S 参数（固件配置，[audio_player_i2s.c#L23-L37](file:///e:/esp32/apex-mcp-esp32-s3-v6_audio/components/apex_audio/src/audio_player_i2s.c#L23-L37)）

| 参数 | 值 |
|:---|:---|
| 采样率 | 44100 Hz |
| 位深 | 16 bit |
| 声道 | 立体声（双声道） |
| I²S 格式 | Philips 标准 |
| MCLK | 不输出（PCM5102A 用内部 PLL 生成） |
| DMA | 12 描述符 × 512 帧 |

### 3.3 接线示意图

```
    ┌─────────────────────────┐                    ┌──────────────────────────┐
    │      ESP32-S3-WROOM     │                    │   PCM5102A 模块          │
    │                         │                    │                          │
    │   GPIO 16  ─────────────┼────────────────────┼─► BCK    │
    │   GPIO 17  ─────────────┼────────────────────┼─► LRCK   │
    │   GPIO 18  ─────────────┼────────────────────┼─► DIN    │
    │                         │                    │                          │
    │   3.3V/5V  ─────────────┼────────────────────┼─► VIN    │
    │   GND      ──────┬──────┼────────────────────┼─► GND    │
    │                 │      │                    │                          │
    │                 └──────┼────────────────────┼─► SCK    │ ← 必须接 GND！
    │                         │                    │                          │
    │                         │                    │   3.5mm 口 ── 有源音箱   │
    └─────────────────────────┘                    └──────────────────────────┘

   (XSMT 通过背面 H3L 跳线设置，无需外接)
```

### 3.4 电源与接地注意事项

#### 供电选择

| 模块 VIN 标识 | 接 ESP32-S3 端 | 说明 |
|:---|:---|:---|
| `VIN` 标 3.3V~5V | 接 **5V** | 模块板载 LDO 降压，5V 推荐 |
| `VIN` 标 3.3V only | 接 **3.3V** | 模块无 LDO，5V 会烧坏 |
| 同时有 `VIN` 和 `3V3` | `VIN` 接 5V，`3V3` 不接 | 用板载 LDO 时让模块自生 3.3V |

> **判断方法**：看模块背面是否有 LDO 稳压芯片（通常丝印 `662K` 或 `ME6211`）。
> - 有 LDO → 接 5V
> - 无 LDO → 接 3.3V

#### 共地（关键）

**所有设备的 GND 必须连在一起**：
- ESP32-S3 的 GND
- PCM5102A 模块的 GND
- 功放模块的 GND
- 电源的 GND

**不共地会导致**：
- 无声
- 严重底噪 / 嗡嗡声
- I²S 信号错位（杂音、断续）

#### 供电能力

- ESP32-S3 启用 WiFi + 音频播放时峰值电流约 400~500 mA
- PCM5102A 自身约 10~20 mA
- 功放（如有）需独立供电或确保 USB 电源 ≥ 2A
- **避免用电脑 USB 口直接驱动喇叭功放**，电流不够会掉电重启

### 3.5 音频输出与功放连接

#### PCM5102A 输出类型

PCM5102A 输出为**线路电平**（约 2.1 Vrms），**不能直接驱动喇叭或耳机**。

| 输出形式 | 是否可直连 | 说明 |
|:---|:---:|:---|
| **喇叭**（4 Ω / 8 Ω） | ❌ | 必须经功放 |
| **耳机**（32 Ω） | ⚠️ | 音量小且可能失真，建议加耳放 |
| **有源音箱**（带内置功放） | ✅ | 用 3.5 mm 音频线从模块耳机口接到音箱 AUX 输入 |
| **功放模块**（PAM8403 / MAX98357A 等） | ✅ | 耳机口 → 3.5mm 转 2pin 线 → 功放 L/R 输入 |

#### 接有源音箱（推荐，最简单）

本模块自带 3.5 mm 耳机插座，直接用一根 3.5 mm 公对公音频线连接：

```
PCM5102A 模块            有源音箱
  3.5mm 耳机口  ──────►  AUX IN（3.5mm 输入）
```

#### 接功放示例（PAM8403，需 3.5mm 转 2pin 线）

```
PCM5102A 模块           3.5mm 转 2pin 线        PAM8403 功放             喇叭
  3.5mm 耳机口  ────►  分出 L / R 两路   ────►  L IN       L OUT  ──────►  喇叭左
                                              R IN       R OUT  ──────►  喇叭右
                                              GND
                                                VIN  ◄──── 5V 独立供电
```

---

## 四、烧录后验证

### 4.1 串口日志检查

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
I (xxx) AUDIO_CORE: I2S 初始化完成：44100Hz 16bit 双声道 (PLL160M 时钟, dma=12x512)
```

关键检查点：
- ✅ 未出现 `external RAM init failed` → PSRAM 正常
- ✅ 未出现 `partition table error` → Flash 容量足够
- ✅ 正常进入 `apex_core` 初始化流程
- ✅ `AUDIO_CORE` 日志显示 I²S 初始化完成

### 4.2 功能测试

固件启动成功后，可按以下项目快速验证：

| 功能 | 验证方法 |
|:---|:---|
| DLNA DMR | 手机端 **网易云音乐** / BubbleUPNP / Windows 媒体播放器 → 能否发现设备并推送音频 |
| Web 配置 | 浏览器访问设备 IP → 能否打开配置页面（默认端口 80） |
| MQTT | 设备是否按预期连接配置的 Broker |

### 4.3 网易云音乐 DLNA 连接与播放

本固件已完整适配网易云音乐的 DLNA DMR，支持以下能力：

| 功能 | 支持 | 说明 |
|:---|:---:|:---|
| 设备发现 | ✅ | 自动出现在投送设备列表中 |
| 推送播放 | ✅ | 支持 MP3 / FLAC / M4A 格式 |
| 暂停 / 恢复 | ✅ | 恢复时从暂停位置继续，不会重头播放 |
| 切歌（上一首/下一首） | ✅ | |
| 音量调节 | ✅ | 通过网易云 App 或设备本地均可调节 |
| 拖动进度条（Seek） | ✅ | 支持跳转到指定位置播放 |

#### 操作步骤

1. 确保手机和 ESP32-S3 开发板连接在**同一个 Wi-Fi 网络**下
2. 打开**网易云音乐 App**（仅限 Android 版，iOS 版不支持 DLNA）
   > 📱 **需要安卓版**：苹果 iOS 版网易云音乐未开放 DLNA 功能
3. 进入任意歌曲播放页面，点击右上角 **「…」** 或 **「投送」** 图标
4. 在弹出的设备列表中，找到名称为 **`APEX-MCP-ESP32S3`** 的设备并点击
5. 连接成功后，网易云界面会显示"正在投送到 APEX-MCP-ESP32S3"
6. 音频将通过开发板的 3.5 mm 耳机口输出（需连接有源音箱或功放）

#### 使用技巧

- **播放进度条**：网易云 App 的进度条会实时显示当前播放位置，可直接拖动跳转
- **音量控制**：使用手机音量键或网易云的音量滑块均可调节设备音量（0-100%）
- **断连重连**：如设备息屏或断网后重新上线，重新在网易云中选择设备即可恢复

---

## 五、故障排查

### 5.1 烧录相关故障

#### ❌ ESP Launchpad 连接时看不到串口

**原因 1**：未安装 USB 转串口驱动。
- **CP210x 驱动**：搜索 `Silicon Labs CP210x USB to UART Bridge Driver`
- **CH340 驱动**：搜索 `CH341SER Windows Driver`

**原因 2**：使用了仅充电的 USB 线。
- 更换标注"数据线"的 USB 线，优先使用 USB 2.0 端口。

#### ❌ 烧录报 `Failed to connect to ESP32-S3`

- 重新执行 [进入下载模式](#step-1--进入下载模式) 步骤
- 降低波特率：在 Launchpad 里将 Flashing Baud 改为 `460800` 甚至 `115200`
- 换一根更短的 USB 数据线（≤ 1m 为佳）

#### ❌ 启动后日志报 `failed to initialize external RAM`

- **根本原因**：你的模组使用的是 Quad PSRAM（型号末尾无 V 后缀），与本固件的 Octal PSRAM 编译配置不兼容
- **解决方案**：更换为 N16R8V 等带 V 后缀的 Octal PSRAM 模组

#### ❌ 启动后日志报 `partition table error` 或 `image has invalid magic`

- **根本原因**：Flash 容量 < 16MB
- **解决方案**：更换为至少 16MB Flash 的模组

#### ❌ 烧录后运行旧固件（未被覆盖）

- 请在 Flash Download Tool 中先点 **ERASE** 擦除，再 START
- 或在 esptool 中增加 `--erase-all` 参数（注意：会清除 NVS 和所有用户配置）

#### ❌ 串口一直输出乱码

- 检查波特率是否为 **115200**
- 若改为 115200 后仍乱码，可能是 Flash Mode/Flash Size 配置错误
- 重新烧录并确认参数：DIO / 80 MHz / 16 MB

### 5.2 接线/音频相关故障

#### ❌ 完全无声

**最常见原因**：H3L 跳线未设置到 High 侧（XSMT 处于静音状态）。

**检查顺序**：
1. ✅ H3L 跳线是否已焊到 High（3.3V）一侧？（本模块无 XSMT 单独引脚，必须靠跳线）
2. ✅ SCK 是否已接到 GND？（侧边引脚，必须短接到 GND）
3. ✅ BCK / LRCK / DIN 三根 I²S 线是否接对引脚？（GPIO 16 / 17 / 18）
4. ✅ 串口日志是否显示 `I2S 初始化完成：44100Hz 16bit 双声道`？
5. ✅ ESP32-S3 与 PCM5102A 的 GND 是否连通？
6. ✅ PCM5102A 的 VIN 是否有电？（万用表测电压）

#### ❌ 有底噪 / 嗡嗡声

- ESP32-S3、DAC、功放、电源的 GND 未共地
- I²S 信号线太长（> 20 cm），改为短线
- USB 电源噪声大，换用带滤波的电源
- WiFi 天线靠近音频线，远离 I²S 信号线

#### ❌ 声音断续 / 杂音

- 供电不足：USB 口输出电流不够，换 2A 电源
- I²S 线接触不良：检查杜邦线是否松动
- 采样率不匹配：固件固定 44.1 kHz，请确认源音频也是 44.1 kHz
- ESP32-S3 进入低功耗模式：检查 `CONFIG_FREERTOS_USE_TICKLESS_IDLE` 是否影响

#### ❌ 只有单声道

- 检查 LRCK 是否接对（GPIO 17）
- 检查功放 L/R 输入是否都接好
- 检查源音频是否本身为单声道

#### ❌ 音量很小

- PCM5102A 输出是线路电平，未经功放直推耳机时音量很小属正常
- 确认是否经功放驱动喇叭
- 检查 `apex_audio` 模块的音量设置（通过 MQTT 或 Web 配置）

#### ❌ 烧录后启动正常但无音频输出

1. 串口检查 `apex_audio` 是否初始化成功：
   ```
   I (xxx) AUDIO_CORE: I2S 初始化完成：44100Hz 16bit 双声道 (PLL160M 时钟, dma=12x512)
   ```
2. 通过 DLNA / Web / MQTT 触发一次播放，串口应看到播放日志
3. 检查 H3L 跳线（XSMT）是否设置正确——本模块无 XSMT 单独引脚，必须靠跳线
4. 检查 SCK 是否已接到 GND（不是悬空）
5. 若使用有源音箱，确认音箱电源已开、音量已调高、3.5 mm 线插紧

---

## 六、固件文件说明

### 目录结构

```
release/
├─ apex_mcp_esp32-s3-v6-audio.bin   # ✅ 合并后的完整固件（烧录到 0x0）
├─ readme_zh.md                      # 本文件（中文版使用指南）
└─ readme_en.md                      # 英文版使用指南
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

### 参考资料

- **PCM5102A 官方 Datasheet**：https://www.ti.com/lit/gpn/pcm5102a
- **固件 I²S 初始化代码**：[audio_player_i2s.c](file:///e:/esp32/apex-mcp-esp32-s3-v6_audio/components/apex_audio/src/audio_player_i2s.c)
- **固件引脚定义**：[audio_player_shared.h](file:///e:/esp32/apex-mcp-esp32-s3-v6_audio/components/apex_audio/src/audio_player_shared.h)

---

## 七、获取帮助

- 固件更新、变更记录：请查看对应版本的 Release 说明
- 硬件相关问题：联系模组/开发板供应商确认芯片规格
- 功能使用问题：查阅 APEX MCP 项目文档或联系技术支持

---

## 附：引脚速查卡（可打印）

```
ESP32-S3          PCM5102A 模块（侧边 6 引脚）
─────────         ─────────────────────────
GPIO 16 ────────► BCK
GPIO 17 ────────► LRCK
GPIO 18 ────────► DIN
3.3V/5V ────────► VIN
GND     ─┬──────► GND
         └──────► SCK   ← 必须接 GND！
```

**3 根 I²S 线 + SCK 接地 + 电源 + 地 = 6 根线**
**H3L 跳线已设到 High（3.3V）→ 无需再接 XSMT**
**音频从 3.5 mm 耳机口输出 → 接有源音箱或功放**
