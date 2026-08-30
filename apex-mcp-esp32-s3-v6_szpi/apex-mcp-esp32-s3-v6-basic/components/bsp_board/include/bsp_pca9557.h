#pragma once

#include "esp_err.h"

/**
 * @brief PCA9557 IO 扩展芯片（实战派板载，I2C0 @0x19）
 * 扩展 IO：IO0=LCD_CS、IO1=PA_EN(功放使能)、IO2=DVP_PWDN(摄像头电源)
 */
esp_err_t bsp_pca9557_init(void);

/** LCD 片选（0 选中） */
void bsp_lcd_cs(uint8_t level);

/** 功放使能（1 打开功放） */
void bsp_pa_en(uint8_t level);

/** 摄像头电源（0 上电） */
void bsp_dvp_pwdn(uint8_t level);
