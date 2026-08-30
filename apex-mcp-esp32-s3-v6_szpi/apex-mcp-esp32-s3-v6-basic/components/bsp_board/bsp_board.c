#define TAG "BSP_BOARD"
#include "bsp_board.h"
#include "bsp_i2c.h"
#include "esp_log.h"
#include "esp_check.h"

esp_err_t bsp_board_init(void)
{
    // 按依赖顺序初始化板载外设
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "I2C 初始化失败");

    // IMU 初始化失败不阻断启动（无该硬件时固件仍可运行，姿态指令会返回错误）
    esp_err_t imu_ret = bsp_imu_init();
    if (imu_ret != ESP_OK)
    {
        ESP_LOGW(TAG, "IMU 初始化失败: %s (姿态功能不可用)", esp_err_to_name(imu_ret));
        return imu_ret;
    }

    ESP_LOGI(TAG, "板级外设初始化完成 (I2C + QMI8658)");

    // SD 卡：挂载失败不阻断启动（无卡时固件照常运行，文件指令返回错误）
    esp_err_t sd_ret = bsp_sd_mount();
    if (sd_ret != ESP_OK)
    {
        ESP_LOGW(TAG, "SD 卡挂载失败: %s (文件功能不可用)", esp_err_to_name(sd_ret));
    }
    return ESP_OK;
}
