#define TAG "WIFI_APP"
#include "esp_log.h"
#include "cJSON.h"
#include "stdlib.h"
#include "esp_wifi.h"
#include "apex_cmd_executor.h"
#include "apex_network.h"
#include "wifi_app.h"

#define FUNCTION_KEY "wifiStatus"
#define KEY_FORCE "force"

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

// ==================== wifiScan：三级策略扫描 ====================
static int wifi_scan_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    bool force = false;
    cJSON *force_item = cJSON_GetObjectItem(params, KEY_FORCE);
    if (cJSON_IsBool(force_item))
        force = cJSON_IsTrue(force_item);

    wifi_ap_record_t *aps = NULL;
    uint16_t count = 0;
    esp_err_t err = apex_wifi_scan(&aps, &count, force);
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND)
        return APEX_ERR_SYS;

    // 判断结果是否来自缓存（已连接且未强制）
    wifi_ap_record_t dummy;
    bool cached = (esp_wifi_sta_get_ap_info(&dummy) == ESP_OK) && !force;

    cJSON *data = cJSON_CreateObject();
    if (data == NULL)
    {
        if (aps)
            free(aps);
        return APEX_ERR_SYS;
    }

    cJSON_AddBoolToObject(data, "cached", cached);
    cJSON_AddNumberToObject(data, "count", count);

    cJSON *arr = cJSON_CreateArray();
    for (uint16_t i = 0; i < count; i++)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", (char *)aps[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", aps[i].rssi);
        cJSON_AddNumberToObject(item, "channel", aps[i].primary);
        cJSON_AddNumberToObject(item, "authmode", aps[i].authmode);
        cJSON_AddItemToArray(arr, item);
    }
    cJSON_AddItemToObject(data, "aps", arr);
    if (aps)
        free(aps);

    *res_data = data;
    return APEX_OK;
}

void wifi_app_init(void)
{
    apex_cmd_entry_t status_cmd = {
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
    apex_cmd_register(status_cmd);

    apex_cmd_entry_t scan_cmd = {
        .cmd_key = "wifiScan",
        .function_name = "WiFi 扫描",
        .function_desc = "扫描周围 WiFi。已连接时默认返回缓存（不断网）；force=true 强制实时扫描（短暂断连后自动恢复）",
        .function_params = "{\"type\":\"object\",\"properties\":{\"force\":{\"type\":\"boolean\",\"description\":\"true=强制实时扫描(已连接时短暂断连自动恢复)\"}},\"required\":[]}",
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_PARALLEL,
        .handler = wifi_scan_handler,
        .is_persistent = false,
    };
    apex_cmd_register(scan_cmd);

    ESP_LOGI(TAG, "组件注册成功: wifiStatus / wifiScan");
}
