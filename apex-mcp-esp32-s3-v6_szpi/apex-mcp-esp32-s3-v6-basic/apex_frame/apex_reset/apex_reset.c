#define TAG "APEX_RESET_LOG"
#include "esp_log.h"
#include "esp_system.h"

// 引入框架核心头文件
#include "apex_cmd_executor.h"
#include "apex_reset.h"
#include "apex_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define FUNCTION_KEY "reSet"

static const function_param_desc_t function_params[] =
    {
        // 数组为空 → 输出 {}
};

// 重置任务：先给网络栈时间发送响应，再写入出厂默认配置，最后重启
static void apex_reset_task(void *arg)
{
    // 预留 1.5 秒让框架将"重置成功"响应发给云端
    vTaskDelay(pdMS_TO_TICKS(1500));

    // 写入出厂默认配置到 NVS
    esp_err_t err = apex_config_reset_factory();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "写入出厂默认配置失败: %s", esp_err_to_name(err));
    }

    ESP_LOGW(TAG, "出厂设置已恢复，执行重启...");
    esp_restart();
}

// ==========================================
// 重置模块 (apex_reset) — 恢复出厂设置
// ==========================================
static int apex_reset_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    ESP_LOGW(TAG, "收到重置指令，擦除 NVS 并恢复出厂设置！");

    // 1. 擦除 NVS，清除所有用户配网信息与运行时配置
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS 擦除失败: %s", esp_err_to_name(err));
        return APEX_ERR_SYS;
    }

    // 2. 重新初始化 NVS，确保后续写入出厂默认值可用
    err = nvs_flash_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS 重新初始化失败: %s", esp_err_to_name(err));
        return APEX_ERR_SYS;
    }

    // 3. 创建异步任务：先发送成功响应，再写入出厂配置并重启
    // 同时通知服务端：设备即将重置
    cJSON *notify_data = cJSON_CreateObject();
    cJSON_AddStringToObject(notify_data, "action", "factory_reset");
    cJSON_AddNumberToObject(notify_data, "delay_ms", 1500);
    apex_cmd_send_notify(FUNCTION_KEY, "reset", notify_data);

    BaseType_t ret = xTaskCreate(apex_reset_task, "reset_task", 2048, NULL, 5, NULL);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "创建重置任务失败");
        return APEX_ERR_SYS;
    }

    return APEX_OK;
}

// ============================================================================
//  组件注册入口 (在 app_main 中由框架统一调用)
// ============================================================================
esp_err_t apex_reset_init(void)
{
    static char function_params_json_buf[64];
    int count = sizeof(function_params) / sizeof(function_params[0]);

    // 调用函数，把 JSON 写进 buffer
    build_function_param_desc_json(function_params, count,
                                   function_params_json_buf, sizeof(function_params_json_buf));
    apex_cmd_entry_t entry = {
        .cmd_key = FUNCTION_KEY,
        .function_name = "系统重置",
        .function_desc = "系统重置为默认状态，重要操作需谨慎（无需参数）",
        .function_params = function_params_json_buf,
        .role = "admin",
        .risk_level = "auth",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_FORCE, // 强制属性：重置为最高优先级，无视系统忙碌状态
        .handler = apex_reset_handler,
        .is_persistent = false, // 非持久化动作，由内部异步任务接管后续流程
        .stop_handler = NULL};  // 重置不可中途取消，无需停止回调

    // 注册到全局 Executor 路由表
    apex_cmd_register(entry);
    ESP_LOGI(TAG, "组件注册成功: %s (v%s)", entry.cmd_key, entry.version);
    return ESP_OK;
}
