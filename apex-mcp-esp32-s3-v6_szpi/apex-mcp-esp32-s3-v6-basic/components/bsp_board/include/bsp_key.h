#pragma once

#include "esp_err.h"

/**
 * @brief BOOT 按键（GPIO0，输入+上拉）
 */
esp_err_t bsp_key_init(void);

/** 返回按键电平：0=按下（低电平），1=松开 */
int bsp_key_level(void);
