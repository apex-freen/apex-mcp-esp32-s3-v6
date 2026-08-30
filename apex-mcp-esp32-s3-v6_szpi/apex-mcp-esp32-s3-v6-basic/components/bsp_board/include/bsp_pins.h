#pragma once

#include "driver/gpio.h"
#include "driver/spi_common.h"

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

/* ==================== LCD ST7789（SPI3，CS 走 PCA9557） ==================== */
#define BSP_LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)
#define BSP_LCD_SPI_NUM (SPI3_HOST)
#define BSP_LCD_H_RES (320)
#define BSP_LCD_V_RES (240)
#define BSP_LCD_SPI_MOSI (GPIO_NUM_40)
#define BSP_LCD_SPI_CLK (GPIO_NUM_41)
#define BSP_LCD_SPI_CS (GPIO_NUM_NC) // CS 由 PCA9557 控制
#define BSP_LCD_DC (GPIO_NUM_39)
#define BSP_LCD_RST (GPIO_NUM_NC)
#define BSP_LCD_BACKLIGHT (GPIO_NUM_42)

/* ==================== BOOT 按键 ==================== */
#define BSP_BOOT_KEY_GPIO (GPIO_NUM_0)

/* ==================== 触摸 FT5x06 ==================== */
#define BSP_TOUCH_ADDR (0x38)

/* ==================== 摄像头 OV2640（8bit DVP，SCCB 复用 I2C0） ==================== */
#define CAMERA_PIN_PWDN (-1) // 电源由 PCA9557 DVP_PWDN 控制
#define CAMERA_PIN_RESET (-1)
#define CAMERA_PIN_XCLK (5)
#define CAMERA_PIN_SIOD (1) // SCCB SDA（复用 I2C0）
#define CAMERA_PIN_SIOC (2) // SCCB SCL（复用 I2C0）
#define CAMERA_PIN_D7 (9)
#define CAMERA_PIN_D6 (4)
#define CAMERA_PIN_D5 (6)
#define CAMERA_PIN_D4 (15)
#define CAMERA_PIN_D3 (17)
#define CAMERA_PIN_D2 (8)
#define CAMERA_PIN_D1 (18)
#define CAMERA_PIN_D0 (16)
#define CAMERA_PIN_VSYNC (3)
#define CAMERA_PIN_HREF (46)
#define CAMERA_PIN_PCLK (7)
#define CAMERA_XCLK_FREQ_HZ (24000000)
