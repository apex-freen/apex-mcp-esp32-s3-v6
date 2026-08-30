#define TAG "APEX_MONITOR"
#include "apex_monitor.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 采样周期
#define MONITOR_INTERVAL_MS (10 * 1000)
// 内部 DRAM 剩余告警阈值（低于此值持续 2 个周期才告警，避免抖动刷屏）
#define MONITOR_MIN_FREE_DRAM (8 * 1024)
#define MONITOR_WARN_HOLD_CYCLES 2

static void monitor_task(void *arg)
{
    uint32_t low_dram_cycles = 0;

    while (1)
    {
        size_t total_dram = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
        size_t free_dram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t largest_dram = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
        size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t min_free = esp_get_minimum_free_heap_size();

        ESP_LOGI(TAG,
                 "DRAM: total=%d free=%d (%d%%) largest=%d | PSRAM: total=%d free=%d (%d%%) | 历史最小堆=%d",
                 (int)total_dram, (int)free_dram,
                 total_dram ? (int)(free_dram * 100 / total_dram) : 0,
                 (int)largest_dram,
                 (int)total_psram, (int)free_psram,
                 total_psram ? (int)(free_psram * 100 / total_psram) : 0,
                 (int)min_free);

        if (free_dram < MONITOR_MIN_FREE_DRAM)
        {
            if (++low_dram_cycles >= MONITOR_WARN_HOLD_CYCLES)
            {
                ESP_LOGW(TAG, "内部 DRAM 持续不足: free=%d bytes (< %d), 请检查内存泄漏或增大缓冲规划",
                         (int)free_dram, MONITOR_MIN_FREE_DRAM);
                low_dram_cycles = 0; // 告警一次后重新计数，避免刷屏
            }
        }
        else
        {
            low_dram_cycles = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(MONITOR_INTERVAL_MS));
    }
}

esp_err_t apex_monitor_init(void)
{
    xTaskCreate(monitor_task, "apex_monitor", 3072, NULL, 3, NULL);
    return ESP_OK;
}
