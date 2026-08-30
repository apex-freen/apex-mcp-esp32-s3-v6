# APEX × 实战派硬件接入设计

> 目标：把立创实战派（ESP32-S3）板载外设逐个接入 APEX 框架，暴露为 MCP 工具。
> 现状：框架 M1~M5 已完成并编译通过；已接入 attitudeGet / sdList / sdRead / sdWrite / ledBlink。
> 本文件为设计稿：先列全量功能清单 → 逐个实现 → 每完成一个标记 ✅。

---

## 一、板载硬件能力清单（实战派）

| # | 外设 | 型号/规格 | 接口 | 状态 |
| --- | --- | --- | --- | --- |
| 1 | 六轴 IMU | QMI8658 | I2C0 @0x6A | ✅ attitudeGet |
| 2 | SD 卡 | SDMMC 1 线 | CLK47/CMD48/D0 21 | ✅ sdList/sdRead/sdWrite |
| 3 | LCD | ST7789 320×240 SPI | MOSI40/SCLK41/DC39/BL42/CS(PCA9557) | ⏸️ 待实现 |
| 4 | 触摸屏 | FT5x06 | I2C0 | ⏸️ 待实现 |
| 5 | 摄像头 | OV2640 8bit DVP | XCLK5/PCLK7/VSYNC3/HREF46/D0~D7 | ⏸️ 待实现 |
| 6 | 音频 DAC | ES8311（播放） | I2S0: MCK38/BCK14/WS13/DO45 | ⏸️ 待实现 |
| 7 | 音频 ADC | ES7210（4 通道录音） | I2S0（复用） | ⏸️ 待实现 |
| 8 | 功放 | PA（PCA9557 IO1 使能） | PCA9557 | ⏸️ 随音频 |
| 9 | IO 扩展 | PCA9557（LCD_CS/PA_EN/DVP_PWDN） | I2C0 @0x19 | ⏸️ 随外设 |
| 10 | BOOT 按键 | GPIO0 | 输入+上拉 | ⏸️ 待实现 |
| 11 | LED | 待确认引脚 | — | ⏸️ ledBlink 已支持任意 GPIO |

---

## 二、功能（Handler）总清单

### 2.1 已实现 ✅

| 指令 | 模式 | 说明 |
| --- | --- | --- |
| attitudeGet | 同步 | 读 QMI8658：加速度/陀螺仪/倾角 |
| sdList / sdRead / sdWrite | 同步 | SD 卡文件操作 |
| ledBlink | 持久化+stop | 任意 GPIO 闪烁 |
| getInfo / getState / stop / otaUpdate / powerDown / powerUp / reSet / reStart | 内置 | 框架自带 |
| sync_add / async_add | 示例 | 同步/异步模式模板 |

### 2.2 待实现（按实施顺序分组）

**Phase 1 — 纯状态类（轻、无实时流）**
- [ ] `keyGet`：读 BOOT 按键电平（同步）
- [ ] `touchGet`：读触摸坐标（同步）
- [ ] `wifiStatus`：当前 WiFi/AP 状态（同步，读 apex_network 状态）
- [ ] `lcdShowText`：LCD 显示文本（英文/数字，自绘点阵）—— 中文字库方案见决策 D

**Phase 2 — LCD 显示**
- [ ] `lcdShowColor`：全屏/区域填充颜色（同步）
- [ ] `lcdShowImage`：显示内置或 SD 中的图片（同步）
- [ ] `lcdBacklight`：背光亮度（同步）

**Phase 3 — 摄像头抓拍**
- [ ] `cameraCapture`：抓拍一帧 JPEG → 存储到 SD + 返回可访问的 URL（同步/异步）
- [ ] `cameraInfo`：摄像头型号/分辨率/格式（同步）

