#define TAG "WIFI_APP"
#include "esp_log.h"
#include "cJSON.h"
#include "apex_cmd_executor.h"
#include "apex_network.h"
#include "wifi_app.h"

#define FUNCTION_KEY "wifiStatus"

static int wifi_status_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    char status[192];
    apex_get_wifi_status_text(status, sizeof(status));

    cJSON *data = cJSON_CreateObject();
    if (data == NULL)
        return APEX_ERR_SYS;
    cJSON_AddStringToObject(data, "status", status);

    *res_data = data;
    return APEX_OK;
}

void wifi_app_init(void)
{
    apex_cmd_entry_t entry = {
        .cmd_key = FUNCTION_KEY,
        .function_name = "网络状态",
        .function_desc = "查询当前 WiFi STA/AP 连接状态、IP 与信号强度",
        .function_params = "{\"type\":\"object\",\"properties\":{}}",
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_ALWAYS_ALLOWED,
        .handler = wifi_status_handler,
        .is_persistent = false,
    };
    apex_cmd_register(entry);
    ESP_LOGI(TAG, "组件注册成功: %s", entry.cmd_key);
}
