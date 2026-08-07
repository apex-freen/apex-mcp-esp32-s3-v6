# APEX MCP ESP32-S3 V6 Audio Board · Firmware Flashing Guide

![Version](https://img.shields.io/badge/Firmware-v1.0.0-blue)
![Chip](https://img.shields.io/badge/Chip-ESP32--S3-green)
![Flash](https://img.shields.io/badge/Flash-16MB--minimum-orange)
![PSRAM](https://img.shields.io/badge/PSRAM-Octal%208--line-important)

> Firmware for the APEX MCP ESP32-S3 V6 Audio Board with integrated DLNA DMR playback, SMB network streaming, MQTT remote control, and OTA update capabilities.
>
> 🎯 **End-user friendly with zero installation required** — recommended method: [ESP Launchpad web tool](#method-a-flash-via-esp-launchpad-online-recommended).

---

## Table of Contents

- [1. Hardware Requirements](#1-hardware-requirements)
- [2. Flash via ESP Launchpad (Online, Recommended)](#2-flash-via-esp-launchpad-online-recommended)
- [3. Flash via Flash Download Tool (Offline)](#3-flash-via-flash-download-tool-offline)
- [4. Flash via esptool CLI (Advanced Users)](#4-flash-via-esptool-cli-advanced-users)
- [5. Verify After Flashing](#5-verify-after-flashing)
- [6. Troubleshooting](#6-troubleshooting)
- [7. Firmware File Reference](#7-firmware-file-reference)

---

## 1. Hardware Requirements

Confirm that your board meets **all** of the conditions below before flashing. The firmware will fail to boot on incompatible hardware.

| Item | Requirement | Notes |
|:---:|:---|:---|
| **SoC** | ESP32-S3 | ❌ Not compatible with ESP32 / S2 / C3 / C6 / H2 or any other chip. |
| **Flash** | ≥ 16 MB | Partition table fully occupies 16 MB; 8 MB or smaller will not boot. |
| **PSRAM** | **Octal 8-line PSRAM** | ✅ Module model ending with **V** suffix: `N16R8V`, `N32R8V`, etc.<br>❌ Non-V suffix modules use Quad PSRAM and will fail with `external RAM init failed`. |

### ✅ Compatible Modules

- `ESP32-S3-WROOM-1 N16R8V`
- `ESP32-S3-WROOM-2 N16R8V`
- `ESP32-S3-DevKitM-1` (M-series ships with Octal PSRAM)
- `ESP32-S3-DEVKITS V1.2` (V revision uses Octal PSRAM)

### ❌ Incompatible Modules

- `ESP32-S3-WROOM-1 N16R8` (no V — uses Quad PSRAM)
- `ESP32-S3-WROOM-1 N8R8` (Flash is only 8 MB)
- `ESP32-S3-WROOM-1 N8R2` (8 MB Flash + 2 MB PSRAM)

---

## 2. Flash via ESP Launchpad (Online, Recommended)

**The best option for end users.** No software to install, no driver packages needed — only a computer with Chrome or Edge.

### 2.1 Prerequisites

1. A **USB data cable** (some cheap cables only carry power and will not work for flashing).
2. An ESP32-S3 board meeting the [hardware requirements](#1-hardware-requirements).
3. Browser: **Chrome 89+** / **Edge 89+** / any Chromium-based browser.
   > ❌ Firefox and Safari do not implement the WebSerial API and cannot be used.

### 2.2 Flashing Steps

#### Step 1 · Enter Download Mode

Most ESP32-S3 boards include auto-download circuitry — you can skip this step. If the flashing step fails with a connection error, return here and re-try:

```
① Press and hold the BOOT button on the board.
② Briefly press the EN / RST (reset) button.
③ Release the BOOT button.
→ Device is now in download mode.
```

#### Step 2 · Open ESP Launchpad

Visit the following URL in your browser:

👉 **https://espressif.github.io/esp-launchpad/**

#### Step 3 · Switch to DIY Mode and Load Firmware

1. Click the **DIY** tab at the top of the page (not the default Quick Start).
2. From the **ESP Chipset Type** dropdown, select **`ESP32-S3`**.
3. Click the **`Add File`** button.
4. Select the firmware file in this directory: **`apex_mcp_esp32-s3-v6-audio.bin`**.
5. Keep the **Flash Address** at its default value: **`0x0`**.

> Note: `apex_mcp_esp32-s3-v6-audio.bin` is a pre-merged full firmware image containing Bootloader, Partition Table, OTA Data, and the Application. Flashing it to `0x0` covers everything.
>
> Flash mode (DIO), frequency (80 MHz), and size (16 MB) are encoded in the firmware image header and automatically recognized by Launchpad — no manual configuration required.

#### Step 4 · Connect and Flash

1. Click the **`Connect`** button at the top of the page.
2. In the browser's "Select a serial port" dialog, pick the serial device corresponding to your board (typically shown as `CP210x USB to UART Bridge` or `USB-SERIAL CH340`), and click Connect.
3. Keep **Flashing baud rate** at the default `921600` (drop to `460800` or `115200` if flashing fails).
4. Click the **`Flash`** button to begin.
5. Wait for the progress bar to complete (a 1.86 MB firmware takes roughly 30-60 seconds). `Flashing complete` indicates success.

#### Step 5 · Boot the Firmware

Press the **EN / RST** reset button on the board once. The firmware will now start up.

---

## 3. Flash via Flash Download Tool (Offline)

For use in air-gapped environments or on machines where browser WebSerial is unavailable. Uses Espressif's official Windows desktop tool.

### 3.1 Download the Tool

Get Flash Download Tool from the Espressif website:

👉 https://www.espressif.com/en/support/download/other-tools

Select **Flash Download Tools** (Windows build, approx. 30 MB, no installation required).

### 3.2 Flashing Steps

1. Extract and run `flash_download_tool_x.x.x.exe`.
2. On the initial selection screen:
   - **ChipType**: `ESP32-S3`
   - **WorkMode**: `Develop`
   - **LoadMode**: `UART`
   - Click **OK**.
3. On the SPI Download page, configure the flash table:

   | # | Check | File | Address |
   |---|:---:|:---|:---:|
   | 1 | ☑ | `apex_mcp_esp32-s3-v6-audio.bin` | `0x0` |

4. Configure the options in the bottom-right panel:
   - **SPI SPEED**: `80 MHz`
   - **SPI MODE**: `DIO`
   - **FLASH SIZE**: `16 MB`
   - **BAUD**: `921600`
   - **COM**: Select the serial port corresponding to your board.
5. First click **ERASE** to wipe the flash chip (takes ~10 seconds).
6. After erasure completes, click **START** to begin flashing.
7. `FINISH` indicates success — press RST to boot.

---

## 4. Flash via esptool CLI (Advanced Users)

For developers who already have ESP-IDF or Python + esptool installed.

### 4.1 Requirements

- Python 3.8+
- esptool installed: `pip install esptool`

### 4.2 Command

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

> Replace `COM5` (Windows) or `/dev/ttyUSB0` (Linux/macOS) with your actual serial port.

---

## 5. Verify After Flashing

### 5.1 Check the Serial Log

Use any serial monitor of your choice (PuTTY, MobaXterm, Arduino IDE, VS Code Serial Monitor, etc.):

| Parameter | Value |
|:---|:---|
| Baud rate | 115200 |
| Data bits | 8 |
| Stop bits | 1 |
| Parity | None |

After pressing RST, a successful boot log begins with lines similar to:

```
I (29) boot: ESP-IDF v5.x 2nd stage bootloader
I (xxx) boot:  ... chip revision 0.x
I (xxx) esp_psram: Found 8MB PSRAM
I (xxx) apex_core: APEX MCP system initialized
I (xxx) apex_network: WiFi connecting to SSID: ...
```

Key checkpoints:
- ✅ No `external RAM init failed` → PSRAM is OK
- ✅ No `partition table error` → Flash size is sufficient
- ✅ The `apex_core` initialization sequence runs normally

### 5.2 Functional Smoke Test

Once booted, quickly validate the major features:

| Feature | How to Verify |
|:---|:---|
| DLNA DMR | Use BubbleUPnP (mobile) or Windows Media Player → device should be discoverable and accept audio push |
| Web Config | Visit the device IP in a browser → config page should load (default port 80) |
| MQTT | Device connects to the configured broker |

---

## 6. Troubleshooting

### ❌ No serial port appears when clicking Connect in Launchpad

**Cause 1**: The USB-to-serial driver is not installed.
- **CP210x driver**: Search for `Silicon Labs CP210x USB to UART Bridge Driver`
- **CH340 driver**: Search for `CH341SER Windows Driver`

**Cause 2**: You are using a charge-only USB cable.
- Swap to a cable explicitly rated for data; prefer a USB 2.0 port.

### ❌ Flashing reports `Failed to connect to ESP32-S3`

- Re-run the [enter download mode](#step-1--enter-download-mode) procedure.
- Reduce the baud rate: in Launchpad, change Flashing Baud to `460800` or even `115200`.
- Try a shorter USB cable (≤ 1 m / 3 ft recommended).

### ❌ Serial log after boot shows `failed to initialize external RAM`

- **Root cause**: Your module uses Quad PSRAM (model has no V suffix), which is incompatible with this firmware's Octal PSRAM build configuration.
- **Resolution**: Swap to an Octal PSRAM module such as N16R8V.

### ❌ Serial log shows `partition table error` or `image has invalid magic`

- **Root cause**: Flash on the module is smaller than 16 MB.
- **Resolution**: Use a module with at least 16 MB of flash.

### ❌ Old firmware still runs after flashing (new image not written)

- In Flash Download Tool, click **ERASE** first to fully wipe the flash before clicking **START**.
- Or, with esptool, add the `--erase-all` flag (note: this also clears NVS and any saved user configuration).

### ❌ Serial output is garbage / unreadable text

- Verify the baud rate is set to **115200**.
- If still garbled at 115200, the Flash Mode / Flash Size parameters are probably wrong.
- Re-flash and confirm: DIO / 80 MHz / 16 MB.

---

## 7. Firmware File Reference

### Directory Layout

```
release/
├─ apex_mcp_esp32-s3-v6-audio.bin   # ✅ Pre-merged full firmware (flash at 0x0)
├─ readme_zh.md                      # Chinese version of this document
├─ readme_en.md                      # This file (English)
└─ esp-launchpad/
    ├─ apex-mcp-esp32s3.toml         # ESP Launchpad Publish config (for vendors)
    └─ README.md                     # TOML config usage guide (for vendors)
```

### Firmware Parameters

| Parameter | Value |
|:---|:---|
| File name | `apex_mcp_esp32-s3-v6-audio.bin` |
| Size | ~ 1.86 MB |
| Flash address | `0x0` |
| Flash Mode | `DIO` |
| Flash Freq | `80 MHz` |
| Flash Size | `16 MB` |
| PSRAM | Octal 8-line, 80 MHz |
| Console baud rate | `115200` |
| Partition layout | Dual OTA (ota_0 / ota_1: 4 MB each) + SPIFFS storage: 8 MB |

---

## 8. Getting Help

- For firmware updates and changelogs, see the Release Notes for the corresponding version.
- For hardware questions, contact your module/board vendor to verify chip specifications.
- For feature usage or integration questions, refer to the APEX MCP project documentation or contact technical support.
