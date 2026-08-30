#define TAG "BSP_TOUCH"
#include "bsp_touch.h"
#include "bsp_pins.h"
#include "bsp_i2c.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_ft5x06.h"

static esp_lcd_touch_handle_t s_touch = NULL;

esp_err_t bsp_touch_init(void)
{
    if (s_touch)
        return ESP_OK; // 幂等

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_V_RES, // 240（配合 swap_xy）
        .y_max = BSP_LCD_H_RES, // 320
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {.reset = 0, .interrupt = 0},
        .flags = {.swap_xy = 1, .mirror_x = 1, .mirror_y = 0},
    };

    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    io_cfg.scl_speed_hz = 400000;

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bsp_i2c_get_bus(), &io_cfg, &tp_io),
                        TAG, "触摸 I2C IO 创建失败");
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_ft5x06(tp_io, &tp_cfg, &s_touch),
                        TAG, "FT5x06 创建失败");

    ESP_LOGI(TAG, "FT5x06 触摸就绪");
    return ESP_OK;
}

esp_err_t bsp_touch_read(bsp_touch_point_t *pt)
{
    if (s_touch == NULL || pt == NULL)
        return ESP_ERR_INVALID_STATE;

    esp_lcd_touch_read_data(s_touch);
    esp_lcd_touch_point_data_t pt_data = {0};
    uint8_t cnt = 0;
    if (esp_lcd_touch_get_data(s_touch, &pt_data, &cnt, 1) == ESP_OK && cnt > 0)
    {
        pt->x = pt_data.x;
        pt->y = pt_data.y;
        pt->pressed = true;
    }
    else
    {
        pt->pressed = false;
    }
    return ESP_OK;
}