**Phase 4 — 音频**
- [ ] `audioPlay`：播放 SD 中的音频文件（WAV 先行；MP3 方案见决策 C）
- [ ] `audioStop`：停止播放（持久化+stop 模式）
- [ ] `audioRecord`：录音 → 存 SD（持久化+stop）
- [ ] `volumeSet`：音量/静音（同步）

**Phase 5 — 高级（重、需单独评估）**
- [ ] `cameraStream`：实时预览流（独立任务+队列）
- [ ] `wifiScan`：扫描周围 WiFi（需处理 AP/STA 状态机冲突）
- [ ] `speechRec`：离线语音识别（esp-sr，需分区表调整）
- [ ] `faceDetect`：人脸检测（esp-dl，需分区表调整）
- [ ] `bleHidSend`：BLE HID 键盘（NimBLE，与 WiFi 共存评估）

---

## 三、核心设计决策（需要拍板的疑虑点）

### 决策 A：图像/大数据传输通道 ⚠️ 最关键

**问题**：MQTT 报文上限为 收 4KB / 发 8KB，而 OV2640 JPEG 一帧（QVGA 质量 12）约 **15~30KB**，装不下。

| 方案 | 做法 | 优缺点 |
| --- | --- | --- |
| A1 扩 MQTT | out_size 提到 64KB | 简单，但大报文经 MQTT 传输脆弱、占内存 |
| **A2 HTTP 取图（推荐）** | 设备内置 HTTP 服务，新增 `/snapshot.jpg` 端点；MQTT 只回 URL | 符合 MCP 惯例（工具返回资源 URL）；图像/音频文件都能复用 |
| A3 分片 | 分 N 片走 MQTT 重组 | 复杂、易错，不推荐 |

**倾向 A2**：在现有 web server（apex_webserver）或独立 HTTP 服务上扩展 `/snapshot.jpg`、`/files/*` 等端点。**需要你确认**：中控（apex_mcp_bridge）能否访问设备的 HTTP（同一局域网即可）？

### 决策 B：文件上传通道（音频/图片内容从哪来）

AI 要播放音乐/显示图片，文件必须先到设备：
- B1 人工拷入 SD（USB/读卡器）—— 最简单
- B2 HTTP POST `/upload` → SD —— AI 可上传文件，推荐
- B3 MQTT base64 —— 仅小文件，不推荐

**倾向 B1+B2 都支持**（B1 天然可用，B2 作为接口）。需确认 B2 是否值得先做。

### 决策 C：MP3 解码方案

实战派用 `chmorgan/esp-audio-player`（旧 esp-adf `audio_element` 架构），**IDF v6 兼容性风险高**（依赖 legacy I2S 等）。

| 方案 | 做法 | 优缺点 |
| --- | --- | --- |
| **C1 WAV 先行（推荐）** | 自解析 WAV 头 + `i2s_std` 直接推 PCM | 零第三方依赖、v6 稳妥；仅支持 WAV/PCM |
| C2 新 esp_audio | 乐鑫新播放框架（esp-adf v2.6+） | 支持 MP3/FLAC；需评估 v6 + 组件获取 |
| C3 沿用 esp-audio-player | 原样移植 | 最快但 v6 下很可能要补兼容层 |

**倾向 C1 先跑通，C2 作为后续增强**。录制方 ES7210 可直接采 WAV（04-audio_es7210 就是 WAV 录音）。

### 决策 D：LCD 中文字库

实战派 `font_alipuhui20.c` 是完整中文字库（20px，约 2 万汉字，**编译进代码 ~2MB+**）。

| 方案 | 做法 | 优缺点 |
| --- | --- | --- |
| D1 英文点阵先行（推荐） | 自绘 ASCII 点阵（5×7/8×16） | 零资源开销；AI 下发英文/数字指令文案够用 |
| D2 中文字库放 storage | 字库文件存 SPIFFS/SD，运行时加载 | 支持中文，需把 .c 转成 .bin 且预留 2MB 空间 |
| D3 编译进代码 | 直接 include 实战派字库 | 简单但 +2MB Flash，OTA 分区受影响 |

