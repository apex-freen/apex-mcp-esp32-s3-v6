#define TAG "APEX_NOTIFY_LOG"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <stdlib.h>

// 引入框架核心头文件
#include "apex_cmd_executor.h"
#include "apex_notify.h"
#include "cJSON.h"

// ============================================================================
// 1. 常量与参数定义区
// ============================================================================
static const char *FUNCTION_KEY = "notifyDemo";

// 参数：事件类型（字符串枚举）和可选附带数据
static const function_param_desc_t function_params[] = {
    {
        .key = "event_type",
        .type = "string",
        .description = "事件类型：alarm（报警）/ status（状态变更）/ done（任务完成）",
    },
    {
        .key = "message",
        .type = "string",
        .description = "事件附加描述",
        .has_default = 1,
        .default_val = 0, // 无 string 默认值时忽略，框架输出时判空
    },
};

// ============================================================================
// 2. Handler — 触发通知的核心演示
// ============================================================================
static int notify_demo_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    ESP_LOGI(TAG, "收到 %s 指令，准备发送设备端事件通知", FUNCTION_KEY);

    // 1. 参数校验
    if (params == NULL || !cJSON_IsObject(params))
    {
        ESP_LOGW(TAG, "参数格式错误，期望一个 JSON Object");
        return APEX_ERR_PARAM;
    }

    cJSON *event_item = cJSON_GetObjectItem(params, "event_type");
    if (!cJSON_IsString(event_item))
    {
        ESP_LOGW(TAG, "缺少 event_type 参数");
        return APEX_ERR_PARAM;
    }

    const char *event_type = event_item->valuestring;
    const char *message = NULL;
    cJSON *msg_item = cJSON_GetObjectItem(params, "message");
    if (cJSON_IsString(msg_item))
        message = msg_item->valuestring;

    // 2. 构造通知数据
    cJSON *notify_data = cJSON_CreateObject();
    cJSON_AddStringToObject(notify_data, "type", event_type);
    if (message)
        cJSON_AddStringToObject(notify_data, "message", message);

    // 3. 将 event_type 映射为通知事件名
    const char *notify_event = event_type; // 直接使用

    // 4. 【核心】发送 JSON-RPC 2.0 通知到服务端
    //    同时同步返回确认给调用方
    apex_cmd_send_notify(FUNCTION_KEY, notify_event, notify_data);

    // 5. 同步返回确认（仅告知"已推送"）
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "status", "notified");
    cJSON_AddStringToObject(result, "event", notify_event);
    *res_data = result;

    return APEX_OK;
}

// ============================================================================
// 3. 组件注册入口
// ============================================================================
esp_err_t apex_notify_init(void)
{
    static char function_params_json_buf[512];
    int count = sizeof(function_params) / sizeof(function_params[0]);

    build_function_param_desc_json(function_params, count,
                                   function_params_json_buf, sizeof(function_params_json_buf));

    apex_cmd_entry_t entry = {
        .cmd_key = FUNCTION_KEY,
        .function_name = "设备事件通知演示",
        .function_desc = "演示设备主动事件通知：调用后立即向服务端推送 JSON-RPC 2.0 通知，同时返回确认",
        .function_params = function_params_json_buf,
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_PARALLEL,
        .handler = notify_demo_handler,
        .is_persistent = false,
        .stop_handler = NULL,
    };

    apex_cmd_register(entry);
    ESP_LOGI(TAG, "组件注册成功: %s (v%s)", entry.cmd_key, entry.version);
    return ESP_OK;
}
