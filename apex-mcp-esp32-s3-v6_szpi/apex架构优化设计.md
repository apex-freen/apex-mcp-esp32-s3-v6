# APEX 框架架构优化设计（借鉴实战派设备层经验）

> 目标：将立创实战派（szpi-s3-esp）在"设备层"的成熟经验——内存规划、任务模型、日志规范、BSP 管理——吸收进 APEX 智能体框架，同时保留 APEX 在"平台层"（MQTT/AES-CCM/OTA/安全）的优势。
> 适用对象：`apex-mcp-esp32-s3-v6-basic`（IDF v6.x）
> 本文档为设计稿，供评审后分里程碑实施。

---

## 一、背景与目标

### 1.1 现状

- APEX 是手搓框架，无参考实现，存在三类问题：**内存无规划、handler 阻塞 MQTT、日志有安全隐患**。
- 实战派官方示例（06~14 为 v5.x 新驱动 API，与 IDF v6 兼容）提供了设备层的最佳实践，可直接借鉴设计，但**不能直接搬代码**。

### 1.2 设计目标

| 编号 | 目标 | 对应章节 |
| --- | --- | --- |
| G1 | 可控制的调试日志：开发/联调可见明文，量产编译期裁剪 | 三、日志体系 |
| G2 | 内存可控：PSRAM 策略 + 缓冲规划 + 内存审计 | 四、内存策略 |
| G3 | handler 不再阻塞 MQTT，独立执行任务 | 五、任务模型 |
| G4 | 预留硬件接入：BSP/外设全部组件化，换外设只改 components | 六、BSP 预留 |
| G5 | 模块归属清晰：apex_frame 只含芯片功能与通用平台能力 | 六、模块归属总原则 |

---

## 二、现状审计（问题清单）

### 2.1 日志问题（G1）

| # | 位置 | 问题 | 风险 |
| --- | --- | --- | --- |
| L1 | apex_cmd_executor.c:254 | `ESP_LOGI("明文响应(前200字): %.200s", ...)` 打印解密后明文，**无条件、不可控** | 🔴 量产隐患 |
| L2 | apex_crypto.c:197 | 每次解密都 `ESP_LOGI("本地时间: %llu, 收到时间: %llu")` | 刷屏 + 时间戳信息泄露 |
| L3 | 各模块 | 大量 `"xxx OK"` 级 INFO 日志，事件 handler 全打日志 | 日志污染，难定位真问题 |
| L4 | — | 无生产/调试分级策略 | 无法按环境切换 |

### 2.2 内存问题（G2）

| # | 位置 | 问题 |
| --- | --- | --- |
| M1 | 全局 | 无 PSRAM 策略配置（对比 14：SPIRAM_OCT/TRY_ALLOCATE_WIFI_LWIP） |
| M2 | apex_cmd_executor.c | cJSON 高频 `CreateObject/PrintUnformatted/malloc`，全部吃内部 DRAM |
| M3 | apex_crypto.c | 加解密缓冲每次 `malloc`（大小 = 明文+13+8+16），无复用 |
| M4 | apex_mqtt | 收发缓冲未显式规划，超长报文仅"拒绝发送" |
| M5 | 全局 | 无运行时内存审计（对比 14 的 displayMemoryUsage） |

### 2.3 任务模型问题（G3）

| # | 位置 | 问题 |
| --- | --- | --- |
| T1 | apex_mqtt.c:90 | `apex_process_incoming_cmd()` 在 esp-mqtt 事件回调内**同步执行整个流水线**（解密→调度→handler） |
| T2 | — | 耗时/持久化 handler 阻塞 MQTT 事件任务 → 后续指令丢包/延迟 |
| T3 | apex_core.c:95 | 30s 任务看门狗整机复位，是"崩溃兜底"而非"优雅隔离" |

### 2.4 硬件接入问题（G4）

| # | 现状 | 目标 |
| --- | --- | --- |
| H1 | 无 BSP 组件、无引脚集中定义 | 建立 bsp_board 组件，引脚集中在头文件（学 esp32_s3_szp.h） |
| H2 | 指令表固定 30 槽 | 可配置容量，容纳硬件 handler |

---

## 三、设计 A：日志体系与安全整改（G1）

### 3.1 日志开关机制（核心：开发可见，量产隐藏）

**采用编译期 Kconfig 开关，而非运行期判断**——编译期裁剪后量产固件中明文日志代码根本不存在，杜绝被误开的风险。

