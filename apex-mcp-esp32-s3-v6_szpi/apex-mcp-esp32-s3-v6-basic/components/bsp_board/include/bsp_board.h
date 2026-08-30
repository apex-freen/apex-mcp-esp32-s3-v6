#pragma once

#include "esp_err.h"
#include "bsp_pins.h"
#include "bsp_i2c.h"
#include "bsp_imu.h"
#include "bsp_sd.h"
#include "bsp_pca9557.h"
#include "bsp_lcd.h"
#include "bsp_touch.h"
#include "bsp_key.h"
#include "bsp_font.h"
#include "bsp_camera.h"

/**
 * @brief 板级支持包入口：初始化板载外设（I2C/IMU/SD/LCD/触摸/按键）
 *
 * 该组件属于 components/ 外设层，仅依赖 ESP-IDF 驱动，不依赖 apex_frame。
 * 更换外设型号时只改本组件。
 */
esp_err_t bsp_board_init(void);
