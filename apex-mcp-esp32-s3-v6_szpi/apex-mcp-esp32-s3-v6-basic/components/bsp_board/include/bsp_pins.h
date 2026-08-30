#pragma once

#include "driver/gpio.h"

/**
 * @brief 板级引脚集中定义（组件化版 esp32_s3_szp.h）
 *
 * 所有板载外设的引脚/地址宏统一收敛在此文件，
 * 更换板型时只需修改本文件。
 */

/* ==================== I2C0 总线 ==================== */
#define BSP_I2C_SDA (GPIO_NUM_1)
#define BSP_I2C_SCL (GPIO_NUM_2)
#define BSP_I2C_NUM (0)
#define BSP_I2C_FREQ_HZ (100000)

/* ==================== QMI8658 六轴 IMU ==================== */
#define QMI8658_SENSOR_ADDR (0x6A)

/* ==================== SD 卡（SDMMC 1 线） ==================== */
#define BSP_SD_CLK (47)
#define BSP_SD_CMD (48)
#define BSP_SD_D0 (21)