```kconfig
# Kconfig.projbuild
menu "APEX Debug"
config APEX_DEBUG_PAYLOAD
    bool "打印加解密明文（仅联调用）"
    default n
    help
      开启后在 LOGD 级别打印加解密前后的明文/密文，
      用于与中控(apex_mcp_bridge)联调验证链路。量产必须关闭。
endmenu
```

配套的 `sdkconfig.defaults` 区分两套构建：

```ini
# sdkconfig.defaults        —— 量产默认（关）
CONFIG_APEX_DEBUG_PAYLOAD=n
CONFIG_LOG_DEFAULT_LEVEL_WARN=y

# sdkconfig.defaults.debug  —— 开发/联调（idf.py -DSDKCONFIG_DEFAULTS=... 选用）
CONFIG_APEX_DEBUG_PAYLOAD=y
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y
```

### 3.2 新增 `apex_log.h`（common/utils/）

基于 ESP_LOG 的统一封装，带模块前缀 + 分级 + 明文开关：

```c
// apex_log.h —— 统一日志接口
#define APEX_LOGE(mod, ...) ESP_LOGE("APEX_" mod, __VA_ARGS__)
#define APEX_LOGW(mod, ...) ESP_LOGW("APEX_" mod, __VA_ARGS__)
#define APEX_LOGI(mod, ...) ESP_LOGI("APEX_" mod, __VA_ARGS__)
#define APEX_LOGD(mod, ...) ESP_LOGD("APEX_" mod, __VA_ARGS__)

// 加解密链路联调日志：开发可见明文，量产编译期裁剪
#if CONFIG_APEX_DEBUG_PAYLOAD
#define APEX_LOG_PAYLOAD(prefix, data, len) \
    ESP_LOGD("APEX_CRYPTO", "%s[%.200s] len=%d", prefix, data, len)
#else
#define APEX_LOG_PAYLOAD(prefix, data, len) do {} while (0)
#endif
```

### 3.3 整改清单

| 动作 | 说明 |
| --- | --- |
| L1 改为受控日志 | `apex_cmd_send_response` 内明文打印包进 `#if CONFIG_APEX_DEBUG_PAYLOAD`；同时打印密文 hex 前若干字节，便于与对端比对 |
| L2 时间戳日志 | 移入 `#if CONFIG_APEX_DEBUG_PAYLOAD`，仅 DEBUG 级别；解密失败原因码保留 WARN |
| 分级策略 | 启动时打一次**配置摘要**（WiFi/MQTT/设备ID/固件版本），此后 INFO 收敛；明文仅 DEBUG 且受开关控制 |
| 构建切换 | 量产 `sdkconfig.defaults`（关）/ 联调 `sdkconfig.defaults.debug`（开），两套一键切换 |

### 3.4 借鉴实战派

- 采用其 TAG 规范（模块名即 TAG）。
- 采用 14 的"启动摘要 + 周期性状态"模式：启动打配置，运行期只打异常。

### 3.5 验证

- **联调构建**（开关开）：能打印加解密前后明文/密文，可与 apex_mcp_bridge 逐字节比对验证链路。
- **量产构建**（开关关）：编译产物中无明文日志代码（可 `strings` 固件确认），正常轮询 5 分钟日志 ≤ 20 行。
- 密钥、Token 等敏感值永不打日志（无论开关状态）。

---

## 四、设计 B：内存与 PSRAM 策略（G2）

### 4.1 sdkconfig 搬入（直接参考 14-handheld）

```ini
# sdkconfig.defaults 新增
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=2048
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
CONFIG_LOG_DEFAULT_LEVEL_WARN=y          # 生产日志级别（见设计 A）
```

### 4.2 缓冲规划表（显式内存策略）

> ✅ = M2 已实施；⏸️ = 决策后暂缓

| 缓冲 | 现状 | 设计 | 分配方式 | 状态 |
| --- | --- | --- | --- | --- |
| MQTT 收包缓冲 | `buffer.size=4096` 显式配置 | 维持显式配置即可 | esp-mqtt 内部 | ✅ 达标 |
| MQTT 发送缓冲 | `buffer.out_size=8192` 显式配置 | 维持显式配置 + 超限前置检查（executor 已有） | esp-mqtt 内部 | ✅ 达标 |
| 加解密缓冲 | 每次 malloc | **复用静态缓冲池**（tx/b64/plain 3×4KB） | 静态数组（内部 RAM） | ✅ |
| 命令队列 | 无 | 固定容量结构体池（见设计 C） | 静态数组 | ⏸️ M3 |
| cJSON 大对象 | 内部 RAM | 仅超大响应（>2KB）可走 PSRAM | 按需 | ⏸️ 见 4.5 |

