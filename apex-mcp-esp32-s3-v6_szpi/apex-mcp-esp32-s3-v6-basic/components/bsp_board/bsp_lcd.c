#define TAG "BSP_LCD"
#include "bsp_lcd.h"
#include "bsp_pins.h"
#include "bsp_pca9557.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

#define LCD_CMD_BITS    8
#define LCD_PARAM_BITS  8
#define LCD_LEDC_CH     LEDC_CHANNEL_0

static esp_lcd_panel_handle_t s_panel = NULL;

uint16_t bsp_lcd_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// ==================== 背光 ====================
static esp_err_t backlight_init(void)
{
    const ledc_channel_config_t bl_ch = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = 0,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = true,
    };
    const ledc_timer_config_t bl_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = 0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&bl_timer), TAG, "LEDC timer 配置失败");
    ESP_RETURN_ON_ERROR(ledc_channel_config(&bl_ch), TAG, "LEDC channel 配置失败");
    return ESP_OK;
}

void bsp_lcd_set_backlight(int percent)
{
    if (percent > 100)
        percent = 100;
    if (percent < 0)
        percent = 0;
    uint32_t duty = (1023 * percent) / 100; // 10bit
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH);
}

// ==================== 面板 ====================
esp_err_t bsp_lcd_init(void)
{
    ESP_RETURN_ON_ERROR(backlight_init(), TAG, "背光初始化失败");

    // SPI3 总线（MISO/Quad 不用）
    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_SPI_CLK,
        .mosi_io_num = BSP_LCD_SPI_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = BSP_LCD_H_RES * BSP_LCD_V_RES * 2,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI 初始化失败");

    // Panel IO（CS 由 PCA9557 软件控制，这里 NC）
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_DC,
        .cs_gpio_num = BSP_LCD_SPI_CS,
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 2,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, &io_handle), TAG, "Panel IO 创建失败");

    // ST7789 驱动
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(io_handle, &panel_config, &s_panel), TAG, "ST7789 创建失败");

    esp_lcd_panel_reset(s_panel);
    bsp_lcd_cs(0); // 选中 LCD（PCA9557）
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "面板初始化失败");
    esp_lcd_panel_invert_color(s_panel, true);
    esp_lcd_panel_swap_xy(s_panel, true);
    esp_lcd_panel_mirror(s_panel, true, false);

    esp_lcd_panel_disp_on_off(s_panel, true);
    bsp_lcd_set_backlight(100);
    ESP_LOGI(TAG, "ST7789 就绪 (%dx%d)", BSP_LCD_H_RES, BSP_LCD_V_RES);
    return ESP_OK;
}

void bsp_lcd_fill(uint16_t color)
{
    // 行缓冲放 PSRAM（参考实战派：大缓冲显式分配）
    uint16_t *line = (uint16_t *)heap_caps_malloc(BSP_LCD_H_RES * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (line == NULL)
        return;
    for (int i = 0; i < BSP_LCD_H_RES; i++)
        line[i] = color;
    for (int y = 0; y < BSP_LCD_V_RES; y++)
        esp_lcd_panel_draw_bitmap(s_panel, 0, y, BSP_LCD_H_RES, y + 1, line);
    heap_caps_free(line);
}

void bsp_lcd_draw_bitmap(int x0, int y0, int x1, int y1, const uint16_t *data)
{
    if (s_panel == NULL || data == NULL)
        return;
    esp_lcd_panel_draw_bitmap(s_panel, x0, y0, x1, y1, data);
}
