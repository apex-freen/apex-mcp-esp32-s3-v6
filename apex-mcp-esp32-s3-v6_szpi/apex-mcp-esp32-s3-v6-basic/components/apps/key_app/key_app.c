#define TAG "KEY_APP"
#include "esp_log.h"
#include "cJSON.h"
#include "apex_cmd_executor.h"
#include "bsp_board.h"
#include "key_app.h"

#define FUNCTION_KEY "keyGet"

static int key_get_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    cJSON *data = cJSON_CreateObject();
    if (data == NULL)
        return APEX_ERR_SYS;

    int level = bsp_key_level();
    cJSON_AddBoolToObject(data, "pressed", level == 0); // 低电平=按下
    cJSON_AddNumberToObject(data, "level", level);

    *res_data = data;
    return APEX_OK;
}

void key_app_init(void)
{
    apex_cmd_entry_t entry = {
        .cmd_key = FUNCTION_KEY,
        .function_name = "BOOT 按键",
        .function_desc = "读取 BOOT 按键当前状态（按下/松开）",
        .function_params = "{\"type\":\"object\",\"properties\":{}}",
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_ALWAYS_ALLOWED,
        .handler = key_get_handler,
        .is_persistent = false,
    };
    apex_cmd_register(entry);
    ESP_LOGI(TAG, "组件注册成功: %s", entry.cmd_key);
}