### 4.3 内存审计（✅ 已实施：apex_monitor）

新增 `apex_monitor` 组件（apex_frame/，已由 apex_core_init 调用）：
- 周期任务（10s）打印：内部 DRAM 总量/已用/最大空闲块、PSRAM 总量/已用、堆最小历史剩余。
- 触发条件：内部 DRAM 剩余 < 8KB 且持续 2 个周期时升级为 WARN 告警（防抖，避免刷屏）。

### 4.4 验证

- 连续执行 100 次 sync_add + 50 次 notify，`esp_get_minimum_free_heap_size()` 不下降（无泄漏）。
- 日志中可见 PSRAM 分配生效（camera/大响应场景）。

### 4.5 关于 cJSON 的决策（⏸️ 暂不启用 PSRAM hook）

`utils.c` 中已有 `cJSON_Parse_PSRAM()`，但其内部 `cJSON_InitHooks()` 是**全局永久切换**——启用后所有后续 cJSON 分配（包括高频小对象）都会进 PSRAM，与"小对象留内部 RAM"原则冲突。
**决策**：MQTT 报文有 4096 上限、加解密已缓冲池化，指令 JSON 规模可控，当前**不启用**。若未来接入摄像头/音频等大响应场景再评估。

---

## 五、设计 C：handler 独立执行任务模型（G3）

### 5.1 目标拓扑（✅ M3 已实施：借鉴实战派"采集→队列→处理"双核流水线）

```
MQTT 事件任务 (esp-mqtt 默认)
    │ 收到 command 报文
    ▼
解密（快，毫秒级）→ 组装 cmd_msg_t → 投递命令队列（不等待）
    ▼
┌─────────────────────────────────────┐
│ apex_cmd_worker 任务（绑定 Core1）    │
│   去重 → 匹配 → 状态锁 → 熔断检查     │
│   → 执行 handler → 加密回传           │
└─────────────────────────────────────┘
```

- **MQTT 回调职责收窄**：解密 + 入队 + 返回（µs~ms 级），永不阻塞收包。✅
- **worker 任务**：10KB 栈（容纳 2KB 消息副本 + 调度流水线 + handler 调用链），绑定 Core1。✅
- **单指令超时监控**：handler 执行前 arm 10s esp_timer，超时后释放槽位、回 `APEX_ERR_TIMEOUT` 并 notify；系统 TWDT（30s）保留为最后防线。✅

### 5.2 数据结构（✅ 已实施，队列深度按内存权衡定为 4）

```c
// apex_cmd_executor.c 内
typedef struct {
    char payload[APEX_CMD_PAYLOAD_MAX]; // 解密后的明文拷贝（静态队列，非复用缓冲池）
    size_t len;
} cmd_msg_t;

#define APEX_CMD_QUEUE_DEPTH 4      // 深度 4（4×2KB=8KB 内部 RAM；原设计 8，权衡后降为 4）
#define APEX_CMD_PAYLOAD_MAX 2048   // 单指令明文上限（超限直接拒收）
```

### 5.3 并发策略映射（⚠️ 实现为单 worker FIFO，如实说明限制）

| flags | worker 行为 |
| --- | --- |
| PARALLEL | 进入公共 worker 队列，顺序执行 |
| EXCLUSIVE | 获取独占锁后才执行（`apex_state_lock` 逻辑不变） |
| FORCE | 仍能绕过 Busy 状态获取锁，但在单 worker FIFO 下**无法真正抢占**（排队等待） |
| ALWAYS_ALLOWED | 随时可执行（只读查询） |

> **限制**：单 worker 为顺序执行模型，handler 之间不并发；FORCE 的"抢占中断"需多 worker 才能实现，暂缓。handler 卡死时 worker 仍会被卡住（超时定时器已对外宣告并释放槽位，系统 TWDT 兜底复位）。

### 5.4 兼容性（✅）

- 保留 `apex_cmd_executor()` / `apex_process_incoming_cmd()` 对外接口签名不变（内部实现改为入队）。
- 既有 8 个内置指令与 sync_add/async_add 示例无需改动。

### 5.5 验证

- 注册一个 `vTaskDelay(5000)` 的测试 handler，执行期间持续下发指令，确认：
  - **MQTT 收包不被阻塞**（连接保持、不重连、keepalive 正常）
  - getState 在 5s 后正常返回（单 worker 串行，排队等待是预期行为）
  - 队列满时打 `命令队列已满` ERROR 且不崩溃
