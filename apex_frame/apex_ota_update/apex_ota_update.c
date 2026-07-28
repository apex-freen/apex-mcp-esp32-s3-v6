#define TAG "APEX_OTA_LOG"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

#include "esp_ota_ops.h"
#include "esp_https_ota.h"

#include "apex_cmd_executor.h"
#include "apex_ota_update.h"
#include "cJSON.h"

#define FUNCTION_KEY "apexOtaUpdate"
#define KEY_PARAM_A "url"
static const function_param_desc_t function_params[] =
    {
        {
            .key = KEY_PARAM_A, // 宏展开是 "add"
            .type = "string",
            // 其他字段不写，编译器自动置 0/NULL
        },
};

static esp_err_t do_http_ota(const char *url)
{
    esp_http_client_config_t http_config = {
        .url = url,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&http_config);

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE("APEX_OTA", "HTTP 连接失败");
        esp_http_client_cleanup(client);
        return err;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0)
    {
        ESP_LOGE("APEX_OTA", "获取 Content-Length 失败");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    ESP_LOGI("APEX_OTA", "固件大小: %d 字节", content_length);

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition)
    {
        ESP_LOGE("APEX_OTA", "找不到可用的 OTA 分区");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    esp_ota_handle_t ota_handle;
    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE("APEX_OTA", "OTA 开始失败");
        esp_http_client_cleanup(client);
        return err;
    }

    char *buffer = malloc(4096);
    if (!buffer)
    {
        esp_ota_abort(ota_handle);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    int total_read = 0;
    int last_pct = -1;
    while (total_read < content_length)
    {
        int read_len = esp_http_client_read(client, buffer, 4096);
        if (read_len <= 0)
        {
            ESP_LOGE("APEX_OTA", "读取数据失败");
            err = ESP_FAIL;
            break;
        }
        err = esp_ota_write(ota_handle, buffer, read_len);
        if (err != ESP_OK)
        {
            ESP_LOGE("APEX_OTA", "写入 Flash 失败");
            break;
        }
        total_read += read_len;
        int pct = (total_read * 100) / content_length;
        if (pct != last_pct)
        {
            ESP_LOGI("APEX_OTA", "下载进度: %d%%", pct);
            last_pct = pct;
        }
    }

    free(buffer);

    if (err != ESP_OK)
    {
        esp_ota_abort(ota_handle);
        esp_http_client_cleanup(client);
        return err;
    }

    err = esp_ota_end(ota_handle);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK)
    {
        ESP_LOGE("APEX_OTA", "OTA 结束校验失败");
        return err;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE("APEX_OTA", "设置启动分区失败");
    }
    return err;
}

static void apex_ota_task(void *pvParameters)
{
    apex_ota_ctx_t *ctx = (apex_ota_ctx_t *)pvParameters;
    ESP_LOGI("APEX_OTA", "开始从 URL 下载固件: %s", ctx->url);

    esp_err_t ret;
    bool is_https = (strncmp(ctx->url, "https://", 8) == 0);

    if (is_https)
    {
        esp_http_client_config_t config = {
            .url = ctx->url,
            .keep_alive_enable = true,

#if CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
            .skip_cert_common_name_check = true,
#endif
        };
        esp_https_ota_config_t ota_config = {
            .http_config = &config,
        };
        ret = esp_https_ota(&ota_config);
    }
    else
    {
        ret = do_http_ota(ctx->url);
    }

    if (ret == ESP_OK)
    {
        ESP_LOGI("APEX_OTA", "固件下载并校验成功，准备重启...");
        apex_cmd_finish(ctx->msg_id, APEX_OK, NULL);
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
    else
    {
        ESP_LOGE("APEX_OTA", "OTA 升级失败: %s", esp_err_to_name(ret));
        apex_cmd_finish(ctx->msg_id, APEX_ERR_SYS, NULL);
    }

    free(ctx);
    vTaskDelete(NULL);
}

// 指令：sys_update
// 参数：{"url": "https://xxx.bin"}
int apex_ota_update_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{

    int ret_code = APEX_ERR_SYS;
    apex_ota_ctx_t *ctx = calloc(1, sizeof(apex_ota_ctx_t));

    // 1. 提取参数
    cJSON *item_url = cJSON_GetObjectItem(params, KEY_PARAM_A);
    if (!cJSON_IsString(item_url))
    {
        ret_code = APEX_ERR_PARAM;
        goto error_exit; // 出错，跳到出口清理
    }

    // ✅ 新增：拦截超长 URL
    if (strlen(item_url->valuestring) >= 256)
    {
        ESP_LOGE("APEX_OTA", "URL超长");
        ret_code = APEX_ERR_PARAM;
        goto error_exit; // 出错，跳到出口清理
    }

    // 3. 准备异步上下文 (malloc 并在任务中释放)

    if (!ctx)
    {
        ret_code = APEX_ERR_SYS;
        goto error_exit;
    }

    strlcpy(ctx->url, item_url->valuestring, sizeof(ctx->url));
    strlcpy(ctx->msg_id, msg_id, sizeof(ctx->msg_id));

    // 4. 创建 OTA 任务
    // 建议分配至少 8KB 栈空间，因为 HTTPS 和 TLS 非常吃内存
    BaseType_t ret = xTaskCreate(apex_ota_task, "apex_ota_task", 8192, ctx, 5, NULL);

    if (ret != pdPASS)
    {
        ret_code = APEX_ERR_SYS;
        goto error_exit;
    }

    // 返回异步开始状态
    return APEX_ASYNC_OK;

    // ========== 统一错误清理出口 ==========
error_exit:
    if (ctx)
    {
        free(ctx);
    }
    apex_cmd_finish(msg_id, ret_code, NULL); // 只要是错的，统一在这里解锁
    return ret_code;
}

// ============================================================================
// 5. 组件注册入口 (在 app_main 中由框架统一调用)
// ============================================================================
esp_err_t apex_ota_update_init(void)
{
    static char function_params_json_buf[1024];
    int count = sizeof(function_params) / sizeof(function_params[0]);

    // 调用函数，把 JSON 写进 buffer
    build_function_param_desc_json(function_params, count,
                                   function_params_json_buf, sizeof(function_params_json_buf));
    apex_cmd_entry_t entry = {
        .cmd_key = FUNCTION_KEY,
        .function_name = "固件升级",
        .function_desc = "固件升级到最新版本",
        .function_params = function_params_json_buf,
        .role = "admin",
        .risk_level = "auth",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_EXCLUSIVE, // ✅ 独占指令，升级的时候 ，不允许其他动作进来
        .is_persistent = true,
        .handler = apex_ota_update_handler,
        .stop_handler = NULL};

    apex_cmd_register(entry);
    ESP_LOGI(TAG, "组件注册成功: %s (v%s)", entry.cmd_key, entry.version);
    return ESP_OK;
}
