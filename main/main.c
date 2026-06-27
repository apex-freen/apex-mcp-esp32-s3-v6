#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"           // ← 必须包含
#include "freertos/FreeRTOS.h" // ← 必须包含，提供 TickType_t 等类型定义
#include "freertos/task.h"     // ← 必须包含，提供 vTaskDelay 声明
#include "apex_core.h"

#include "async_add.h"
#include "sync_add.h"
#include "apex_notify.h"

static const char *TAG = "MAIN";
void app_main(void)
{
    // 查看 main 任务栈剩余
    UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "Main task stack free: %d bytes", uxHighWaterMark);
    // 初始化所有模块
    ESP_ERROR_CHECK(apex_core_init());

    async_add_init();
    sync_add_init();
    apex_notify_init();
    // 再次查看
    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "Main task stack free after init: %d bytes", uxHighWaterMark);
    // 主循环（空循环，所有逻辑通过事件驱动）
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
