#define TAG "ASYNC_ADD_LOG"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

// 引入框架核心头文件 (包含注册接口和响应助手)
#include "apex_cmd_executor.h"
#include "async_add.h"
#include "cJSON.h"

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
static const char *FUNCTION_KEY = "async_add";
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

// ============================================================================
// 2. 异步任务专用的上下文结构体
// ============================================================================
typedef struct
{
    int a;           // 业务参数 A
    int b;           // 业务参数 B
    char msg_id[48]; // 【必须】用于保存原始请求的唯一标识，用于异步回调
} add_async_ctx_t;

// ============================================================================
// 3. 异步后台处理任务 (真正的耗时业务逻辑写在这里)
// ============================================================================
static void add_async_task(void *pvParameters)
{
    // 1. 获取并接管上下文数据
    add_async_ctx_t *ctx = (add_async_ctx_t *)pvParameters;
    ESP_LOGI(TAG, "异步计算开始: %d + %d (ID: %s)", ctx->a, ctx->b, ctx->msg_id);

    // 2. 执行真正的业务逻辑 (模拟耗时操作，如读取传感器、等待外设、网络请求等)
    vTaskDelay(pdMS_TO_TICKS(100000)); // 100 秒后才返回结果，用于测试异步长任务的中间状态
    int result_value = ctx->a + ctx->b;

    // 3. 回传最终结果并释放状态机
    // apex_cmd_finish 内部会调用 apex_cmd_send_async_done 发送响应，并自动清理槽位
    cJSON *res_data = cJSON_CreateObject();
    cJSON_AddNumberToObject(res_data, "result", result_value);
    apex_cmd_finish(ctx->msg_id, APEX_OK, res_data);
    free(ctx);
    ESP_LOGI(TAG, "异步计算完成并已回调回传结果");
    vTaskDelete(NULL);
}

// ============================================================================
// 4. 标准指令处理函数 (Handler - 运行在 Executor 的上下文中)
// ============================================================================
static int apex_cmd_add_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    ESP_LOGI(TAG, "收到 %s 指令，准备解析参数", FUNCTION_KEY);

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

    // 2. 组装异步上下文并分配内存
    // 【规范】推荐使用 calloc (1, size)，它能保证分配的内存被自动清零，避免脏数据
    add_async_ctx_t *ctx = calloc(1, sizeof(add_async_ctx_t));
    if (!ctx)
    {
        ESP_LOGE(TAG, "内存分配失败");
        return APEX_ERR_SYS;
    }

    // 填充业务参数
    ctx->a = item_a->valueint;
    ctx->b = item_b->valueint;

    // 【核心操作】安全拷贝 msg_id 到异步上下文
    // 使用 strlcpy 可以自动保证字符串以 '\0' 结尾，比 strncpy 更安全 (ESP-IDF 支持)
    if (msg_id != NULL)
    {
        strlcpy(ctx->msg_id, msg_id, sizeof(ctx->msg_id));
    }

    // 3. 创建 FreeRTOS 异步任务
    // 参数说明：任务函数, 任务名, 栈大小(4096字节通常够用), 参数指针, 优先级, 任务句柄
    BaseType_t ret = xTaskCreate(add_async_task, "task_add", 4096, ctx, 5, NULL);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "创建后台任务失败");
        free(ctx);
        return APEX_ERR_SYS;
    }

    return APEX_ASYNC_OK;
}

// ============================================================================
// 5. 组件注册入口 (在 app_main 中由框架统一调用)
// ============================================================================
void async_add_init(void)
{
    static char function_params_json_buf[1024];
    int count = sizeof(function_params) / sizeof(function_params[0]);

    // 调用函数，把 JSON 写进 buffer
    build_function_param_desc_json(function_params, count,
                                   function_params_json_buf, sizeof(function_params_json_buf));
    apex_cmd_entry_t entry = {
        .cmd_key = FUNCTION_KEY,
        .function_name = "异步加法计算",
        .function_desc = "模拟耗时操作：接收两个参数，延迟100秒后返回加法结果",
        .function_params = function_params_json_buf,
        .role = "user",
        .version = "1.0.1",
        .handler = apex_cmd_add_handler};

    // 注册到全局 Executor 路由表
    apex_cmd_register(entry);
    ESP_LOGI(TAG, "组件注册成功: %s (v%s)", entry.cmd_key, entry.version);
}
