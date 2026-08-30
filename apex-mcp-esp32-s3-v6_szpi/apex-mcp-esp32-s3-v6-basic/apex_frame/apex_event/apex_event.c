#include "apex_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#define TAG "APEX_EVENT"

// 定义事件基类（与声明对应）
ESP_EVENT_DEFINE_BASE(APEX_EVENTS);

// 状态锁存表结构
typedef struct
{
    bool is_latched; // 该事件是否被锁存（发生过）
    void *data;      // 存储最后一次的数据副本（可选）
    size_t data_len;
} apex_latch_t;

static apex_latch_t s_latch_table[APEX_EVENT_MAX];
static SemaphoreHandle_t s_latch_mutex = NULL;

// 内部函数：判断该事件是否属于需要“回溯”的状态类事件
static bool is_sticky_event(apex_event_id_t id)
{
    switch (id)
    {
    case APEX_EVENT_NET_CONNECTED:
        return true;
    case APEX_EVENT_NET_DISCONNECTED:
        return true;
    case APEX_EVENT_MQTT_CONNECTED:
        return true;
    case APEX_EVENT_MQTT_DISCONNECTED:
        return true;
    case APEX_EVENT_MQTT_CONFIG_UPDATED:
        return true;
    case APEX_EVENT_HTTP_SERVER_STARTED:
        // return true;
    default:
        return false;
    }
}

esp_err_t apex_event_init(void)
{
    s_latch_mutex = xSemaphoreCreateMutex();
    memset(s_latch_table, 0, sizeof(s_latch_table));

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    { // 忽略重复创建错误
        return err;
    }
    return ESP_OK;
}

esp_err_t apex_event_send(apex_event_id_t event_id, void *data, size_t data_len)
{
    if (event_id >= APEX_EVENT_MAX)
        return ESP_ERR_INVALID_ARG;

    // 1. 如果是 Sticky 事件，更新锁存表
    if (is_sticky_event(event_id))
    {
        xSemaphoreTake(s_latch_mutex, portMAX_DELAY);

        s_latch_table[event_id].is_latched = true;
        // 如果有数据，可以考虑在这里 malloc 存储，但简单起见我们先处理无数据的状态

        // 特殊处理：互斥状态清除
        if (event_id == APEX_EVENT_NET_CONNECTED)
            s_latch_table[APEX_EVENT_NET_DISCONNECTED].is_latched = false;
        if (event_id == APEX_EVENT_NET_DISCONNECTED)
            s_latch_table[APEX_EVENT_NET_CONNECTED].is_latched = false;

        xSemaphoreGive(s_latch_mutex);
    }

    // 2. 正常投递事件到系统循环
    return esp_event_post(APEX_EVENTS, event_id, data, data_len, portMAX_DELAY);
}

esp_err_t apex_event_register_handler(apex_event_id_t event_id, esp_event_handler_t handler, void *handler_arg)
{
    if (event_id >= APEX_EVENT_MAX)
        return ESP_ERR_INVALID_ARG;

    // 1. 注册原生监听（处理未来的事件）
    esp_err_t err = esp_event_handler_instance_register(APEX_EVENTS, event_id, handler, handler_arg, NULL);
    if (err != ESP_OK)
        return err;

    // 2. 检查回溯（处理过去的事件）
    if (is_sticky_event(event_id))
    {
        bool should_trigger = false;

        xSemaphoreTake(s_latch_mutex, portMAX_DELAY);
        if (s_latch_table[event_id].is_latched)
        {
            should_trigger = true;
        }
        xSemaphoreGive(s_latch_mutex);

        if (should_trigger)
        {
            ESP_LOGI(TAG, "检测到 Sticky 事件 ID:%d, 执行回溯触发", event_id);
            // 手动调用一次 handler，模拟事件发生
            handler(handler_arg, APEX_EVENTS, event_id, NULL);
        }
    }

    return ESP_OK;
}
