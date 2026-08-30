#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int16_t x;
    int16_t y;
    bool pressed; // 是否有有效触点
} bsp_touch_point_t;

/**
 * @brief FT5x06 电容触摸初始化（I2C0，坐标已按面板方向转换）
 */
esp_err_t bsp_touch_init(void);

/**
 * @brief 读取触摸坐标（单点）
 */
esp_err_t bsp_touch_read(bsp_touch_point_t *pt);
