#pragma once

#include "esp_err.h"
#include <stdint.h>

/**
 * @brief ST7789 LCD（320×240，SPI3，背光 LEDC PWM）
 */
esp_err_t bsp_lcd_init(void);

/** 整屏填充颜色 */
void bsp_lcd_fill(uint16_t color);

/** 区域填充颜色 [x0,y0] - [x1,y1]（含端点） */
void bsp_lcd_fill_rect(int x0, int y0, int x1, int y1, uint16_t color);

/** 绘制位图区域 [x0,y0] - [x1,y1]（含端点，RGB565） */
void bsp_lcd_draw_bitmap(int x0, int y0, int x1, int y1, const uint16_t *data);

/** 背光亮度 0~100 */
void bsp_lcd_set_backlight(int percent);

/** RGB888 → RGB565 */
uint16_t bsp_lcd_rgb565(uint8_t r, uint8_t g, uint8_t b);
