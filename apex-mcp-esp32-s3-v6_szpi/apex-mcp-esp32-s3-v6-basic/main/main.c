#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"           // ← 必须包含
#include "freertos/FreeRTOS.h" // ← 必须包含，提供 TickType_t 等类型定义
#include "freertos/task.h"     // ← 必须包含，提供 vTaskDelay 声明
#include "apex_core.h"

#include "async_add.h"
#include "sync_add.h"
#include "bsp_board.h"
#include "attitude_app.h"
#include "sd_file_app.h"
#include "led_app.h"
#include "key_app.h"
#include "touch_app.h"
#include "wifi_app.h"
#include "lcd_app.h"
#include "camera_app.h"

static const char *TAG = "MAIN";
void app_main(void)
{
    // 查看 main 任务栈剩余
    UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "Main task stack free: %d bytes", uxHighWaterMark);
    // 初始化所有模块
    ESP_ERROR_CHECK(apex_core_init());

    // 外设层：板级外设初始化（components/ 下，替换外设只改这里）
    // 宽容处理：无对应硬件时框架照常运行，硬件指令返回错误
    esp_err_t bsp_ret = bsp_board_init();
    if (bsp_ret != ESP_OK)
    {
        ESP_LOGW(TAG, "板级外设初始化失败: %s (框架继续运行)", esp_err_to_name(bsp_ret));
    }
    // 硬件能力 → MCP 工具（必须在 apex_core_init 之后注册，与硬件是否就绪无关）
    attitude_app_init();
    sd_file_app_init();
    led_app_init();
    key_app_init();
    touch_app_init();
    wifi_app_init();
    lcd_app_init();
    camera_app_init();

    async_add_init();
    sync_add_init();
    // 再次查看
    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "Main task stack free after init: %d bytes", uxHighWaterMark);
    // 主循环（空循环，所有逻辑通过事件驱动）
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
