#define TAG "LED_APP"
#include "esp_log.h"
#include "cJSON.h"
#include "apex_cmd_executor.h"
#include "led_app.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define FUNCTION_KEY   "ledBlink"
#define KEY_GPIO       "gpio"
#define KEY_INTERVAL   "interval_ms"

static int s_led_gpio = -1;
static int s_led_interval_ms = 500;
static volatile bool s_led_running = false;

// 闪烁任务：根据全局状态持续翻转 GPIO
static void led_blink_task(void *arg)
{
    while (s_led_running)
    {
        gpio_set_level(s_led_gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(s_led_interval_ms));
        if (!s_led_running)
            break;
        gpio_set_level(s_led_gpio, 0);
        vTaskDelay(pdMS_TO_TICKS(s_led_interval_ms));
    }
    gpio_set_level(s_led_gpio, 0);
    vTaskDelete(NULL);
}

// ==================== 持久化 Handler：启动闪烁 ====================
static int led_blink_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    if (s_led_running)
        return APEX_ERR_BUSY; // 已在闪烁，拒绝重复启动

    cJSON *gpio_item = cJSON_GetObjectItem(params, KEY_GPIO);
    if (!cJSON_IsNumber(gpio_item) || !GPIO_IS_VALID_GPIO(gpio_item->valueint))
        return APEX_ERR_PARAM;
    int gpio = gpio_item->valueint;

    int interval = 500;
    cJSON *interval_item = cJSON_GetObjectItem(params, KEY_INTERVAL);
    if (cJSON_IsNumber(interval_item))
        interval = interval_item->valueint;
    if (interval < 100 || interval > 10000)
        return APEX_ERR_PARAM;

    // 配置 GPIO 为推挽输出
    gpio_config_t io = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << gpio,
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    if (gpio_config(&io) != ESP_OK)
        return APEX_ERR_SYS;

    s_led_gpio = gpio;
    s_led_interval_ms = interval;
    s_led_running = true;
    xTaskCreate(led_blink_task, "led_blink", 2048, NULL, 5, NULL);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "gpio", gpio);
    cJSON_AddNumberToObject(data, "interval_ms", interval);
    cJSON_AddStringToObject(data, "status", "blinking");
    *res_data = data;
    return APEX_OK; // is_persistent=true → 框架保持锁定，需 stop 指令终止
}

// ==================== stop 钩子：关闭硬件 ====================
static int led_stop_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    s_led_running = false;
    if (s_led_gpio >= 0)
        gpio_set_level(s_led_gpio, 0);
    // 注意：解锁与响应由框架 apex_stop 统一调用 apex_cmd_finish 处理
    return APEX_OK;
}

// ==================== 组件注册入口 ====================
void led_app_init(void)
{
    apex_cmd_entry_t entry = {
        .cmd_key = FUNCTION_KEY,
        .function_name = "LED 闪烁",
        .function_desc = "让指定 GPIO 的 LED 持续闪烁（默认 500ms 周期），通过 stop 指令停止",
        .function_params = "{\"type\":\"object\",\"properties\":{\"gpio\":{\"type\":\"integer\",\"description\":\"LED 所在 GPIO 引脚号\"},\"interval_ms\":{\"type\":\"integer\",\"minimum\":100,\"maximum\":10000,\"default\":500,\"description\":\"闪烁间隔毫秒\"}},\"required\":[\"gpio\"]}",
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_EXCLUSIVE, // 独占：闪烁期间阻止其他硬件操作
        .handler = led_blink_handler,
        .is_persistent = true,
        .stop_handler = led_stop_handler,
    };
    apex_cmd_register(entry);
    ESP_LOGI(TAG, "组件注册成功: %s (v%s)", entry.cmd_key, entry.version);
}
