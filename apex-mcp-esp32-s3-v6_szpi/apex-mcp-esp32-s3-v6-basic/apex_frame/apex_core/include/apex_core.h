#pragma once

#include "esp_err.h"

/**
 * @brief 框架总入口
 * 初始化 NVS、系统事件循环、日志系统，并启动网络子系统
 */
esp_err_t apex_core_init(void);
