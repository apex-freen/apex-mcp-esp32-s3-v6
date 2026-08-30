#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "bsp_pins.h"

/**
 * @brief I2C0 总线内部接口（bsp_board 组件内部使用）
 *
 * 独立出头文件是为了让公共 bsp_board.h 不暴露驱动类型，
 * 应用层（attitude_app 等）只需依赖 bsp_board，无需感知 driver 组件。
 */

/**
 * @brief 初始化 I2C0 总线（幂等）
 */
esp_err_t bsp_i2c_init(void);

/**
 * @brief 获取全局 I2C0 总线句柄（供 I2C 外设共享，如 IMU/触摸/IO 扩展）
 */
i2c_master_bus_handle_t bsp_i2c_get_bus(void);
