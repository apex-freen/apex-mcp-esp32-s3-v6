#pragma once

#include "esp_err.h"
#include "bsp_pins.h"
#include "bsp_imu.h"
#include "bsp_sd.h"

/**
 * @brief 板级支持包入口：初始化板载外设（当前：I2C0 总线 + QMI8658 IMU）
 *
 * 该组件属于 components/ 外设层，仅依赖 ESP-IDF 驱动，不依赖 apex_frame。
 * 更换外设型号时只改本组件。
 */
esp_err_t bsp_board_init(void);
