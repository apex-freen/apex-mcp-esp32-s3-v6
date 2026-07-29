#define TAG "APEX_GET_STATE_LOG"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
// 引入框架核心头文件 (包含注册接口和响应助手)
#include "apex_cmd_executor.h"
#include "apex_get_state.h"
#include "cJSON.h"

#define FUNCTION_KEY "getState"

static const function_param_desc_t function_params[] =
    {
        // 数组为空 → 输出 {}
};

// 1. 内部定义：需要被“隐身”的系统级指令列表
static const char *SYSTEM_COMMAND_WHITELIST[] = {
    "test"
    // "apexOtaUpdate", "getInfo", "stop", "getState",
    // "powerDown", "powerUp", "restart", "reset"
};

// 检查是否为系统指令的辅助函数
static bool is_internal_system_cmd(const char *cmd_key)
{
    for (int i = 0; i < sizeof(SYSTEM_COMMAND_WHITELIST) / sizeof(char *); i++)
    {
        if (strcmp(cmd_key, SYSTEM_COMMAND_WHITELIST[i]) == 0)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief 获取设备当前指令状态
 * @note 同步方法直接在 Executor 线程中运行，不开启新任务
 */
static int apex_get_state_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *running_list = cJSON_AddArrayToObject(root, "running_commands");

    if (xSemaphoreTake(g_apex_state.lock, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        // 系统是否关机状态
        cJSON_AddStringToObject(root, "power", g_apex_state.is_standby ? "powerDown" : "powerUp");
        // 系统模式：NORMAL 或 EXCLUSIVE
        bool is_busy = (g_apex_state.current_state == APEX_STATE_BUSY);
        cJSON_AddStringToObject(root, "sys_mode", is_busy ? "EXCLUSIVE" : "NORMAL");

        // 遍历所有活跃槽位
        for (int i = 0; i < APEX_MAX_PARALLEL_CMDS; i++)
        {
            apex_active_cmd_t *slot = &g_apex_state.active_cmds[i];

            if (slot->in_use)
            {
                // 🔥 核心优化：双重过滤
                // 1. 过滤掉当前的这个查询指令 (通过 msg_id)
                // 2. 过滤掉所有框架自带的系统功能 (通过 cmd_key)
                if (strcmp(slot->msg_id, msg_id) == 0 || is_internal_system_cmd(slot->cmd_key))
                {
                    continue;
                }

                cJSON *item = cJSON_CreateObject();
                cJSON_AddStringToObject(item, "cmd", slot->cmd_key);
                cJSON_AddStringToObject(item, "msg_id", slot->msg_id);
                cJSON_AddBoolToObject(item, "persistent", slot->is_persistent);

                // 根据要求，已去掉 runtime 字段
                cJSON_AddItemToArray(running_list, item);
            }
        }
        xSemaphoreGive(g_apex_state.lock);
    }

    *res_data = root;
    return APEX_OK;
}

// ============================================================================
// 5. 组件注册入口 (在 app_main 中由框架统一调用)
// ============================================================================
esp_err_t apex_get_state_init(void)
{
    static char function_params_json_buf[64];
    int count = sizeof(function_params) / sizeof(function_params[0]);

    // 调用函数，把 JSON 写进 buffer
    build_function_param_desc_json(function_params, count,
                                   function_params_json_buf, sizeof(function_params_json_buf));
    apex_cmd_entry_t entry = {
        .cmd_key = FUNCTION_KEY,
        .function_name = "指令状态",
        .function_desc = "获取设备当前指令状态（无需参数）",
        .function_params = function_params_json_buf,
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_ALWAYS_ALLOWED, // 常驻开放：状态查询在任何情况下都应可执行
        .handler = apex_get_state_handler,
        .is_persistent = false, // 非持久化：查询状态是瞬时操作，执行即完成
        .stop_handler = NULL};  // 状态查询无需停止回调

    // 注册到全局 Executor 路由表
    apex_cmd_register(entry);
    ESP_LOGI(TAG, "组件注册成功: %s (v%s)", entry.cmd_key, entry.version);
    return ESP_OK;
}
