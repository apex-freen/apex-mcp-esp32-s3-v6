#define TAG "APEX_CORE"
#include "apex_core.h"
#include "apex_event.h"
#include "apex_config.h"
#include "apex_mqtt.h"
#include "esp_log.h"
#include "apex_network.h"
#include "apex_webserver.h"
#include "apex_cmd_executor.h"
#include "esp_task_wdt.h"
#include "apex_notify.h"

// ============================================================================
// 事件组 事件位   Event Group
// ============================================================================
// #define CTRL_WIFI_RECONFIG_BIT BIT0
// #define CTRL_MQTT_START_BIT BIT1
// #define CTRL_MQTT_STOP_BIT BIT2

// 真正分配内存空间的地方
// EventGroupHandle_t apex_event_group = NULL;

// ============================================================================
// 全局事件循环（核心：所有模块事件都走这里）  系统事件控制面板
// ============================================================================
static void apex_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == APEX_EVENTS)
    {
        switch (event_id)
        {
        case APEX_EVENT_NETWORK_CONFIG_UPDATED:
            ESP_LOGI(TAG, "EVENT: 收到网络配置更新，仅标记信号...\n");
            // 核心：只发信号，不干活。这消除了 sys_evt 任务的栈溢出风险
            break;
        case APEX_EVENT_BASE_CONFIG_UPDATED:
            ESP_LOGI(TAG, "EVENT: 收到基础配置更新，仅标记信号...\n");
            // 核心：只发信号，不干活。这消除了 sys_evt 任务的栈溢出风险
            break;
        case APEX_EVENT_MQTT_CONFIG_UPDATED:
            ESP_LOGI(TAG, "EVENT: 收到MQTT配置更新，仅标记信号...\n");
            // 核心：只发信号，不干活。这消除了 sys_evt 任务的栈溢出风险
            break;

        case APEX_EVENT_NET_CONNECTED:
            ESP_LOGI(TAG, "EVENT: 联网成功，准备启动应用层...\n");
            break;

        case APEX_EVENT_NET_DISCONNECTED:
            ESP_LOGI(TAG, "EVENT: 断网，标记停止应用...\n");
            break;
        case APEX_EVENT_MQTT_DISCONNECTED:
            ESP_LOGI(TAG, "EVENT: MQTT服务离线，mqtt 断开应用...\n");
            break;
        case APEX_EVENT_HTTP_SERVER_STARTED:
            ESP_LOGI(TAG, "EVENT: WEB服务已开启...\n");
            break;
        case APEX_EVENT_MQTT_CONNECTED:
            ESP_LOGI(TAG, "EVENT: MQTT已连接...\n");
            break;

        // case APEX_EVENT_WIFI_AP_STARTED:
        //     ESP_LOGI(TAG, "EVENT: AP 已开启\n");
        //     break;
        // }
        default:
            ESP_LOGI(TAG, "有未标明的模块通知EVENT: %d\n", event_id);
        }
    }
    else
    {
        switch (event_id)
        default:
            ESP_LOGI(TAG, "ESP EVENT: %d\n", event_id);
    }
}

esp_err_t apex_core_init(void)
{

    ESP_LOGI(TAG, "设备启动，初始化所有模块...");
    // 2. 创建系统事件循环
    ESP_ERROR_CHECK(apex_event_init());

    // 3. 初始化系统配置（从NVS加载）
    ESP_ERROR_CHECK(apex_config_init());

    // 4. 初始化任务看门狗（30 秒超时，handler 卡死自动复位）
    //    若系统已初始化（esp-idf 启动时开启），则跳过
    //    ⚠ 不监控 main 任务，因为它只做初始化后进入空闲循环
    //    临时降级 task_wdt 的日志级别，避免 "TWDT already initialized" 被当作 ERROR 输出
    esp_log_level_t prev_level = esp_log_level_get("task_wdt");
    esp_log_level_set("task_wdt", ESP_LOG_WARN);
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 30000,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    if (esp_task_wdt_init(&twdt_config) == ESP_OK)
    {
        ESP_LOGI(TAG, "TWDT 已启用 (超时: %d ms)", twdt_config.timeout_ms);
    }
    else
    {
        ESP_LOGW(TAG, "TWDT 已由系统初始化，使用现有配置");
    }
    esp_log_level_set("task_wdt", prev_level);

    // 1. 初始化事件组
    // apex_event_group = xEventGroupCreate();

    // 3. 注册事件回调 (保留之前的注册逻辑)
    esp_event_handler_instance_register(APEX_EVENTS, ESP_EVENT_ANY_ID, &apex_event_handler, NULL, NULL);

    // 4. 创建【系统控制任务】
    // 关键点：分配 4096 或更厚的栈空间
    // xTaskCreate(apex_control_task, "apex_control_task", 4096, NULL, 5, NULL);

    ESP_ERROR_CHECK(apex_network_init());
    // ESP_LOGI("MEM apex_network_init", "剩余堆内存: %d bytes, 最小历史剩余: %d bytes",
    //          esp_get_free_heap_size(),
    //          esp_get_minimum_free_heap_size());
    ESP_ERROR_CHECK(apex_network_start());
    // ESP_LOGI("MEM apex_network_start", "剩余堆内存: %d bytes, 最小历史剩余: %d bytes",
    //          esp_get_free_heap_size(),
    //          esp_get_minimum_free_heap_size());
    // vTaskDelay(pdMS_TO_TICKS(500)); // 缓冲 500ms

    ESP_ERROR_CHECK(web_server_start());
    // ESP_LOGI("MEM web_server_start", "剩余堆内存: %d bytes, 最小历史剩余: %d bytes",
    //          esp_get_free_heap_size(),
    //          esp_get_minimum_free_heap_size());

    ESP_ERROR_CHECK(apex_mqtt_init());
    // ESP_LOGI("MEM apex_mqtt_init", "剩余堆内存: %d bytes, 最小历史剩余: %d bytes",
    //          esp_get_free_heap_size(),
    //          esp_get_minimum_free_heap_size());
    ESP_ERROR_CHECK(apex_cmd_executor_init());
    // ESP_LOGI("MEM apex_cmd_executor_init", "剩余堆内存: %d bytes, 最小历史剩余: %d bytes",
    //          esp_get_free_heap_size(),
    //          esp_get_minimum_free_heap_size());

    ESP_ERROR_CHECK(apex_notify_init());

    ESP_LOGI(TAG, "所有模块初始化完成：");
    ESP_LOGI(TAG, "1. WiFi: APSTA模式");
    ESP_LOGI(TAG, "2. Web服务器: http://192.168.4.1");
    ESP_LOGI(TAG, "3. 自动逻辑: APSTA 连网→5分钟关→STA | STA断网→1分钟 →APSTA");

    return ESP_OK;
}
