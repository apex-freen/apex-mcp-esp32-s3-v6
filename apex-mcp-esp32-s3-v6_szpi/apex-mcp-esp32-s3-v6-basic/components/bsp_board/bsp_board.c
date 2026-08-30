#define TAG "BSP_BOARD"
#include "bsp_board.h"
#include "bsp_i2c.h"
#include "esp_log.h"
#include "esp_check.h"

// 单个外设初始化失败不阻断启动（无对应硬件时框架照常运行，相关指令返回错误）
esp_err_t bsp_board_init(void)
{
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "I2C 初始化失败");
    ESP_RETURN_ON_ERROR(bsp_pca9557_init(), TAG, "PCA9557 初始化失败");

    if (bsp_lcd_init() != ESP_OK)
        ESP_LOGW(TAG, "LCD 初始化失败 (显示功能不可用)");
    if (bsp_touch_init() != ESP_OK)
        ESP_LOGW(TAG, "触摸初始化失败 (触摸功能不可用)");

    if (bsp_imu_init() != ESP_OK)
        ESP_LOGW(TAG, "IMU 初始化失败 (姿态功能不可用)");

    if (bsp_key_init() != ESP_OK)
        ESP_LOGW(TAG, "按键初始化失败");

    if (bsp_sd_mount() != ESP_OK)
        ESP_LOGW(TAG, "SD 卡挂载失败 (文件功能不可用)");

    ESP_LOGI(TAG, "板级外设初始化完成 (I2C/IMU/SD/LCD/触摸/按键)");
    return ESP_OK;
}
