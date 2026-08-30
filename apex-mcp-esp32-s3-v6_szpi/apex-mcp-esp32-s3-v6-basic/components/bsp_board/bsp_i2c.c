#define TAG "BSP_I2C"
#include "bsp_i2c.h"
#include "esp_log.h"

// 全局 I2C0 总线句柄（bsp_imu 等外设共享）
static i2c_master_bus_handle_t s_i2c_bus = NULL;

esp_err_t bsp_i2c_init(void)
{
    if (s_i2c_bus)
        return ESP_OK; // 幂等：已初始化

    // 新 I2C master 驱动（IDF v5.2+ / v6 通用，参考实战派 07/13/14）
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = BSP_I2C_NUM,
        .scl_io_num = BSP_I2C_SCL,
        .sda_io_num = BSP_I2C_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = 1,
    };

    esp_err_t ret = i2c_new_master_bus(&i2c_mst_config, &s_i2c_bus);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "I2C0 就绪 (SDA=%d, SCL=%d, %d Hz)", BSP_I2C_SDA, BSP_I2C_SCL, BSP_I2C_FREQ_HZ);
    }
    else
    {
        ESP_LOGE(TAG, "I2C0 创建失败: %s", esp_err_to_name(ret));
    }
    return ret;
}

i2c_master_bus_handle_t bsp_i2c_get_bus(void)
{
    return s_i2c_bus;
}
