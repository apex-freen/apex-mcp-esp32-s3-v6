#define TAG "BSP_CAMERA"
#include "bsp_camera.h"
#include "bsp_pins.h"
#include "bsp_pca9557.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "esp_camera.h"

static bool s_ready = false;
static int s_pid = 0;
static camera_fb_t *s_last_fb = NULL; // 最近一次抓取的帧（归还用）

esp_err_t bsp_camera_init(void)
{
    if (s_ready)
        return ESP_OK;

    bsp_dvp_pwdn(0); // 摄像头供电（PCA9557）

    camera_config_t config = {
        .pin_pwdn = CAMERA_PIN_PWDN,
        .pin_reset = CAMERA_PIN_RESET,
        .pin_xclk = CAMERA_PIN_XCLK,
        .pin_sccb_sda = CAMERA_PIN_SIOD,
        .pin_sccb_scl = CAMERA_PIN_SIOC,
        .sccb_i2c_port = 0, // 复用已初始化的 I2C0

        .pin_d7 = CAMERA_PIN_D7,
        .pin_d6 = CAMERA_PIN_D6,
        .pin_d5 = CAMERA_PIN_D5,
        .pin_d4 = CAMERA_PIN_D4,
        .pin_d3 = CAMERA_PIN_D3,
        .pin_d2 = CAMERA_PIN_D2,
        .pin_d1 = CAMERA_PIN_D1,
        .pin_d0 = CAMERA_PIN_D0,
        .pin_vsync = CAMERA_PIN_VSYNC,
        .pin_href = CAMERA_PIN_HREF,
        .pin_pclk = CAMERA_PIN_PCLK,

        .xclk_freq_hz = CAMERA_XCLK_FREQ_HZ,
        .ledc_timer = LEDC_TIMER_1,
        .ledc_channel = LEDC_CHANNEL_1,

        .pixel_format = PIXFORMAT_JPEG, // OV2640 支持硬件 JPEG
        .frame_size = FRAMESIZE_QVGA,   // 320x240
        .jpeg_quality = 12,
        .fb_count = 2,
        .fb_location = CAMERA_FB_IN_PSRAM, // 帧缓冲放 PSRAM
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "摄像头初始化失败 (0x%x): %s", err, esp_err_to_name(err));
        return err;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL)
        return ESP_ERR_NOT_FOUND;

    s_pid = s->id.PID;
    if (s_pid == GC0308_PID)
        s->set_hmirror(s, 1); // GC0308 需镜像（参考实战派）

    s_ready = true;
    ESP_LOGI(TAG, "OV2640/GC0308 就绪 (PID=0x%x, QVGA JPEG, 帧缓冲 PSRAM)", s_pid);
    return ESP_OK;
}

esp_err_t bsp_camera_capture_jpeg(uint8_t **out, size_t *len)
{
    if (!s_ready || out == NULL || len == NULL)
        return ESP_ERR_INVALID_STATE;

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == NULL)
    {
        ESP_LOGW(TAG, "抓帧失败");
        return ESP_FAIL;
    }

    if (fb->format != PIXFORMAT_JPEG)
    {
        // OV2640 硬件 JPEG，理论上不会走到这里
        esp_camera_fb_return(fb);
        return ESP_ERR_NOT_SUPPORTED;
    }

    s_last_fb = fb; // 记录以归还
    *out = fb->buf;
    *len = fb->len;
    return ESP_OK; // 调用方处理完必须 bsp_camera_return_frame()
}

void bsp_camera_return_frame(void)
{
    if (s_last_fb)
    {
        esp_camera_fb_return(s_last_fb);
        s_last_fb = NULL;
    }
}

int bsp_camera_get_pid(void)
{
    return s_pid;
}

bool bsp_camera_ready(void)
{
    return s_ready;
}