**倾向 D1 先做，D2 按需**。**需确认**：AI 显示的内容是否必须中文？

### 决策 E：WiFi 扫描的冲突处理

`esp_wifi_scan_start` 会中断当前 STA 连接，apex_network 有自动重连逻辑，扫描与 AP/STA 状态机冲突。
- 方案：`wifiScan` 执行时通知 apex_network 暂停自动逻辑 → 扫描 → 恢复；或仅返回已缓存/AP 侧扫描结果。
- **倾向**：Phase 5 再设计，先不做。

### 决策 F：摄像头采集任务的归属

- `cameraCapture`（抓一帧）：耗时短（几十 ms），可在 worker 直接执行 —— **先做这个**
- `cameraStream`（持续流）：必须独立任务 + 队列（借鉴实战派双核流水线），worker 只控制启停 —— 后做

### 决策 G：分区表

当前：nvs(128K)+otadata+phy_init+ota_0(4M)+ota_1(4M)+storage(8M)。
- Phase 1~4 不动分区表（LCD/摄像头/音频资源走 SD）。
- 只有 Phase 5 的 esp-sr（语音模型 ~2-4MB）才需评估——届时 storage 内划子分区或调整 ota 大小。
- **倾向**：Phase 5 触发时再议。

---

## 四、实施顺序（推荐）

| 阶段 | 内容 | 依赖 | 验收 |
| --- | --- | --- | --- |
| P1 | keyGet / touchGet / wifiStatus / lcdShowText(英文) | bsp_key + bsp_touch + bsp_lcd + bsp_font | 指令返回正确值 |
| P2 | lcdShowColor / lcdShowImage / lcdBacklight | bsp_lcd | 屏显正确 |
| P3 | cameraCapture（HTTP 取图） | 决策 A 定案 + bsp_camera | 浏览器能访问快照 |
| P4 | audioPlay(WAV) / audioRecord / volumeSet | 决策 C 定案 + bsp_audio | 播放/录音文件正确 |
| P5 | cameraStream / wifiScan / 语音 / 人脸 / BLE | 各决策 + 分区表评估 | 按需 |

> 每完成一项，本文件对应条目标 ✅ 并记录实现要点。

---

## 五、待用户确认项汇总

1. **决策 A**：图像走 HTTP 取图（MQTT 只回 URL）？中控能否访问设备 HTTP？ → ✅ **已定：HTTP 取图**
2. **决策 B**：是否需要 HTTP 上传通道（/upload）？ → ⏸️ 后续按需
3. **决策 C**：音频先只做 WAV 播放（MP3 后续）？ → ✅ **已定：先不做音乐类播放（Phase 4 整体延后）**
4. **决策 D**：LCD 文本先只支持英文/数字？ → ✅ **已定：英文点阵先行**
5. **Phase 优先级**：P1~P4 是否按此顺序推进 → ✅ **已定：按 P1→P4 顺序（音频延后，实际 P1→P3 先行）**

### 实施状态

- [x] **P1**（✅ 已实现）：keyGet / touchGet / wifiStatus / lcdShowText(英文)
  - 新增 bsp_pca9557 / bsp_lcd(ST7789) / bsp_touch(FT5x06) / bsp_key / bsp_font(5x7)
  - 依赖按 v6 组件名：esp_lcd esp_driver_spi esp_driver_ledc esp_lcd_touch_ft5x06
- [ ] **P2**：lcdShowColor / lcdShowImage / lcdBacklight
- [ ] **P3**：cameraCapture（HTTP 取图）—— 需在 web server 加 /snapshot.jpg 端点
- [ ] **P4**：audioPlay(WAV) / audioRecord / volumeSet —— ⏸️ 延后
- [ ] **P5**：cameraStream / wifiScan / 语音 / 人脸 / BLE —— 按需
