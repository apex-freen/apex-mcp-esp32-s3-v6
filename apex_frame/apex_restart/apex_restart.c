#define TAG "APEX_RESTART_LOG"
#include "esp_log.h"
#include "esp_system.h"

// 引入框架核心头文件
#include "apex_cmd_executor.h"
#include "apex_restart.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define FUNCTION_KEY "reStart"

static const function_param_desc_t function_params[] =
    {
        // 数组为空 → 输出 {}
};

// 延迟重启任务：给底层网络栈 1~2 秒的时间把"重启成功"响应发给云端
static void apex_delayed_restart_task(void *arg)
{
    ESP_LOGW(TAG, "系统将在 1.5 秒后重启...");
    vTaskDelay(pdMS_TO_TICKS(1500));
    ESP_LOGW(TAG, "执行物理重启 esp_restart()");
    esp_restart();
}

// ==========================================
// 重启模块 (apex_restart) — 软复位设备
// ==========================================
static int apex_restart_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    ESP_LOGI(TAG, "收到重启指令，准备软复位");

    // 创建独立任务执行延迟重启，让当前 handler 正常返回完成通讯闭环
    BaseType_t ret = xTaskCreate(apex_delayed_restart_task, "restart_task", 2048, NULL, 5, NULL);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "创建重启任务失败");
        return APEX_ERR_SYS;
    }

    return APEX_OK;
}

// ============================================================================
//  组件注册入口 (在 app_main 中由框架统一调用)
// ============================================================================
esp_err_t apex_restart_init(void)
{
    static char function_params_json_buf[64];
    int count = sizeof(function_params) / sizeof(function_params[0]);

    // 调用函数，把 JSON 写进 buffer
    build_function_param_desc_json(function_params, count,
                                   function_params_json_buf, sizeof(function_params_json_buf));
    apex_cmd_entry_t entry = {
        .cmd_key = FUNCTION_KEY,
        .function_name = "系统重启",
        .function_desc = "系统重启功能，重要操作需谨慎（无需参数）",
        .function_params = function_params_json_buf,
        .role = "user",
        .risk_level = "auth",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_FORCE, // 强制属性：重启为最高优先级，无视系统忙碌状态
        .handler = apex_restart_handler,
        .is_persistent = false, // 非持久化动作，由延迟重启任务接管后续流程
        .stop_handler = NULL};  // 重启不可中途取消，无需停止回调

    // 注册到全局 Executor 路由表
    apex_cmd_register(entry);
    ESP_LOGI(TAG, "组件注册成功: %s (v%s)", entry.cmd_key, entry.version);
    return ESP_OK;
}
