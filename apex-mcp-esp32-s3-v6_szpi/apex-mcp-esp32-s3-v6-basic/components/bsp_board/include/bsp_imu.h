#pragma once

#include "esp_err.h"
#include <stdint.h>

/**
 * @brief QMI8658 六轴姿态数据
 */
typedef struct
{
    int16_t acc_x, acc_y, acc_z;      // 加速度原始值
    int16_t gyr_x, gyr_y, gyr_z;      // 陀螺仪原始值
    float AngleX, AngleY, AngleZ;     // 由加速度推算的倾角（度）
} bsp_imu_data_t;

/**
 * @brief 初始化 QMI8658（阻塞等待芯片就绪）
 */
esp_err_t bsp_imu_init(void);

/**
 * @brief 读取一次姿态数据（加速度/陀螺仪/倾角）
 */
esp_err_t bsp_imu_read(bsp_imu_data_t *out);