- 注册一个永不返回的 handler，确认 10s 后收到 `APEX_ERR_TIMEOUT` 响应 + `command_timeout` notify。

---

## 六、设计 D：模块归属总原则 + BSP 组件化（G4/G5）

### 6.0 模块归属总原则（G5，核心决策）

**划分标准不是"外设 vs 非外设"，而是"是否与芯片强绑定"**：

- **apex_frame/ —— 芯片功能 + 通用平台服务（与具体外设型号无关）**
  - 芯片自带通信：WiFi、BLE（ESP32-S3 内置）
  - 协议栈/平台服务：MQTT、WebServer、AES-CCM（走 PSA）、NVS 配置、OTA、事件总线、看门狗、电源管理
  - 芯片级配置：PSRAM 模式、时钟、晶振 —— 属芯片能力，留在 apex_frame/sdkconfig（⚠️ 勿与"外设"混淆，换外设不能误删）

- **components/ —— 一切可替换的板级外设（与具体型号绑定）**
  - BSP：`bsp_board`（引脚定义、外设初始化）—— BSP 本质是"外设的板级适配"，不进 apex_frame
  - 外设驱动与应用封装：LCD / 摄像头 / 音频 Codec / IMU / SD / 触摸
  - 外挂通信模块（如 4G、LoRa、外接 WiFi）也归此类——虽是"通信"，但不是芯片自带

- **依赖方向（硬约束）**：`apex_frame` 永不依赖 `components`；换外设 = 只改/替换 components，框架零改动。

```
┌────────────────────────────────────────────────┐
│ components/   外设层（可替换，按型号定制）        │
│   bsp_board + 外设驱动 + apps(handler)          │
├────────────────────────────────────────────────┤
│ apex_frame/   平台层（芯片功能，跨外设通用）      │
│   core/event/config/network/mqtt/crypto/...    │
├────────────────────────────────────────────────┤
│ ESP-IDF v6（芯片 SDK）                          │
└────────────────────────────────────────────────┘
```

### 6.1 组件布局

> ✅ = M4 已实施；⏸️ = 待后续里程碑

```
apex_frame/                        # 平台层（芯片功能 + 通用服务，不依赖 components）
components/
├── bsp_board/                     # ✅ 已实施（组件化 esp32_s3_szp 的起点）
│   ├── include/bsp_board.h        #   板级入口（不暴露驱动类型，应用层零驱动依赖）
│   ├── include/bsp_pins.h         #   ⭐ 引脚宏集中定义（学 esp32_s3_szp.h）✅
│   ├── include/bsp_i2c.h          #   I2C0 内部接口（新 i2c_master 驱动，IDF v6 兼容）✅
│   ├── include/bsp_imu.h          #   QMI8658 API ✅
│   ├── bsp_board.c / bsp_i2c.c    #   ✅
│   ├── bsp_imu.c                  #   ✅（从实战派 02/14 移植）
│   ├── bsp_lcd.c                  #   ⏸️ ST7789 + 背光（esp_lcd 框架）
│   ├── bsp_camera.c               #   ⏸️ OV2640 + PSRAM 帧缓冲
│   ├── bsp_audio.c                #   ⏸️ ES8311/ES7210（i2s_std/i2s_tdm）
│   ├── bsp_sd.c                   #   ✅ SDMMC（从实战派 03/14 移植）
│   └── bsp_touch.c                #   ⏸️ FT5x06 + LVGL 适配
└── apps/                          # 硬件能力 → MCP 工具
    ├── attitude_app.c             #   ✅ attitudeGet（同步，首个硬件 handler 范例）
    ├── sd_file_app.c              #   ✅ sdList/sdRead/sdWrite（SD 文件操作）
    ├── led_app.c                  #   ✅ ledBlink（持久化 + stop 模式示范）
    ├── camera_app.c               #   ⏸️ cameraStream（异步 handler）
    ├── music_app.c                #   ⏸️ audioPlay（持久化 + stop）
    └── ...
```

**M5 已落地的要点**：
- **三种 handler 模式齐备**（后续扩展模板）：
  | 模式 | 范例 | 说明 |
  | --- | --- | --- |
  | 同步 | attitudeGet / sdList / sdRead / sdWrite | 返回 APEX_OK，框架自动响应 |
  | 持久化+stop | ledBlink | `is_persistent=true` + `stop_handler`，由系统 stop 指令终止 |
  | 异步 | async_add（示例） | `APEX_ASYNC_OK` + `apex_cmd_finish` |
