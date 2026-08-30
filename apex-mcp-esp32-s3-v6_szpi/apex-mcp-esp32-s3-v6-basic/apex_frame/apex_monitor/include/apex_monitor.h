#pragma once

#include "esp_err.h"

/**
 * @brief 内存审计模块（借鉴实战派 14-handheld 的 displayMemoryUsage）
 *
 * 周期打印内部 DRAM / PSRAM / 历史最小堆用量；
 * 当内部 DRAM 剩余低于阈值时升级为 WARN 告警（防抖）。
 */
esp_err_t apex_monitor_init(void);
