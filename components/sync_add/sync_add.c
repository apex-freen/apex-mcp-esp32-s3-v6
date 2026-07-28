#define TAG "SYNC_ADD_LOG"
#include "esp_log.h"

// 引入框架核心头文件
#include "apex_cmd_executor.h"
#include "sync_add.h"

// 参数样例
// static const char* mode_values[] = {"cool", "heat", "fan", "dry", "auto"};

// static const function_param_desc_t function_params[] =
// {
//     {
//         .key         = "add",
//         .type        = "int",
//         .has_min     = 1, .min_val  = 0,
//         .has_max     = 1, .max_val  = 100,
//         .has_multipleOf    = 1, .multipleOf_val = 1,
//     },
//     {
//         .key         = "adder",
//         .type        = "int",
//         .has_min     = 1, .min_val  = 0,
//         .has_max     = 1, .max_val  = 200,
//     },
//     {
//         .key         = "mode",
//         .type        = "string",
//         .enum_vals   = mode_values,
//         .enum_count  = 5,
//         .has_default = 1, .default_val = 0,   // 0 = mode_values[0] = "cool"
//     },
// };

// ============================================================================
// 1. 常量与参数定义区 (防止硬编码，一处修改，全局生效)
// ============================================================================
// 指令注册名 (供路由使用)
static const char *FUNCTION_KEY = "sync_add";

// JSON 参数键名定义 (防止在提取参数和描述 Schema 时写错字)
#define KEY_PARAM_A "add"
#define KEY_PARAM_B "adder"
static const function_param_desc_t function_params[] =
    {
        {.key = KEY_PARAM_A,
         .type = "int",
         .description = "第一个加数",
         .has_min = 1,
         .min_val = 0,
         .has_max = 1,
         .max_val = 100,
         .has_multipleOf = 1,
         .multipleOf_val = 1,
         .unit = "celsius",
         .has_default = 1,
         .default_val = 50},
        {
            .key = KEY_PARAM_B,
            .type = "int",
            .description = "第二个加数",
            .has_min = 1,
            .min_val = 0,
            .has_max = 1,
            .max_val = 200,
        },
};

/**
 * @brief 同步加法 Handler
 * @note 同步方法直接在 Executor 线程中运行，不开启新任务
 */
static int sync_add_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    ESP_LOGI(TAG, "收到同步加法请求 [ID: %s]", msg_id);

    // 1. 参数校验与提取 (使用前面定义的宏，绝对不会和 Schema 对应错)
    if (params == NULL || !cJSON_IsObject(params))
    {
        ESP_LOGW(TAG, "参数格式错误，期望一个 JSON Object");
        return APEX_ERR_PARAM;
    }

    cJSON *item_a = cJSON_GetObjectItem(params, KEY_PARAM_A);
    cJSON *item_b = cJSON_GetObjectItem(params, KEY_PARAM_B);

    if (!cJSON_IsNumber(item_a) || !cJSON_IsNumber(item_b))
    {
        ESP_LOGW(TAG, "参数缺失或类型错误: 需要 %s(int) 和 %s(int)", KEY_PARAM_A, KEY_PARAM_B);
        return APEX_ERR_PARAM;
    }

    int val1 = item_a->valueint;
    int val2 = item_b->valueint;

    // 2. 立即执行计算 (没有 Delay，没有 Task)
    int sum = val1 + val2;

    // 3. 构造结果对象，框架会将 *res_data 包装进统一响应格式并加密发出
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "result", sum);
    *res_data = data;

    return APEX_OK; // 同步指令执行完成，框架自动发送结果并解锁
}

// ============================================================================
// 5. 组件注册入口 (在 app_main 中由框架统一调用)
// ============================================================================
void sync_add_init(void)
{
    static char function_params_json_buf[1024];
    int count = sizeof(function_params) / sizeof(function_params[0]);

    // 调用函数，把 JSON 写进 buffer
    build_function_param_desc_json(function_params, count,
                                   function_params_json_buf, sizeof(function_params_json_buf));
    apex_cmd_entry_t entry = {
        .cmd_key = FUNCTION_KEY,
        .function_name = "同步加法计算",
        .function_desc = "同步加法：接收两个参数，立即返回计算结果",
        .function_params = function_params_json_buf,
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_PARALLEL, // 并行指令，不锁定系统，可与其他指令同时执行
        .handler = sync_add_handler,
        .is_persistent = false, // 非持久化动作，返回即结束
        .stop_handler = NULL,   // 同步指令无需停止回调
    };

    // 注册到全局 Executor 路由表
    apex_cmd_register(entry);
    ESP_LOGI(TAG, "组件注册成功: %s (v%s)", entry.cmd_key, entry.version);
}
