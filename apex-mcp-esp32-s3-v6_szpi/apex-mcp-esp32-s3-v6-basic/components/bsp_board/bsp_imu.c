#define TAG "BSP_IMU"
#include "bsp_board.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "math.h"

// ==================== QMI8658 寄存器地址 ====================
enum qmi8658_reg
{
    QMI8658_WHO_AM_I,
    QMI8658_REVISION_ID,
    QMI8658_CTRL1,
    QMI8658_CTRL2,
    QMI8658_CTRL3,
    QMI8658_CTRL4,
    QMI8658_CTRL5,
    QMI8658_CTRL6,
    QMI8658_CTRL7,
    QMI8658_CTRL8,
    QMI8658_CTRL9,
    QMI8658_CATL1_L,
    QMI8658_CATL1_H,
    QMI8658_CATL2_L,
    QMI8658_CATL2_H,
    QMI8658_CATL3_L,
    QMI8658_CATL3_H,
    QMI8658_CATL4_L,
    QMI8658_CATL4_H,
    QMI8658_FIFO_WTM_TH,
    QMI8658_FIFO_CTRL,
    QMI8658_FIFO_SMPL_CNT,
    QMI8658_FIFO_STATUS,
    QMI8658_FIFO_DATA,
    QMI8658_STATUSINT = 45,
    QMI8658_STATUS0,
    QMI8658_STATUS1,
    QMI8658_TIMESTAMP_LOW,
    QMI8658_TIMESTAMP_MID,
    QMI8658_TIMESTAMP_HIGH,
    QMI8658_TEMP_L,
    QMI8658_TEMP_H,
    QMI8658_AX_L,
    QMI8658_AX_H,
    QMI8658_AY_L,
    QMI8658_AY_H,
    QMI8658_AZ_L,
    QMI8658_AZ_H,
    QMI8658_GX_L,
    QMI8658_GX_H,
    QMI8658_GY_L,
    QMI8658_GY_H,
    QMI8658_GZ_L,
    QMI8658_GZ_H,
    QMI8658_COD_STATUS = 70,
    QMI8658_dQW_L = 73,
    QMI8658_dQW_H,
    QMI8658_dQX_L,
    QMI8658_dQX_H,
    QMI8658_dQY_L,
    QMI8658_dQY_H,
    QMI8658_dQZ_L,
    QMI8658_dQZ_H,
    QMI8658_dVX_L,
    QMI8658_dVX_H,
    QMI8658_dVY_L,
    QMI8658_dVY_H,
    QMI8658_dVZ_L,
    QMI8658_dVZ_H,
    QMI8658_TAP_STATUS = 89,
    QMI8658_STEP_CNT_LOW,
    QMI8658_STEP_CNT_MIDL,
    QMI8658_STEP_CNT_HIGH,
    QMI8658_RESET = 96
};

static i2c_master_dev_handle_t s_imu_dev = NULL;

static esp_err_t qmi8658_dev_init(void)
{
    if (s_imu_dev)
        return ESP_OK;

    i2c_master_bus_handle_t bus = bsp_i2c_get_bus();
    if (bus == NULL)
    {
        ESP_LOGE(TAG, "I2C 总线未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = QMI8658_SENSOR_ADDR,
        .scl_speed_hz = BSP_I2C_FREQ_HZ,
    };
    return i2c_master_bus_add_device(bus, &dev_cfg, &s_imu_dev);
}

static esp_err_t qmi8658_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    if (qmi8658_dev_init() != ESP_OK)
        return ESP_FAIL;
    return i2c_master_transmit_receive(s_imu_dev, &reg_addr, 1, data, len, 1000 / portTICK_PERIOD_MS);
}

static esp_err_t qmi8658_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    if (qmi8658_dev_init() != ESP_OK)
        return ESP_FAIL;
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_transmit(s_imu_dev, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
}

esp_err_t bsp_imu_init(void)
{
    uint8_t id = 0;

    qmi8658_register_read(QMI8658_WHO_AM_I, &id, 1);
    int retry = 10;
    while (id != 0x05 && retry-- > 0) // 芯片 ID 应为 0x05
    {
        vTaskDelay(pdMS_TO_TICKS(200));
        qmi8658_register_read(QMI8658_WHO_AM_I, &id, 1);
    }
    if (id != 0x05)
    {
        ESP_LOGE(TAG, "QMI8658 未就绪 (ID=0x%02x)", id);
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "QMI8658 就绪 (ID=0x%02x)", id);

    qmi8658_register_write_byte(QMI8658_RESET, 0xb0); // 复位
    vTaskDelay(pdMS_TO_TICKS(10));
    qmi8658_register_write_byte(QMI8658_CTRL1, 0x40); // 地址自动增加
    qmi8658_register_write_byte(QMI8658_CTRL7, 0x03); // 使能加速度+陀螺仪
    qmi8658_register_write_byte(QMI8658_CTRL2, 0x95); // ACC 4g 250Hz
    qmi8658_register_write_byte(QMI8658_CTRL3, 0xd5); // GRY 512dps 250Hz
    return ESP_OK;
}

esp_err_t bsp_imu_read(bsp_imu_data_t *out)
{
    if (out == NULL)
        return ESP_ERR_INVALID_ARG;

    uint8_t status = 0;
    if (qmi8658_register_read(QMI8658_STATUS0, &status, 1) != ESP_OK)
        return ESP_FAIL;
    if (!(status & 0x03)) // 加速度/陀螺仪数据未就绪
        return ESP_ERR_NOT_FOUND;

    int16_t buf[6];
    if (qmi8658_register_read(QMI8658_AX_L, (uint8_t *)buf, 12) != ESP_OK)
        return ESP_FAIL;

    out->acc_x = buf[0];
    out->acc_y = buf[1];
    out->acc_z = buf[2];
    out->gyr_x = buf[3];
    out->gyr_y = buf[4];
    out->gyr_z = buf[5];

    // 由加速度推算倾角（弧度转角度，180/π≈57.29578）
    float temp;
    temp = (float)out->acc_x / sqrt((float)out->acc_y * out->acc_y + (float)out->acc_z * out->acc_z);
    out->AngleX = atan(temp) * 57.29578f;
    temp = (float)out->acc_y / sqrt((float)out->acc_x * out->acc_x + (float)out->acc_z * out->acc_z);
    out->AngleY = atan(temp) * 57.29578f;
    temp = sqrt((float)out->acc_x * out->acc_x + (float)out->acc_y * out->acc_y) / (float)out->acc_z;
    out->AngleZ = atan(temp) * 57.29578f;

    return ESP_OK;
}
