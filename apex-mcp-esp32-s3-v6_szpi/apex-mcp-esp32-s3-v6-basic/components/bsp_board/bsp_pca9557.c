#define TAG "BSP_PCA9557"
#include "bsp_pca9557.h"
#include "bsp_i2c.h"
#include "esp_log.h"
#include "esp_bit_defs.h"
#include "freertos/FreeRTOS.h"

#define PCA9557_INPUT_PORT 0x00
#define PCA9557_OUTPUT_PORT 0x01
#define PCA9557_POLARITY_INVERSION_PORT 0x02
#define PCA9557_CONFIGURATION_PORT 0x03

#define LCD_CS_GPIO BIT(0)   // PCA9557_IO0
#define PA_EN_GPIO BIT(1)    // PCA9557_IO1
#define DVP_PWDN_GPIO BIT(2) // PCA9557_IO2

#define PCA9557_ADDR 0x19
#define SET_BITS(_m, _s, _v) ((_v) ? (_m) | ((_s)) : (_m) & ~((_s)))

static i2c_master_dev_handle_t s_dev = NULL;

static esp_err_t pca9557_dev_init(void)
{
    if (s_dev)
        return ESP_OK;

    i2c_master_bus_handle_t bus = bsp_i2c_get_bus();
    if (bus == NULL)
        return ESP_ERR_INVALID_STATE;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCA9557_ADDR,
        .scl_speed_hz = 100000,
    };
    return i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
}

static esp_err_t reg_read(uint8_t reg, uint8_t *data, size_t len)
{
    if (pca9557_dev_init() != ESP_OK)
        return ESP_FAIL;
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, 1000 / portTICK_PERIOD_MS);
}

static esp_err_t reg_write_byte(uint8_t reg, uint8_t data)
{
    if (pca9557_dev_init() != ESP_OK)
        return ESP_FAIL;
    uint8_t buf[2] = {reg, data};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 1000 / portTICK_PERIOD_MS);
}

esp_err_t bsp_pca9557_init(void)
{
    // 默认：DVP_PWDN=1(关)、PA_EN=0(静音)、LCD_CS=1(未选中)
    reg_write_byte(PCA9557_OUTPUT_PORT, 0x05);
    // IO0/1/2 设为输出，其余保持输入
    reg_write_byte(PCA9557_CONFIGURATION_PORT, 0xf8);
    ESP_LOGI(TAG, "PCA9557 就绪 (IO0=LCD_CS, IO1=PA_EN, IO2=DVP_PWDN)");
    return ESP_OK;
}

static esp_err_t set_output_state(uint8_t gpio_bit, uint8_t level)
{
    uint8_t data;
    if (reg_read(PCA9557_OUTPUT_PORT, &data, 1) != ESP_OK)
        return ESP_FAIL;
    return reg_write_byte(PCA9557_OUTPUT_PORT, SET_BITS(data, gpio_bit, level));
}

void bsp_lcd_cs(uint8_t level)
{
    set_output_state(LCD_CS_GPIO, level);
}

void bsp_pa_en(uint8_t level)
{
    set_output_state(PA_EN_GPIO, level);
}

void bsp_dvp_pwdn(uint8_t level)
{
    set_output_state(DVP_PWDN_GPIO, level);
}