- **真实外设**：bsp_sd（SDMMC 1 线，从实战派 03/14 移植）已并入 bsp_board，SD 文件指令可落盘。
- **IDF v6 组件命名**：bsp_board 依赖已按 v6 实际组件名（`esp_driver_i2c`/`esp_driver_gpio`/`esp_driver_sdmmc`）配置。

**M4 已落地的要点**：
- 依赖方向验证：`bsp_board` 只依赖 ESP-IDF 驱动；`attitude_app` 只依赖 `bsp_board + apex_cmd_executor`；apex_frame 零改动。
- `bsp_board.h` 不暴露 `driver/i2c_master.h`（内部拆 `bsp_i2c.h`），应用层无需感知驱动组件——外设封装边界干净。
- 无硬件宽容启动：`bsp_board_init()` 失败仅告警，固件照常运行，`attitudeGet` 指令运行时报错而非崩溃。

### 6.2 硬件能力 ↔ handler 映射（直接复用实战派代码逻辑）

| 实战派示例 | 能力 | handler 模式 | 移植要点 |
| --- | --- | --- | --- |
| 07-lcd_camera | 相机预览 | 异步（async） | 帧缓冲 PSRAM；采集/显示任务进独立核心（设计 C 预留） |
| 11-mp3_player | 音乐播放 | 持久化 + stop | `is_persistent=true` + `stop_handler` |
| 12-speech_recognition | 语音识别 | 持久化 | esp-sr 组件经 idf_component.yml 引入 |
| 02-attitude | 姿态 | 同步 | 只读，`ALWAYS_ALLOWED` |
| 03-micro_sd | 文件列表/读写 | 同步/异步 | 注意 SD 与 OTA 分区共存 |
| 09-wifi_scan | 配网辅助 | 同步 | 与 apex_network 合并策略 |

### 6.3 指令表容量

`s_cmd_table[30]` 与故障表 `[30]` 改为 `CONFIG_APEX_MAX_CMDS`（默认 48，Kconfig 暴露），避免加硬件 handler 后撞上限。

### 6.4 分区与存储

- 现有 ota_0/ota_1(4M)+storage(8M) 可满足；接入 SD 后注意 spiffs 与 SD 挂载点分离（`/storage` vs `/sdcard`）。
- 摄像头/音频等大内存场景依赖设计 B 的 PSRAM 配置先行落地。

---

## 七、实施路线图

| 里程碑 | 内容 | 风险 | 依赖 |
| --- | --- | --- | --- |
| M1 | 设计 A：日志开关机制（Kconfig 双构建 + apex_log.h + 整改 L1/L2） | 低 | 无 |
| M2 | 设计 B：sdkconfig PSRAM + 缓冲池 + 内存审计 | 低 | 无 |
| M3 | 设计 C：cmd_worker 任务重构 | 中 | M1（便于调试） |
| M4 | 设计 D：模块归属落地 + bsp_board 组件 + 首个硬件 handler（建议 attitudeGet 或 cameraStream） | 中 | M2 + M3 |
| M5 | 硬件 handler 扩展（音乐/语音/SD/配网） | 中 | M4 |

> 建议 M1、M2 先行——改动小、见效快、为 M3/M4 提供干净的调试与内存基线。

---

## 八、风险与注意事项

1. **API 版本**：实战派 06~14 可参考；01~05 的旧 I2C/I2S API 在 IDF v6 已废弃，仅借鉴思路。
2. **worker 重构**：handler 执行上下文从"MQTT 回调"变为"worker 任务"，若现有 handler 依赖回调上下文（极少数）需同步调整；`msg_id` 生命周期不变。
3. **内存**：命令队列 8×2KB = 16KB 静态开销，需在 M2 确认内存预算；若紧张可降队列深度或改指针池。
4. **看门狗策略变化**：从"整机复位"改为"单指令超时"，需评估安全兜底是否足够（保留整机看门狗作为最后防线，阈值放宽）。
5. **OTA 与 SD 共存**：升级时确保 SD 挂载不干扰 OTA 校验（分区隔离）。
6. **明文日志开关误开**：`CONFIG_APEX_DEBUG_PAYLOAD` 采用编译期裁剪（`#if` 而非运行期判断），量产 CI 中强制校验该配置为 `n`（构建脚本断言），杜绝误开。
