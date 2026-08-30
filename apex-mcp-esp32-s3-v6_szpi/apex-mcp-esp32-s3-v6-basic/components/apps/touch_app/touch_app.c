#define TAG "TOUCH_APP"
#include "esp_log.h"
#include "cJSON.h"
#include "apex_cmd_executor.h"
#include "bsp_board.h"
#include "touch_app.h"

#define FUNCTION_KEY "touchGet"

static int touch_get_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    bsp_touch_point_t pt;
    if (bsp_touch_read(&pt) != ESP_OK)
        return APEX_ERR_SYS;

    cJSON *data = cJSON_CreateObject();
    if (data == NULL)
        return APEX_ERR_SYS;

    cJSON_AddBoolToObject(data, "pressed", pt.pressed);
    cJSON_AddNumberToObject(data, "x", pt.x);
    cJSON_AddNumberToObject(data, "y", pt.y);

    *res_data = data;
    return APEX_OK;
}

void touch_app_init(void)
{
    apex_cmd_entry_t entry = {
        .cmd_key = FUNCTION_KEY,
        .function_name = "触摸屏",
        .function_desc = "读取 FT5x06 触摸屏坐标（未触摸时 pressed=false）",
        .function_params = "{\"type\":\"object\",\"properties\":{}}",
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_ALWAYS_ALLOWED,
        .handler = touch_get_handler,
        .is_persistent = false,
    };
    apex_cmd_register(entry);
    ESP_LOGI(TAG, "组件注册成功: %s", entry.cmd_key);
}
