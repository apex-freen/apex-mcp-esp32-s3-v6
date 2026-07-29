#define TAG "APEX_POWER_DOWN_LOG"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
// 引入框架核心头文件 (包含注册接口和响应助手)
#include "apex_cmd_executor.h"
#include "apex_power_down.h"
#include "cJSON.h"

#define FUNCTION_KEY "powerDown"

static const function_param_desc_t function_params[] =
    {
        // 数组为空 → 输出 {}
};

// ==========================================
//  关机/待机模块 (apex_power_down)
// ==========================================
static int apex_power_down_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    if (g_apex_state.is_standby)
    {
        return APEX_OK; // 幂等
    }

    ESP_LOGI(TAG, "进入软件待机模式...");
    g_apex_state.is_standby = true;

    // TODO: 通知应用层关闭所有耗电外设（屏幕、电机驱动、传感器电源）
    // app_hardware_suspend();

    return APEX_OK;
}

// ============================================================================
//  组件注册入口 (在 app_main 中由框架统一调用)
// ============================================================================
esp_err_t apex_power_down_init(void)
{
    static char function_params_json_buf[64];
    int count = sizeof(function_params) / sizeof(function_params[0]);

    // 调用函数，把 JSON 写进 buffer
    build_function_param_desc_json(function_params, count,
                                   function_params_json_buf, sizeof(function_params_json_buf));
    apex_cmd_entry_t entry = {
        .cmd_key = FUNCTION_KEY,
        .function_name = "系统关机",
        .function_desc = "系统关机功能，进入低功耗模式，只有开机命令才能恢复工作（无需参数）",
        .function_params = function_params_json_buf,
        .role = "user",
        .risk_level = "auth",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_ALWAYS_ALLOWED, // 常驻开放：关机休眠在任何情况下都应可执行
        .handler = apex_power_down_handler,
        .is_persistent = false, // 非持久化：关机是瞬间操作，执行即完成
        .stop_handler = NULL};  // 关机无需停止回调

    // 注册到全局 Executor 路由表
    apex_cmd_register(entry);
    ESP_LOGI(TAG, "组件注册成功: %s (v%s)", entry.cmd_key, entry.version);
    return ESP_OK;
}
