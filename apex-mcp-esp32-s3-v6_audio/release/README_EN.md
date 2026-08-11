# APEX MCP ESP32-S3 V6 Audio Board · User Guide

![Version](https://img.shields.io/badge/Firmware-v1.0.0-blue)
![Chip](https://img.shields.io/badge/Chip-ESP32--S3-green)
![Flash](https://img.shields.io/badge/Flash-16MB--minimum-orange)
![PSRAM](https://img.shields.io/badge/PSRAM-Octal%208--line-important)
![DAC](https://img.shields.io/badge/DAC-PCM5102A-blue)
![I2S](https://img.shields.io/badge/Interface-I2S%20Philips-green)

> This firmware targets the APEX MCP ESP32-S3 V6 audio board, integrating DLNA DMR audio playback, SMB network streaming, MQTT remote control, and OTA upgrades.
>
> 🎯 **Designed for end users — zero-install flashing** — recommended via [ESP Launchpad web tool](#21-via-esp-launchpad-online-flashing-recommended).
>
> 🔧 This guide covers both **firmware flashing** and **hardware wiring**. Follow it in order to go from receiving the firmware to playing audio.

---

## Table of Contents

- [1. Hardware Requirements & Bill of Materials](#1-hardware-requirements--bill-of-materials)
- [2. Flashing Firmware](#2-flashing-firmware)
- [3. Hardware Wiring](#3-hardware-wiring)
- [4. Post-Flash Verification](#4-post-flash-verification)
- [5. Troubleshooting](#5-troubleshooting)
- [6. Firmware Files](#6-firmware-files)

---

## 1. Hardware Requirements & Bill of Materials

### 1.1 Main Board Requirements

Before flashing, confirm your development board meets **all** of the following hardware conditions, otherwise the firmware will not boot.

| Item | Requirement | Notes |
|:---:|:---|:---|
| **Chip** | ESP32-S3 | ❌ Not compatible with ESP32 / S2 / C3 / C6 / H2 etc. |
| **Flash** | ≥ 16 MB | Partition table occupies 16 MB; 8 MB and below will not boot |
| **PSRAM** | **Octal 8-line PSRAM** | ✅ Module model ends with **V** suffix: `N16R8V`, `N32R8V` etc.<br>❌ Without V suffix (Quad PSRAM) fails to boot with `external RAM init failed` |

#### ✅ Compatible Modules

- `ESP32-S3-WROOM-1 N16R8V`
- `ESP32-S3-WROOM-2 N16R8V`
- `ESP32-S3-DevKitM-1` (M series with Octal PSRAM)
- `ESP32-S3-DEVKITS V1.2` (V version Octal)

#### ❌ Incompatible Modules

- `ESP32-S3-WROOM-1 N16R8` (no V = Quad PSRAM)
- `ESP32-S3-WROOM-1 N8R8` (Flash only 8 MB)
- `ESP32-S3-WROOM-1 N8R2` (Flash 8 MB + PSRAM 2 MB)

### 1.2 Hardware Bill of Materials

| Component | Model | Notes |
|:---|:---|:---|
| **Main board** | ESP32-S3-WROOM-1 N16R8V | Octal PSRAM, 16 MB Flash |
| **DAC module** | PCM5102A I²S DAC board (side 6-pin version) | Stereo 24-bit DAC, 3.5 mm jack output |
| **Amplifier (optional)** | PAM8403 / MAX98357A etc. | PCM5102A output is line-level, **cannot drive speakers directly** |
| **Speaker** | 4 Ω / 8 Ω passive speaker | Connects to amplifier output |
| **Dupont wires** | Female-to-female / male-to-female × 6 | Recommended length ≤ 20 cm to reduce HF interference |
| **Power supply** | 5V / 1A USB | Shared supply for ESP32-S3 and DAC |

### 1.3 PCM5102A Module Form Factor

The PCM5102A module paired with this firmware is the **side 6-pin version**:

```
        ┌──────────────────────────┐
        │     PCM5102A Module      │
        │                          │
        │    [3.5 mm headphone]    │
        │                          │
        │    Back: H1L~H4L jumpers │
        │                          │
[SCK]─┐ │                          │
[BCK]─┤ │                          │
[DIN]─┤ │                          │   ← Side 6 pins
[LRCK]─┤ │                          │
[GND]─┤ │                          │
[VIN]─┘ │                          │
        └──────────────────────────┘
```

- **Side pins**: only `SCK` `BCK` `DIN` `LRCK` `GND` `VIN` (6 total)
- **No dedicated XSMT pin**: mute control is set via H3L solder jumper on the back
- **Audio output**: 3.5 mm headphone jack (no LOUT/ROUT side pins)
- **SCK is a side pin**: must be shorted to GND with a Dupont jumper

---

## 2. Flashing Firmware

### 2.1 Via ESP Launchpad Online (Recommended)

**Preferred method for end users.** No software installation, no driver toolkits needed — just a computer with Chrome or Edge browser.

#### Prerequisites

1. A **USB data cable** (note: some cheap cables are charge-only and cannot communicate)
2. An ESP32-S3 development board meeting [hardware requirements](#1-hardware-requirements--bill-of-materials)
3. Browser: **Chrome 89+** / **Edge 89+** / any Chromium-based browser
   > ❌ Firefox and Safari do not support WebSerial API.

#### Flashing Steps

**Step 1 · Enter Download Mode**

Most ESP32-S3 boards include auto-download circuitry — you can skip this step. If the flashing step fails with a connection error, return here and re-try:

```
① Press and hold the BOOT button on the board.
② Briefly press the EN / RST (reset) button.
③ Release the BOOT button.
→ Device is now in download mode.
```

**Step 2 · Open ESP Launchpad**

Visit the following URL in your browser:

👉 **https://espressif.github.io/esp-launchpad/**

**Step 3 · Switch to DIY Mode and Load Firmware**

1. Click the **DIY** tab at the top of the page (not the default Quick Start).
2. From the **ESP Chipset Type** dropdown, select **`ESP32-S3`**.
3. Click the **`Add File`** button.
4. Select the firmware file in this directory: **`apex_mcp_esp32-s3-v6-audio.bin`**.
5. Keep the **Flash Address** at its default value: **`0x0`**.

> Note: `apex_mcp_esp32-s3-v6-audio.bin` is a pre-merged full firmware image containing Bootloader, Partition Table, OTA Data, and the Application. Flashing it to `0x0` covers everything.
>
> Flash mode (DIO), frequency (80 MHz), and size (16 MB) are encoded in the firmware image header and automatically recognized by Launchpad — no manual configuration required.

**Step 4 · Connect and Flash**

1. Click the **`Connect`** button at the top of the page.
2. In the browser's "Select a serial port" dialog, pick the serial device corresponding to your board (typically shown as `CP210x USB to UART Bridge` or `USB-SERIAL CH340`), and click Connect.
3. Keep **Flashing baud rate** at the default `921600` (drop to `460800` or `115200` if flashing fails).
4. Click the **`Flash`** button to begin.
5. Wait for the progress bar to complete (a 1.86 MB firmware takes roughly 30-60 seconds). `Flashing complete` indicates success.

**Step 5 · Boot the Firmware**

Press the **EN / RST** reset button on the board once. The firmware will now start up.

---

### 2.2 Via Flash Download Tool (Offline)

For environments without network or where browser WebSerial is unavailable. Uses Espressif's official Windows desktop tool.

#### Download the Tool

Download Flash Download Tool from Espressif's official site:

👉 https://www.espressif.com/en/support/download/other-tools

Select **Flash Download Tools** (Windows version, ~30 MB, no installation required).

#### Flashing Steps

1. Extract and run `flash_download_tool_x.x.x.exe`
2. On the selection page:
   - **ChipType**: `ESP32-S3`
   - **WorkMode**: `Develop`
   - **LoadMode**: `UART`
   - Click **OK**
3. On the SPI download page, configure:
   | # | Check | File | Address |
   |---|:---:|:---|:---:|
   | 1 | ☑ | `apex_mcp_esp32-s3-v6-audio.bin` | `0x0` |
4. Bottom-right configuration:
   - **SPI SPEED**: `80 MHz`
   - **SPI MODE**: `DIO`
   - **FLASH SIZE**: `16 MB`
   - **BAUD**: `921600`
   - **COM**: select your board's serial port
5. Click **ERASE** first to erase the entire flash (about 10 seconds)
6. After erasing, click **START** to begin flashing
7. `FINISH` indicates completion — press RST to boot

---

### 2.3 Via esptool Command Line (Advanced Users)

For developers with ESP-IDF or Python + esptool environment installed.

#### Requirements

- Python 3.8+
- esptool installed: `pip install esptool`

#### Flashing Commands

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

## 3. Hardware Wiring

After flashing, wire the ESP32-S3 to the PCM5102A DAC module as follows. The firmware hard-codes the pin assignment — **please follow this section exactly**, otherwise you'll get no audio or noise.

### 3.1 PCM5102A Module Solder Jumpers (H1L~H4L, Critical!)

The **back** of the module has 4 solder jumper groups: `H1L`, `H2L`, `H3L`, `H4L`. Each group has 3 pads (left / center / right). Solder the center pad to one side to set the corresponding logic level.

> ⚠️ **Different module vendors may map "left = High" or "left = Low" differently.** Refer to the silkscreen on the back of your module (often marked with `H` and `L`, or `3.3V` and `GND`).

#### Recommended Logic Levels (verified working)

| Jumper | Chip pin | Function | Level | Notes |
|:---:|:---|:---|:---:|:---|
| **H1L** | FLT | Filter delay mode | **Low** | Normal delay mode |
| **H2L** | DEMP | 44.1 kHz de-emphasis | **Low** | De-emphasis off |
| **H3L** | XSMT | Soft mute control | **High** ⚠️ | **Must be high! Otherwise silent** |
| **H4L** | FMT | Audio format select | **Low** | I²S format (matches firmware's Philips standard) |

#### ⚠️ H3L is the most common cause of "no sound"

Because this module has **no dedicated XSMT side pin**, the XSMT level is entirely determined by the H3L jumper. If H3L is Low, the chip is permanently muted — I²S data is received correctly but no audio appears at the output.

**Solution**: Solder H3L's center pad to the **High (3.3V) side**.

> The physical "High side" may be left or right depending on the module:
> - Measure both side pads with a multimeter before soldering
> - The side that reads 3.3V is the High side
> - Or refer to the `H` / `3.3V` silkscreen on the back of the module

#### Verified Configuration (for reference)

This firmware has been verified working with the following configuration:

| Jumper | Solder direction | Module silkscreen |
|:---:|:---|:---|
| H1L | Right side | Low (GND) |
| H2L | Right side | Low (GND) |
| H3L | Left side ⚠️ | High (3.3V) |
| H4L | Right side | Low (GND) |

> The above direction is for reference only — **please refer to your own module's silkscreen**.

### 3.2 ESP32-S3 ↔ PCM5102A Wiring Table

#### Firmware hard-coded I²S pins ([audio_player_shared.h#L34-L36](file:///e:/esp32/apex-mcp-esp32-s3-v6_audio/components/apex_audio/src/audio_player_shared.h#L34-L36))

| ESP32-S3 GPIO | I²S function | PCM5102A module side pin | Notes |
|:---:|:---|:---|:---|
| **GPIO 16** | BCK (bit clock) | `BCK` | I²S bit clock |
| **GPIO 17** | WS (word select) | `LRCK` | Left/right channel select |
| **GPIO 18** | DOUT (data out) | `DIN` | Serial audio data |
| —— | MCLK (unused) | `SCK` | **Must be wired to GND** |
| **3.3V / 5V** | Power | `VIN` | Module power (3.3V or 5V — see 3.4) |
| **GND** | Ground | `GND` | Common ground |

#### Full wiring table (6 wires total)

| # | ESP32-S3 side | PCM5102A module side | Notes |
|:---:|:---|:---|:---|
| 1 | **GPIO 16** | `BCK` | I²S bit clock |
| 2 | **GPIO 17** | `LRCK` | I²S word select |
| 3 | **GPIO 18** | `DIN` | I²S data |
| 4 | **GND** | `SCK` | ⚠️ **SCK must be tied to GND** to enable internal PLL |
| 5 | **GND** | `GND` | Common ground |
| 6 | **3.3V or 5V** | `VIN` | Module power (see section 3.4) |

> 💡 **Wiring tip**: You can merge wires #4 and #5 — run a single wire from ESP32's GND to both `SCK` and `GND` on the module (via a Y-jumper or short jumper).
>
> If the H3L jumper is already set to High, **no separate XSMT wire is needed**.

#### I²S Parameters (firmware config, [audio_player_i2s.c#L23-L37](file:///e:/esp32/apex-mcp-esp32-s3-v6_audio/components/apex_audio/src/audio_player_i2s.c#L23-L37))

| Parameter | Value |
|:---|:---|
| Sample rate | 44100 Hz |
| Bit depth | 16 bit |
| Channels | Stereo |
| I²S format | Philips standard |
| MCLK | Not output (PCM5102A uses internal PLL) |
| DMA | 12 descriptors × 512 frames |

### 3.3 Wiring Diagram

```
    ┌─────────────────────────┐                    ┌──────────────────────────┐
    │      ESP32-S3-WROOM     │                    │   PCM5102A module        │
    │                         │                    │                          │
    │   GPIO 16  ─────────────┼────────────────────┼─► BCK    │
    │   GPIO 17  ─────────────┼────────────────────┼─► LRCK   │
    │   GPIO 18  ─────────────┼────────────────────┼─► DIN    │
    │                         │                    │                          │
    │   3.3V/5V  ─────────────┼────────────────────┼─► VIN    │
    │   GND      ──────┬──────┼────────────────────┼─► GND    │
    │                 │      │                    │                          │
    │                 └──────┼────────────────────┼─► SCK    │ ← must be GND!
    │                         │                    │                          │
    │                         │                    │   3.5mm jack ── active spk│
    └─────────────────────────┘                    └──────────────────────────┘

   (XSMT is set via H3L jumper on the back — no external wire needed)
```

### 3.4 Power & Grounding Notes

#### Power Supply Selection

| Module VIN label | ESP32-S3 side | Notes |
|:---|:---|:---|
| `VIN` labeled 3.3V~5V | Use **5V** | Module has onboard LDO; 5V recommended |
| `VIN` labeled 3.3V only | Use **3.3V** | No LDO; 5V will damage the module |
| Both `VIN` and `3V3` present | `VIN` ← 5V, leave `3V3` floating | Let module's LDO generate 3.3V |

> **How to tell**: Inspect the back of the module for an LDO regulator (usually silkscreened `662K` or `ME6211`).
> - Has LDO → use 5V
> - No LDO → use 3.3V

#### Common Ground (Critical)

**The GND of every device must be connected together**:
- ESP32-S3 GND
- PCM5102A module GND
- Amplifier GND
- Power supply GND

**Failure to common-ground causes**:
- No audio
- Severe hum / buzzing
- I²S signal misalignment (noise, stuttering)

#### Power Capacity

- ESP32-S3 with Wi-Fi + audio playback peaks around 400~500 mA
- PCM5102A itself draws about 10~20 mA
- If using an amplifier, ensure the USB power supply is ≥ 2A, or use a dedicated supply for the amp
- **Avoid driving a speaker amplifier directly from a computer's USB port** — insufficient current can cause brownout resets

### 3.5 Audio Output & Amplifier Connection

#### PCM5102A Output Type

PCM5102A output is **line-level** (~2.1 Vrms) and **cannot directly drive speakers or headphones**.

| Output target | Direct connect? | Notes |
|:---|:---:|:---|
| **Passive speaker** (4 Ω / 8 Ω) | ❌ | Requires amplifier |
| **Headphones** (32 Ω) | ⚠️ | Low volume, possible distortion; add a headphone amp |
| **Active speaker** (built-in amp) | ✅ | Use a 3.5 mm audio cable from the module's jack to AUX input |
| **Amplifier module** (PAM8403 / MAX98357A etc.) | ✅ | Jack → 3.5 mm to 2-pin cable → amp L/R input |

#### Connecting an active speaker (recommended, simplest)

This module has an onboard 3.5 mm headphone jack — use a standard 3.5 mm male-to-male audio cable:

```
PCM5102A module           Active speaker
  3.5 mm jack    ──────►  AUX IN (3.5 mm input)
```

#### Example: Connecting a PAM8403 amplifier (requires 3.5 mm to 2-pin cable)

```
PCM5102A module        3.5 mm to 2-pin cable      PAM8403 amp              Speaker
  3.5 mm jack   ────►  splits to L / R     ────►  L IN       L OUT  ──────►  Left speaker
                                               R IN       R OUT  ──────►  Right speaker
                                               GND
                                                 VIN  ◄──── 5V dedicated supply
```

---

## 4. Post-Flash Verification

### 4.1 Serial Log Check

Use any serial monitor (PuTTY, MobaXterm, Arduino IDE, VS Code Serial Monitor):

| Parameter | Value |
|:---|:---|
| Baud rate | 115200 |
| Data bits | 8 |
| Stop bits | 1 |
| Parity | None |

After pressing RST, the normal boot log should start with something like:

```
I (29) boot: ESP-IDF v5.x 2nd stage bootloader
I (xxx) boot:  ... chip revision 0.x
I (xxx) esp_psram: Found 8MB PSRAM
I (xxx) apex_core: APEX MCP system initialized
I (xxx) apex_network: WiFi connecting to SSID: ...
I (xxx) AUDIO_CORE: I2S 初始化完成：44100Hz 16bit 双声道 (PLL160M 时钟, dma=12x512)
```

Key checkpoints:
- ✅ No `external RAM init failed` → PSRAM OK
- ✅ No `partition table error` → Flash capacity sufficient
- ✅ Normal entry into `apex_core` initialization
- ✅ `AUDIO_CORE` log shows I²S initialization complete

### 4.2 Functional Testing

After the firmware boots successfully, quickly verify these items:

| Function | Verification method |
|:---|:---|
| DLNA DMR | **NetEase Cloud Music** / BubbleUPNP on phone / Windows Media Player → can you discover the device and push audio |
| Web config | Browser to device IP → can you open the config page (default port 80) |
| MQTT | Does the device connect to the configured Broker as expected |

### 4.3 NetEase Cloud Music DLNA Connection & Playback

This firmware has full DLNA DMR support for NetEase Cloud Music, with the following capabilities:

| Feature | Supported | Notes |
|:---|:---:|:---|
| Device Discovery | ✅ | Automatically appears in the cast device list |
| Push Playback | ✅ | Supports MP3 / FLAC / M4A formats |
| Pause / Resume | ✅ | Resumes from the pause point, not from the beginning |
| Track Skip (Prev/Next) | ✅ | |
| Volume Control | ✅ | Adjustable via NetEase app or device locally |
| Seek (Drag Progress Bar) | ✅ | Supports jumping to a specific position |

#### Steps

1. Ensure your phone and ESP32-S3 board are on the **same Wi-Fi network**
2. Open the **NetEase Cloud Music app** (Android only — iOS does not support DLNA)
   > 📱 **Android required**: The iOS version of NetEase Cloud Music does not expose DLNA functionality
3. Go to any song's playback screen, tap the **「…」** or **「Cast」** icon in the top-right corner
4. In the device list that appears, find and select the device named **`APEX-MCP-ESP32S3`**
5. Once connected, NetEase will show "Casting to APEX-MCP-ESP32S3"
6. Audio will output through the board's 3.5 mm headphone jack (requires powered speakers or amplifier)

#### Tips

- **Progress bar**: NetEase's progress bar shows real-time playback position; you can drag it to seek
- **Volume control**: Use your phone's volume keys or NetEase's volume slider to adjust device volume (0–100%)
- **Reconnection**: If the device goes offline and comes back, simply reselect it in NetEase to resume

---

## 5. Troubleshooting

### 5.1 Flashing-Related Issues

#### ❌ No serial port visible when connecting via ESP Launchpad

**Cause 1**: USB-to-serial driver not installed.
- **CP210x driver**: search for `Silicon Labs CP210x USB to UART Bridge Driver`
- **CH340 driver**: search for `CH341SER Windows Driver`

**Cause 2**: Using a charge-only USB cable.
- Replace with a cable labeled "data cable"; prefer USB 2.0 ports.

#### ❌ Flashing reports `Failed to connect to ESP32-S3`

- Re-do the [Enter Download Mode](#step-1--enter-download-mode) steps
- Lower baud rate: in Launchpad, change Flashing Baud to `460800` or even `115200`
- Try a shorter USB data cable (≤ 1m recommended)

#### ❌ Boot log reports `failed to initialize external RAM`

- **Root cause**: Your module uses Quad PSRAM (model has no V suffix), incompatible with this firmware's Octal PSRAM build config
- **Solution**: Replace with an Octal PSRAM module like N16R8V (with V suffix)

#### ❌ Boot log reports `partition table error` or `image has invalid magic`

- **Root cause**: Flash capacity < 16MB
- **Solution**: Replace with a module with at least 16MB Flash

#### ❌ Old firmware still running after flash (not overwritten)

- In Flash Download Tool, click **ERASE** first, then START
- Or add `--erase-all` to esptool (note: this clears NVS and all user config)

#### ❌ Serial output is all garbled

- Check that baud rate is **115200**
- If still garbled at 115200, Flash Mode/Flash Size may be misconfigured
- Re-flash and confirm parameters: DIO / 80 MHz / 16 MB

### 5.2 Wiring/Audio-Related Issues

#### ❌ Completely silent

**Most common cause**: H3L jumper not set to High (XSMT is in muted state).

**Check in order**:
1. ✅ Is H3L jumper soldered to the High (3.3V) side? (This module has no XSMT side pin — must use the jumper)
2. ✅ Is SCK wired to GND? (Side pin — must be shorted to GND)
3. ✅ Are BCK / LRCK / DIN wired to the correct pins? (GPIO 16 / 17 / 18)
4. ✅ Does the serial log show `I2S 初始化完成：44100Hz 16bit 双声道`?
5. ✅ Is GND common between ESP32-S3 and PCM5102A?
6. ✅ Does PCM5102A's VIN have power? (Measure with multimeter)

#### ❌ Hiss / buzzing background noise

- GND not common across ESP32-S3, DAC, amp, and supply
- I²S signal wires too long (> 20 cm) — use shorter wires
- Noisy USB power — use a filtered power supply
- Wi-Fi antenna near audio lines — keep I²S wires away from the antenna

#### ❌ Audio stuttering / noise bursts

- Insufficient power: USB port cannot deliver enough current — use a 2A supply
- Loose I²S wires: check Dupont connections
- Sample rate mismatch: firmware is fixed at 44.1 kHz, verify the source audio
- ESP32-S3 entering light-sleep: check `CONFIG_FREERTOS_USE_TICKLESS_IDLE`

#### ❌ Only one channel playing

- Check that LRCK is on GPIO 17
- Check that both L/R inputs on the amp are wired
- Check that the source audio itself is not mono

#### ❌ Very low volume

- PCM5102A output is line-level; direct headphone drive will be quiet — this is expected
- Verify you are using an amplifier for speakers
- Check the `apex_audio` module's volume setting (via MQTT or Web config)

#### ❌ Boots normally but no audio output

1. Check serial log for `apex_audio` initialization:
   ```
   I (xxx) AUDIO_CORE: I2S 初始化完成：44100Hz 16bit 双声道 (PLL160M 时钟, dma=12x512)
   ```
2. Trigger playback via DLNA / Web / MQTT — serial log should show playback activity
3. Check the H3L jumper (XSMT) — this module has no dedicated XSMT pin, must use the jumper
4. Check that SCK is wired to GND (not floating)
5. If using an active speaker, verify it's powered on, volume is up, and the 3.5 mm cable is fully inserted

---

## 6. Firmware Files

### Directory Structure

```
release/
├─ apex_mcp_esp32-s3-v6-audio.bin   # ✅ Pre-merged full firmware (flash to 0x0)
├─ readme_zh.md                      # This file (Chinese version)
└─ readme_en.md                      # English version
```

### Firmware Parameters

| Parameter | Value |
|:---|:---|
| Filename | `apex_mcp_esp32-s3-v6-audio.bin` |
| Size | ~1.86 MB |
| Flash address | `0x0` |
| Flash Mode | `DIO` |
| Flash Freq | `80 MHz` |
| Flash Size | `16 MB` |
| PSRAM | Octal 8-line, 80 MHz |
| Console baud rate | `115200` |
| Partition scheme | OTA dual-slot (ota_0 / ota_1, 4 MB each) + SPIFFS storage 8 MB |

### References

- **PCM5102A official datasheet**: https://www.ti.com/lit/gpn/pcm5102a
- **Firmware I²S init code**: [audio_player_i2s.c](file:///e:/esp32/apex-mcp-esp32-s3-v6_audio/components/apex_audio/src/audio_player_i2s.c)
- **Firmware pin definitions**: [audio_player_shared.h](file:///e:/esp32/apex-mcp-esp32-s3-v6_audio/components/apex_audio/src/audio_player_shared.h)

---

## 7. Getting Help

- Firmware updates and changelog: see the corresponding Release notes
- Hardware-related issues: contact the module/board supplier to confirm chip specs
- Functional issues: consult the APEX MCP project docs or contact technical support

---

## Appendix: Quick Reference Card (printable)

```
ESP32-S3          PCM5102A module (side 6 pins)
─────────         ─────────────────────────
GPIO 16 ────────► BCK
GPIO 17 ────────► LRCK
GPIO 18 ────────► DIN
3.3V/5V ────────► VIN
GND     ─┬──────► GND
         └──────► SCK   ← must be GND!
```

**3 I²S wires + SCK-to-GND + power + ground = 6 wires**
**H3L jumper set to High (3.3V) → no separate XSMT wire needed**
**Audio output via 3.5 mm jack → connect to active speaker or amplifier**
