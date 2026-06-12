#define TAG "SYNC_ADD_LOG"
#include "esp_log.h"

// 引入框架核心头文件
#include "apex_cmd_executor.h"
#include "sync_add.h"

// ============================================================================
// 1. 常量与参数定义区 (防止硬编码，一处修改，全局生效)
// ============================================================================
// 指令注册名 (供路由使用)
static const char *FUNCTION_KEY = "sync_add";

// JSON 参数键名定义 (防止在提取参数和描述 Schema 时写错字)
#define KEY_PARAM_A "add"
#define KEY_PARAM_B "adder"
// 利用 C 语言的宏拼接特性，自动生成自描述的 JSON Schema 字符串
// 展开后等价于: "{\"add\":\"int\", \"adder\":\"int\"}"
#define PARAM_SCHEMA "{\"" KEY_PARAM_A "\":\"int\", \"" KEY_PARAM_B "\":\"int\"}"

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
    apex_cmd_entry_t entry = {
        .cmd_key = FUNCTION_KEY,
        .function_name = "同步加法计算",
        .function_desc = "同步加法：接收两个参数，立即返回计算结果",
        .function_params = PARAM_SCHEMA, // 自动引用上面拼接好的 Schema 常量
        .role = "user",
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
