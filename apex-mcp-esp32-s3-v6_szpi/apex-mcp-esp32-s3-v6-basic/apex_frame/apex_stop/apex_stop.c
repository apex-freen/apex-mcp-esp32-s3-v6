#define TAG "APEX_STOP"
#include "esp_log.h"
#include "cJSON.h"
#include "apex_cmd_executor.h"
#include "apex_stop.h"

#define FUNCTION_KEY "stop"
#define KEY_PARAM_A "fun_key"
static const function_param_desc_t function_params[] =
    {
        {
            .key = KEY_PARAM_A, // 宏展开是 "add"
            .type = "string",
            // 其他字段不写，编译器自动置 0/NULL
        },
};

// ==========================================
// 通用停止指令 Handler
// ==========================================
static int apex_stop_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    // 1. 获取目标 (支持通过 cmd_key 或更精确的 msg_id 停止)
    cJSON *target_item = cJSON_GetObjectItem(params, KEY_PARAM_A);
    if (!cJSON_IsString(target_item))
        return APEX_ERR_PARAM;
    const char *target_cmd = target_item->valuestring;

    // 2. 查找运行中的 msg_id
    // 💡 建议优化：优先检查参数里有没有传具体的 target_msg_id，没有再根据 cmd_key 查
    const char *running_msg_id = apex_state_get_active_msg_id(target_cmd);
    if (running_msg_id == NULL)
    {
        ESP_LOGW(TAG, "指令 %s 当前并未在运行，无需停止", target_cmd);
        return APEX_OK; // 幂等处理，没跑也算停成功了
    }

    // 3. 获取该指令的定义，执行硬件层的停止钩子
    const apex_cmd_entry_t *entry = apex_cmd_find_entry(target_cmd);
    if (entry && entry->stop_handler)
    {
        // 调用业务模块实现的 stop_handler，让硬件停下来（比如断开电机电源）
        entry->stop_handler(params, running_msg_id, res_data);
    }

    // 4. 🔥 核心优化：调用统一的终结方法
    // 这会自动给云端发送一条该指令已“completed”的消息，并释放槽位
    // code 传 APEX_ERR_STOPPED (-2 之类的自定义码) 或 0，取决于希望 App 如何显示
    apex_cmd_finish(running_msg_id, 0, NULL);

    ESP_LOGI(TAG, "已强制终止指令: %s", target_cmd);
    return APEX_OK;
}

// ==========================================
// 模块初始化与注册
// ==========================================
esp_err_t apex_stop_init(void)
{
    static char function_params_json_buf[1024];
    int count = sizeof(function_params) / sizeof(function_params[0]);

    // 调用函数，把 JSON 写进 buffer
    build_function_param_desc_json(function_params, count,
                                   function_params_json_buf, sizeof(function_params_json_buf));
    apex_cmd_entry_t entry = {
        .cmd_key = FUNCTION_KEY, // ✅ 模块名称：统一且干练
        .function_name = "通用停止指令",
        .function_desc = "强制停止指定的持续性动作或异步任务",
        .function_params = function_params_json_buf, // {"fun_key":"string"} — 指定要停止的目标指令 key
        .role = "admin",                             // 停止别的指令通常需要较高权限
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_FORCE, // ✅ 关键：必须是 FORCE，无视设备当前的 BUSY 状态
        .is_persistent = false,       // 停止指令本身是个瞬间动作
        .handler = apex_stop_handler, // ✅ 绑定的主逻辑
        .stop_handler = NULL          // 停止指令不需要被停止
    };

    // 注册到全局 Executor 路由表
    apex_cmd_register(entry);
    ESP_LOGI(TAG, "组件注册成功: %s (v%s)", entry.cmd_key, entry.version);
    return ESP_OK;
}
