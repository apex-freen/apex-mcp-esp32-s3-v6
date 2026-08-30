#define TAG "BSP_KEY"
#include "bsp_key.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"

static bool s_inited = false;

esp_err_t bsp_key_init(void)
{
    if (s_inited)
        return ESP_OK;
    gpio_config_t io = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << BSP_BOOT_KEY_GPIO,
        .pull_down_en = 0,
        .pull_up_en = 1, // 内部上拉，按下为低
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "按键 GPIO 配置失败");
    s_inited = true;
    ESP_LOGI(TAG, "BOOT 按键就绪 (GPIO%d)", BSP_BOOT_KEY_GPIO);
    return ESP_OK;
}

int bsp_key_level(void)
{
    return gpio_get_level(BSP_BOOT_KEY_GPIO);
}
