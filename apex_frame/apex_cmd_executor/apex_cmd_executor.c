#define TAG "APEX_CMD_EXECUTOR"
#include "apex_cmd_executor.h"
#include <string.h>
#include "cJSON.h"
#include "esp_log.h"
#include "apex_config.h"
#include "mbedtls/base64.h"
#include "psa/crypto.h"
#include "apex_mqtt.h"
#include "apex_crypto.h"
#include "esp_task_wdt.h"
#include "apex_get_state.h"
#include "apex_ota_update.h"
#include "apex_stop.h"
#include "apex_power_down.h"
#include "apex_power_up.h"
#include "apex_reset.h"
#include "apex_restart.h"

static apex_cmd_entry_t s_cmd_table[30]; // 预留30个功能位
static int s_cmd_count = 0;

// 故障分级：失败计数 & 熔断降级 & 重复拦截
#define FAULT_MAX_FAILURES 5             // 连续失败次数阈值
#define FAULT_DEGRADE_MS (5 * 60 * 1000) // 降级恢复时间 (5分钟)
#define DEDUP_WINDOW_MS 2000             // 重复指令拦截窗口 (2秒)
#define DEDUP_CACHE_SIZE 16              // 最近 N 条指令缓存

typedef struct
{
    char cmd_key[32];
    int fail_count;
    TickType_t degraded_until; // 0=正常, >0=降级到期 tick
} fault_tracker_t;

static fault_tracker_t s_fault_trackers[30];
static int s_fault_count = 0;

// 重复指令拦截缓存
typedef struct
{
    char msg_id[64];
    TickType_t received_at; // 收到时刻 (tick)
} dedup_entry_t;

static dedup_entry_t s_dedup_cache[DEDUP_CACHE_SIZE];
static int s_dedup_index = 0;

static bool dedup_check_and_record(const char *msg_id)
{
    if (!msg_id || msg_id[0] == '\0')
        return false; // 无 msg_id 不过滤

    TickType_t now = xTaskGetTickCount();

    for (int i = 0; i < DEDUP_CACHE_SIZE; i++)
    {
        if (s_dedup_cache[i].msg_id[0] == '\0')
            continue;
        if (strcmp(s_dedup_cache[i].msg_id, msg_id) == 0)
        {
            if ((now - s_dedup_cache[i].received_at) * portTICK_PERIOD_MS < DEDUP_WINDOW_MS)
                return true; // 命中：重复
            break;           // 命中但超时：覆写
        }
    }

    // 缓存当前 msg_id（环形写入）
    int idx = s_dedup_index % DEDUP_CACHE_SIZE;
    strncpy(s_dedup_cache[idx].msg_id, msg_id, sizeof(s_dedup_cache[idx].msg_id) - 1);
    s_dedup_cache[idx].received_at = now;
    s_dedup_index++;
    return false;
}

static fault_tracker_t *fault_tracker_get(const char *cmd_key)
{
    if (!cmd_key)
        return NULL;
    for (int i = 0; i < s_fault_count; i++)
    {
        if (strcmp(s_fault_trackers[i].cmd_key, cmd_key) == 0)
            return &s_fault_trackers[i];
    }
    // 新条目
    if (s_fault_count >= 30)
        return NULL;
    strncpy(s_fault_trackers[s_fault_count].cmd_key, cmd_key, sizeof(s_fault_trackers[0].cmd_key) - 1);
    return &s_fault_trackers[s_fault_count++];
}

// 设备指令状态设计
apex_state_manager_t g_apex_state;

/**
 * @brief 将 APEX 错误码转换为人类可读的消息
 */
const char *apex_err_to_msg(int code)
{
    switch (code)
    {
    case APEX_OK:
        return "success";
    case APEX_ASYNC_OK:
        return "processing";
    case APEX_ERR_PARAM:
        return "invalid parameters";
    case APEX_ERR_SYS:
        return "internal system error";
    case APEX_ERR_NOT_FOUND:
        return "function not found";
    case APEX_ERR_STANDBY:
        return "device is in standby mode"; // -401
    case APEX_ERR_BUSY:
        return "System Busy";
    case APEX_ERR_NOT_SUPPORTED:
        return "operation not supported";
    case APEX_ERR_OVERFLOW:
        return "too many active commands";
    case APEX_ERR_TIMEOUT:
        return "system command timeout";
    case APEX_ERR_DEGRADED:
        return "command degraded due to consecutive failures";
    case APEX_ERR_DUPLICATE:
        return "duplicate command, ignored within dedup window";
    default:
        return (code < 0) ? "unknown error" : "success";
    }
}

void apex_cmd_send_async_processing(const char *func_key, const char *msg_id, int code)
{
    // 构造异步任务的 Data 负载
    cJSON *data = cJSON_CreateObject();
    if (!data)
        return;
    cJSON_AddStringToObject(data, "mode", "async");
    cJSON_AddStringToObject(data, "status", "processing");

    // 直接复用万能发送器
    apex_cmd_send_response(func_key, msg_id, code, data);
}

void apex_cmd_send_async_done(const char *func_key, const char *msg_id, int code, cJSON *result_obj)
{
    cJSON *data = cJSON_CreateObject();
    if (!data)
    {
        if (result_obj)
            cJSON_Delete(result_obj); // 内存申请失败也要保护外部传入的对象
        return;
    }

    cJSON_AddStringToObject(data, "mode", "async");
    cJSON_AddStringToObject(data, "status", "completed");

    // 统一的结果挂载逻辑
    if (result_obj)
    {
        cJSON_AddItemToObject(data, "result", result_obj);
    }
    else
    {
        cJSON_AddNullToObject(data, "result");
    }

    // 1. 发送响应
    apex_cmd_send_response(func_key, msg_id, code, data);

    // 2. ✅ 自动化：既然任务已 Done，自动从登记表中移除该 msg_id
    // apex_state_unlock(msg_id);
}

void apex_cmd_send_sync(const char *func_key, const char *msg_id, int code, cJSON *result_obj)
{
    ESP_LOGI(TAG, "apex_cmd_send_sync");
    cJSON *data = cJSON_CreateObject();
    if (!data)
    {
        if (result_obj)
            cJSON_Delete(result_obj);
        return;
    }

    cJSON_AddStringToObject(data, "mode", "sync");
    cJSON_AddStringToObject(data, "status", "completed");

    // 保持与 async_done 一致的健壮性
    if (result_obj)
    {
        cJSON_AddItemToObject(data, "result", result_obj);
    }
    else
    {
        cJSON_AddNullToObject(data, "result");
    }

    // 1. 发送响应
    apex_cmd_send_response(func_key, msg_id, code, data);

    // 2. ✅ 自动化：同步指令执行完立刻解锁
    // apex_state_unlock(msg_id);
}

/**
 * @brief 核心发送函数：负责构造标准外壳、序列化、加密并发布 MQTT
 * @param func_key 方法名
 * @param msg_id 消息ID
 * @param status_code 状态码
 * @param res_data 业务数据负载（cJSON对象，函数内部会接管并自动销毁）
 */
void apex_cmd_send_response(const char *func_key, const char *msg_id, int status_code, cJSON *res_data)
{

    ESP_LOGI(TAG, "apex_cmd_send_response");
    // 1. 构造统一的 Response 报文结构
    cJSON *reply = cJSON_CreateObject();
    cJSON_AddStringToObject(reply, "function_key", func_key ? func_key : "unknown");
    cJSON_AddStringToObject(reply, "msg_id", msg_id ? msg_id : "");
    cJSON_AddNumberToObject(reply, "code", status_code);
    // ✅ 自动化映射错误消息
    cJSON_AddStringToObject(reply, "msg", apex_err_to_msg(status_code));

    // 挂载数据：如果没有数据，也必须传 null 以保持 JSON 结构一致
    if (res_data)
    {
        // cJSON_AddItemToObject(reply, "dev_data", res_data);
        cJSON_AddItemToObject(reply, "result", res_data);
    }
    else
    {
        // cJSON_AddNullToObject(reply, "dev_data");
        cJSON_AddNullToObject(reply, "result");
    }

    // 2. 序列化为字符串
    char *reply_plain = cJSON_PrintUnformatted(reply);

    if (reply_plain)
    {
        size_t plaintext_len = strlen(reply_plain);
        // 计算所需空间：Nonce(13) + Timestamp(8) + Plaintext + MAC(16)
        size_t needed_size = 13 + 8 + plaintext_len + 16;

        uint8_t *out_buf = (uint8_t *)malloc(needed_size);
        size_t out_len = 0;

        if (out_buf)
        {
            if (payload_crypto_encrypt((uint8_t *)reply_plain, plaintext_len, out_buf, &out_len) == ESP_OK)
            {
                const mqtt_topics_t *topics = mqtt_topics_get();
                ESP_LOGI(TAG, "payload_crypto_encrypt OK");
                // ✅ 正确做法：显式传递加密后的真实长度 out_len
                // 并补齐 qos 和 retain 参数（根据函数定义）
                apex_mqtt_publish(topics->response, (const char *)out_buf, (int)out_len, 1, 0);

                ESP_LOGD(TAG, "加密报文已发出，总长度: %d 字节", out_len);
            }
            free(out_buf);
        }
        free(reply_plain);
    }
    // 5. 递归销毁整个 cJSON 树（包括传入的 res_data）
    cJSON_Delete(reply);
}

/**
 * @brief 设备主动通知：按 JSON-RPC 2.0 Notification 格式加密发布到 MQTT notice topic
 * @param func_key 触发通知的指令标识
 * @param event    事件类型 (如 "reset", "motor_started")
 * @param data     事件附带数据 (cJSON 对象，函数内部接管并销毁)
 *
 * 输出为 JSON-RPC 2.0 通知 (无 id，服务端不回复):
 * {
 *   "jsonrpc": "2.0",
 *   "method": "device.notify",
 *   "params": {
 *     "function_key": "reSet",
 *     "event": "reset",
 *     "data": {...}
 *   }
 * }
 */
void apex_cmd_send_notify(const char *func_key, const char *event, cJSON *data)
{
    ESP_LOGI(TAG, "apex_cmd_send_notify: [%s] event=%s", func_key ? func_key : "unknown", event ? event : "");

    // 构造 JSON-RPC 2.0 Notification
    cJSON *notify = cJSON_CreateObject();
    cJSON_AddStringToObject(notify, "jsonrpc", "2.0");
    cJSON_AddStringToObject(notify, "method", "device.notify");

    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "function_key", func_key ? func_key : "unknown");
    cJSON_AddStringToObject(params, "event", event ? event : "");
    if (data)
        cJSON_AddItemToObject(params, "data", data);
    else
        cJSON_AddNullToObject(params, "data");
    cJSON_AddItemToObject(notify, "params", params);

    char *notify_plain = cJSON_PrintUnformatted(notify);
    if (notify_plain)
    {
        size_t plaintext_len = strlen(notify_plain);
        size_t needed_size = 13 + 8 + plaintext_len + 16;
        uint8_t *out_buf = (uint8_t *)malloc(needed_size);
        size_t out_len = 0;

        if (out_buf)
        {
            if (payload_crypto_encrypt((uint8_t *)notify_plain, plaintext_len, out_buf, &out_len) == ESP_OK)
            {
                const mqtt_topics_t *topics = mqtt_topics_get();
                apex_mqtt_publish(topics->notice, (const char *)out_buf, (int)out_len, 1, 0);
                ESP_LOGD(TAG, "notify 已 JSON-RPC 发布到 %s, 长度: %d", topics->notice, out_len);
            }
            free(out_buf);
        }
        free(notify_plain);
    }
    cJSON_Delete(notify);
}

/**
 * @brief 核心指令分发与执行函数
 * @param json_raw 解密后的明文字符串
 */
void apex_cmd_executor(const char *json_raw)
{
    if (json_raw == NULL)
        return;

    cJSON *root = cJSON_Parse(json_raw);
    if (!root)
    {
        ESP_LOGE(TAG, "JSON 解析失败");
        apex_cmd_send_notify("unknown", "parse_error", NULL);
        return;
    }

    // 1. 协议解析
    cJSON *func_item = cJSON_GetObjectItem(root, "function_key");
    cJSON *msg_id_item = cJSON_GetObjectItem(root, "msg_id");

    if (!cJSON_IsString(func_item) || !cJSON_IsString(msg_id_item))
    {
        ESP_LOGW(TAG, "参数格式不对，function_key 或 msg_id 不是字符串");
        goto cleanup;
    }

    const char *func_key = func_item->valuestring;
    const char *msg_id = msg_id_item->valuestring;

    // 2. 重复指令拦截：同一个 msg_id 在窗口内再次收到直接拒绝
    if (dedup_check_and_record(msg_id))
    {
        ESP_LOGW(TAG, "重复指令 [%s] (%s), 窗口内拦截", func_key, msg_id);
        apex_cmd_send_sync(func_key, msg_id, APEX_ERR_DUPLICATE, NULL);
        goto cleanup;
    }

    cJSON *params_to_pass = cJSON_GetObjectItem(root, "function_params");

    // 3. 获取指令定义
    const apex_cmd_entry_t *entry = apex_cmd_find_entry(func_key);
    if (!entry)
    {
        ESP_LOGW(TAG, "指令未找到: %s", func_key);
        apex_cmd_send_sync(func_key, msg_id, APEX_ERR_NOT_FOUND, NULL);
        cJSON *nd = cJSON_CreateObject();
        cJSON_AddStringToObject(nd, "reason", "function_not_found");
        apex_cmd_send_notify(func_key, "command_failed", nd);
        goto cleanup;
    }

    // 4. 入场登记 (Lock)
    int lock_ret = apex_state_lock(entry->flags, func_key, msg_id);
    if (lock_ret != APEX_OK)
    {
        ESP_LOGW(TAG, "指令 [%s] 登记失败 (Code: %d), 终止执行", func_key, lock_ret);
        apex_cmd_send_sync(func_key, msg_id, lock_ret, NULL);
        goto cleanup;
    }

    // 5. 熔断检查：指令连续失败 N 次则降级
    fault_tracker_t *ft = fault_tracker_get(func_key);
    if (ft && ft->degraded_until > 0)
    {
        if (xTaskGetTickCount() < ft->degraded_until)
        {
            ESP_LOGW(TAG, "指令 [%s] 已熔断降级，剩余: %d 秒",
                     func_key, (int)((ft->degraded_until - xTaskGetTickCount()) * portTICK_PERIOD_MS / 1000));
            apex_cmd_send_sync(func_key, msg_id, APEX_ERR_DEGRADED, NULL);
            goto cleanup;
        }
        else
        {
            ESP_LOGI(TAG, "指令 [%s] 降级到期，自动恢复", func_key);
            ft->degraded_until = 0;
            ft->fail_count = 0;
        }
    }

    // 6. 执行业务逻辑
    cJSON *res_data = NULL;
    ESP_LOGI(TAG, "执行功能: %s [%s]  [%s]", entry->function_name, func_key, msg_id);
    if (entry->flags == APEX_CMD_FLAG_FORCE)
    {
        ESP_LOGW(TAG, "⚠ FORCE 指令: %s [%s]", entry->function_name, func_key);
    }
    if (entry->flags == APEX_CMD_FLAG_ALWAYS_ALLOWED)
    {
        ESP_LOGI(TAG, "常驻开放指令: %s [%s]", entry->function_name, func_key);
    }
    int status_code = entry->handler(params_to_pass, msg_id, &res_data);

    // 故障分级：记录错误 & 通知 & 超额熔断
    if (ft && status_code < 0 && status_code != APEX_ASYNC_OK)
    {
        ft->fail_count++;
        ESP_LOGW(TAG, "指令 [%s] 执行失败 (code=%d), 累计: %d/%d",
                 func_key, status_code, ft->fail_count, FAULT_MAX_FAILURES);

        // 通知服务端：指令执行失败
        cJSON *nd = cJSON_CreateObject();
        cJSON_AddNumberToObject(nd, "code", status_code);
        cJSON_AddStringToObject(nd, "reason", apex_err_to_msg(status_code));
        cJSON_AddNumberToObject(nd, "consecutive_failures", ft->fail_count);
        apex_cmd_send_notify(func_key, "command_failed", nd);
        if (ft->fail_count >= FAULT_MAX_FAILURES)
        {
            ft->degraded_until = xTaskGetTickCount() + pdMS_TO_TICKS(FAULT_DEGRADE_MS);
            ESP_LOGE(TAG, "指令 [%s] 已熔断降级 %d 分钟", func_key, FAULT_DEGRADE_MS / 60000);
            cJSON *notify_data = cJSON_CreateObject();
            cJSON_AddStringToObject(notify_data, "reason", "consecutive_failures");
            cJSON_AddNumberToObject(notify_data, "fail_count", ft->fail_count);
            cJSON_AddNumberToObject(notify_data, "degrade_minutes", FAULT_DEGRADE_MS / 60000);
            apex_cmd_send_notify(func_key, "degraded", notify_data);
        }
    }
    else if (ft && status_code >= 0)
    {
        ft->fail_count = 0; // 成功时清零
    }

    // ==========================================
    // 7. 核心：响应分发与状态流转 (生命周期终点)
    // ==========================================
    if (status_code == APEX_ASYNC_OK)
    {
        // 类别 A：异步长任务
        // 动作：发送处理中状态。
        // 锁状态：保持锁定！(由业务层的 apex_cmd_send_async_done 自行负责调用 unlock)
        ESP_LOGI(TAG, "异步长任务: %s [%s] [%s]", entry->function_name, func_key, msg_id);
        // apex_state_unlock(msg_id);// 只发送“处理中”消息。开发者后续在业务代码里调 apex_cmd_finish 来解锁。
        apex_cmd_send_async_processing(func_key, msg_id, APEX_OK);
    }
    else if (entry->is_persistent && status_code == APEX_OK)
    {
        // 类别 B：持久化动作 (且启动成功)
        // 动作：发送执行成功。
        // 锁状态：✅ 保持锁定！物理硬件还在动，把槽位占住，留给 apex_stop 来注销。
        ESP_LOGI(TAG, "同步长任务: %s [%s] [%s]", entry->function_name, func_key, msg_id);
        // apex_state_unlock(msg_id);// 发送同步成功响应，但槽位依然占着，直到开发者调 finish 或收到 stop。
        apex_cmd_send_sync(func_key, msg_id, status_code, res_data);
    }
    else
    {
        // 类别 C：普通同步指令 / 任何执行失败的指令
        // 动作：发送最终结果，立即释放锁
        ESP_LOGI(TAG, "同步指令完成: %s [%s] [%s] (code: %d)", entry->function_name, func_key, msg_id, status_code);
        apex_state_unlock(msg_id);
        apex_cmd_send_sync(func_key, msg_id, status_code, res_data);
    }

cleanup:
    if (root)
        cJSON_Delete(root);
}

void apex_process_incoming_cmd(const char *b64_cipher_text, size_t b64_len)
{
    if (b64_cipher_text == NULL || b64_len == 0)
        return;

    // 1. 准备 Base64 解码后的缓冲区
    // Base64 解码后长度约为原长的 3/4，直接用 b64_len 长度肯定够
    size_t decoded_len = 0;
    uint8_t *binary_cipher = malloc(b64_len);

    // 2. Base64 解码：将字符串转回二进制
    int ret = mbedtls_base64_decode(binary_cipher, b64_len, &decoded_len,
                                    (const unsigned char *)b64_cipher_text, b64_len);

    // ESP_LOGI("CORE", "Base64 解码成功,内容: %s", b64_cipher_text);
    if (ret != 0)
    {
        ESP_LOGE("CORE", "Base64 解码失败");
        free(binary_cipher);
        return;
    }

    // 3. 准备明文缓冲区
    uint8_t *decrypted_data = (uint8_t *)calloc(1, decoded_len + 1);
    size_t actual_plain_len = 0;

    // 2. 调用 crypto 组件进行解密
    // 传入 &actual_plain_len 让函数写回真实长度
    if (payload_crypto_decrypt(binary_cipher, decoded_len, decrypted_data, &actual_plain_len) == ESP_OK)
    {
        // ✅ 关键步骤：手动补上字符串结束符，确保 JSON 解析器不会越界
        decrypted_data[actual_plain_len] = '\0';

        ESP_LOGI("CORE", "指令安全解密成功,内容:%s (长度: %d)，进入解析器...", decrypted_data, actual_plain_len);

        // 3. 将解密后的明文交给指令解析器
        apex_cmd_executor((const char *)decrypted_data);
    }
    else
    {
        ESP_LOGE("CORE", "指令解密失败，可能存在重放攻击或 MAC 校验不匹配！");
    }

    // 4. 释放内存
    free(binary_cipher);
    free(decrypted_data);
}

void apex_cmd_register(apex_cmd_entry_t entry)
{
    if (s_cmd_count < 30)
    {
        s_cmd_table[s_cmd_count++] = entry;
    }
    else
    {
        ESP_LOGE(TAG, "指令注册表已满 (30/30)! 指令 [%s] 注册失败，请扩充 s_cmd_table", entry.cmd_key);
    }
}

/**
 * @brief 自描述查询方法 (内部接口)
 * 修改点：返回 cJSON 对象指针，不再返回字符串，由 executor 统一负责打印和释放。
 */
cJSON *apex_cmd_getinfo(void)
{
    cJSON *data_obj = cJSON_CreateObject();

    // --- 1. 填充设备基础信息 (通常来自全局配置结构体 g_apex_config) ---
    cJSON_AddStringToObject(data_obj, "dev_desc", g_apex_config.device.device_desc);
    cJSON_AddStringToObject(data_obj, "dev_name", g_apex_config.device.device_name); // 假设配置里有
    cJSON_AddStringToObject(data_obj, "dev_pwd", g_apex_config.device.device_pwd);

    // --- 2. 构造工具列表 (tools) —— MCP 标准格式 ---
    cJSON *tools_array = cJSON_CreateArray();
    for (int i = 0; i < s_cmd_count; i++)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", s_cmd_table[i].cmd_key);
        cJSON_AddStringToObject(item, "description", s_cmd_table[i].function_desc);
        cJSON_AddStringToObject(item, "function_name", s_cmd_table[i].function_name); // 保留中文展示名

        // 处理 inputSchema
        if (s_cmd_table[i].function_params && strlen(s_cmd_table[i].function_params) > 0)
        {
            cJSON *params_def = cJSON_Parse(s_cmd_table[i].function_params);
            if (params_def)
            {
                cJSON_AddItemToObject(item, "inputSchema", params_def);
            }
            else
            {
                // 解析失败（如 buffer 过小导致截断），输出最小合法 schema
                ESP_LOGW(TAG, "function_params JSON 解析失败 for [%s], 使用默认空 schema",
                         s_cmd_table[i].cmd_key);
                cJSON *empty_schema = cJSON_CreateObject();
                cJSON_AddStringToObject(empty_schema, "type", "object");
                cJSON *empty_props = cJSON_CreateObject();
                cJSON_AddItemToObject(empty_schema, "properties", empty_props);
                cJSON_AddItemToObject(item, "inputSchema", empty_schema);
            }
        }
        else
        {
            cJSON_AddNullToObject(item, "inputSchema");
        }

        cJSON_AddStringToObject(item, "role", s_cmd_table[i].role);
        cJSON_AddStringToObject(item, "version", s_cmd_table[i].version);
        cJSON_AddItemToArray(tools_array, item);
    }

    // --- 3. 将工具数组挂载到 data 对象中 ---
    cJSON_AddItemToObject(data_obj, "tools", tools_array);

    return data_obj;
}

/**
 * @brief getInfo 指令的 Handler
 */
int apex_cmd_getinfo_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    // 直接获取当前注册表的所有功能
    *res_data = apex_cmd_getinfo();

    // 如果获取失败（虽然理论上不会，除非内存炸了）
    if (*res_data == NULL)
        return APEX_ERR_SYS;

    return APEX_OK; // 执行成功
}

esp_err_t apex_state_init(void)
{
    // 1. 全局清零：确保所有布尔值、计数器、槽位信息初始都为 0 (false/NULL)
    memset(&g_apex_state, 0, sizeof(apex_state_manager_t));

    // 2. 显式设置初始状态
    g_apex_state.current_state = APEX_STATE_IDLE;

    // 3. 创建互斥锁（Mutex），自动处理优先级翻转，保护状态表
    if (g_apex_state.lock == NULL)
    {
        g_apex_state.lock = xSemaphoreCreateMutex();
        if (g_apex_state.lock == NULL)
        {
            ESP_LOGE("STATE", "致命错误：无法创建状态锁，内存不足！");
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_LOGI("STATE", "Atom 状态管理器初始化成功 (锁句柄: %p)", g_apex_state.lock);
    return ESP_OK;
}

esp_err_t apex_crypto_init_with_pwd(const char *pwd);

esp_err_t apex_cmd_executor_init(void)
{

    ESP_ERROR_CHECK(apex_crypto_init_with_pwd(g_apex_config.device.device_pwd));
    ESP_LOGI(TAG, "加载 apex_crypto_init_with_pwd 完成");

    ESP_ERROR_CHECK(apex_state_init());
    ESP_LOGI(TAG, "加载 apex_state_init 完成");

    // ============================================================
    // 统一模块加载器：在框架初始化时一次性加载所有内置指令模块
    // 每个 xxx_init() 调用内部会通过 apex_cmd_register() 注册到路由表
    // 新增模块只需在此处添加一行 ESP_ERROR_CHECK(xxx_init()) 即可
    // ============================================================
    ESP_ERROR_CHECK(apex_ota_update_init());
    ESP_LOGI(TAG, "apex_ota_update_init 完成");

    ESP_ERROR_CHECK(apex_get_state_init());
    ESP_LOGI(TAG, "apex_get_state_init 完成");

    ESP_ERROR_CHECK(apex_stop_init());
    ESP_LOGI(TAG, "apex_stop_init 完成");

    ESP_ERROR_CHECK(apex_power_down_init());
    ESP_LOGI(TAG, "apex_power_down_init 完成");

    ESP_ERROR_CHECK(apex_power_up_init());
    ESP_LOGI(TAG, "apex_power_up_init 完成");

    ESP_ERROR_CHECK(apex_reset_init());
    ESP_LOGI(TAG, "apex_reset_init 完成");

    ESP_ERROR_CHECK(apex_restart_init());
    ESP_LOGI(TAG, "apex_restart_init 完成");

    // 注册内置的 getInfo 功能
    apex_cmd_entry_t info_cmd = {
        .cmd_key = "getInfo",
        .function_name = "获取设备能力集",
        .function_desc = "返回设备当前支持的所有指令、参数结构及版本信息（无需参数）",
        .function_params = "{\"type\":\"object\",\"properties\":{}}", // 无参
        .role = "user",
        .version = "1.0.0",
        .handler = apex_cmd_getinfo_handler};
    apex_cmd_register(info_cmd);
    ESP_LOGI(TAG, "加载 apex_cmd_executor_init 完成");
    return ESP_OK;
}

/**
 * @brief 检查在待机模式下是否允许执行该指令
 */
static bool is_cmd_allowed_in_standby(const char *cmd_key)
{
    if (strcmp(cmd_key, "powerUp") == 0 ||
        strcmp(cmd_key, "getState") == 0 ||
        strcmp(cmd_key, "getInfo") == 0)
    {
        return true;
    }
    return false;
}

/**
 * @brief 尝试锁定设备状态并登记指令
 * @param flag 指令并发属性 (PARALLEL / EXCLUSIVE / FORCE / ALWAYS_ALLOWED)
 * @param cmd_key 指令标识
 * @param msg_id 本次指令的唯一 ID
 * @return APEX_OK: 登记成功; APEX_ERR_BUSY: 系统繁忙; APEX_ERR_STANDBY: 设备休眠; APEX_ERR_OVERFLOW: 槽位已满; APEX_ERR_TIMEOUT: 锁获取超时
 */
int apex_state_lock(apex_cmd_flag_t flag, const char *cmd_key, const char *msg_id)
{
    int ret = APEX_ERR_BUSY;

    // 1. 尝试获取锁
    if (xSemaphoreTake(g_apex_state.lock, pdMS_TO_TICKS(500)) != pdTRUE)
    {
        ESP_LOGE("APEX", "Lock acquisition timeout! cmd: %s", cmd_key);
        return APEX_ERR_TIMEOUT;
    }

    // --- 进入临界区 ---

    // 2. 待机状态拦截（ALWAYS_ALLOWED 指令无视待机限制）
    if (g_apex_state.is_standby && flag != APEX_CMD_FLAG_ALWAYS_ALLOWED && !is_cmd_allowed_in_standby(cmd_key))
    {
        ESP_LOGW("APEX", "系统待机中，拒绝指令: %s", cmd_key);
        ret = APEX_ERR_STANDBY;
        goto cleanup;
    }

    // 3. 独占状态检查：FORCE/ALWAYS_ALLOWED 指令无视系统忙碌
    if (g_apex_state.current_state == APEX_STATE_BUSY &&
        flag != APEX_CMD_FLAG_FORCE && flag != APEX_CMD_FLAG_ALWAYS_ALLOWED)
    {
        ESP_LOGW("APEX", "系统繁忙，正在执行独占指令: %s", g_apex_state.exclusive_cmd);
        ret = APEX_ERR_BUSY;
        goto cleanup;
    }

    if (g_apex_state.current_state == APEX_STATE_BUSY && flag == APEX_CMD_FLAG_FORCE)
    {
        ESP_LOGW("APEX", "FORCE 指令 [%s] 无视系统忙碌状态，强制执行", cmd_key);
    }
    if (g_apex_state.is_standby && flag == APEX_CMD_FLAG_ALWAYS_ALLOWED)
    {
        ESP_LOGW("APEX", "ALWAYS_ALLOWED 指令 [%s] 无视待机状态，允许执行", cmd_key);
    }

    // 4. 查找空闲槽位
    int slot = -1;
    for (int i = 0; i < APEX_MAX_PARALLEL_CMDS; i++)
    {
        if (!g_apex_state.active_cmds[i].in_use)
        {
            slot = i;
            break;
        }
    }

    if (slot == -1)
    {
        ESP_LOGE("APEX", "无可用槽位 (Parallel Limit)");
        ret = APEX_ERR_OVERFLOW;
        goto cleanup;
    }

    // 5. 登记任务信息
    g_apex_state.active_cmds[slot].in_use = true;
    strlcpy(g_apex_state.active_cmds[slot].cmd_key, cmd_key, 32);
    strlcpy(g_apex_state.active_cmds[slot].msg_id, msg_id, 64);
    g_apex_state.active_cmds[slot].start_tick = xTaskGetTickCount();

    // 6. 如果是独占模式，升级系统状态
    if (flag == APEX_CMD_FLAG_EXCLUSIVE)
    {
        g_apex_state.current_state = APEX_STATE_BUSY;
        strlcpy(g_apex_state.exclusive_cmd, cmd_key, 32);
    }

    strlcpy(g_apex_state.last_cmd, cmd_key, 32);
    ret = APEX_OK;

cleanup:
    // 7. 无论如何都会释放锁
    xSemaphoreGive(g_apex_state.lock);
    return ret;
}

/**
 * @brief 解除设备锁定状态
 */
void apex_state_unlock(const char *msg_id)
{
    // 不要死等，避免因为之前的泄露导致整个执行器任务挂起
    if (xSemaphoreTake(g_apex_state.lock, pdMS_TO_TICKS(200)) == pdTRUE)
    {
        for (int i = 0; i < APEX_MAX_PARALLEL_CMDS; i++)
        {
            if (g_apex_state.active_cmds[i].in_use &&
                strcmp(g_apex_state.active_cmds[i].msg_id, msg_id) == 0)
            {
                // 释放槽位
                g_apex_state.active_cmds[i].in_use = false;

                // 如果该指令是当前的独占指令，恢复系统为 IDLE
                if (g_apex_state.current_state == APEX_STATE_BUSY &&
                    strcmp(g_apex_state.exclusive_cmd, g_apex_state.active_cmds[i].cmd_key) == 0)
                {
                    g_apex_state.current_state = APEX_STATE_IDLE;
                    memset(g_apex_state.exclusive_cmd, 0, sizeof(g_apex_state.exclusive_cmd));
                }
                break;
            }
        }
        xSemaphoreGive(g_apex_state.lock);
    }
    else
    {
        ESP_LOGE("APEX", "Unlock failed: Lock held by task %s",
                 pcTaskGetName(xSemaphoreGetMutexHolder(g_apex_state.lock)));
    }
}

// 推荐做法：提供精准搜索 API
const apex_cmd_entry_t *apex_cmd_find_entry(const char *cmd_key)
{
    if (!cmd_key)
        return NULL;
    for (int i = 0; i < s_cmd_count; i++)
    {
        if (strcmp(s_cmd_table[i].cmd_key, cmd_key) == 0)
        {
            return &s_cmd_table[i];
        }
    }
    return NULL;
}

/**
 * @brief 根据指令 Key 查找当前正在运行的任务 ID
 * @param cmd_key 目标指令标识
 * @return 找到则返回 msg_id 字符串指针，否则返回 NULL
 * @note 返回的指针指向全局静态内存，调用者无需释放，但需注意该指针在下次解锁后可能失效
 */
const char *apex_state_get_active_msg_id(const char *cmd_key)
{
    if (cmd_key == NULL)
        return NULL;

    const char *found_msg_id = NULL;

    // 1. 加锁：防止在查找过程中，任务被其他线程（如异步回调）释放
    if (xSemaphoreTake(g_apex_state.lock, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        // 2. 遍历 8 个登记槽位
        for (int i = 0; i < APEX_MAX_PARALLEL_CMDS; i++)
        {
            if (g_apex_state.active_cmds[i].in_use &&
                strcmp(g_apex_state.active_cmds[i].cmd_key, cmd_key) == 0)
            {
                // 找到第一个匹配的正在运行的任务
                found_msg_id = g_apex_state.active_cmds[i].msg_id;
                break;
            }
        }
        xSemaphoreGive(g_apex_state.lock);
    }
    else
    {
        ESP_LOGE("STATE", "获取 MsgID 失败：无法获取状态锁");
    }

    return found_msg_id;
}

/**
 * @brief 手动终结指令并清理槽位
 * @param msg_id 指令 ID
 * @param code 状态码 (0 成功)
 * @param data 返回的业务 JSON 数据 (函数内部会接管内存并在发送后释放)
 */
esp_err_t apex_cmd_finish(const char *msg_id, int code, cJSON *data)
{
    if (msg_id == NULL)
    {
        if (data)
            cJSON_Delete(data); // 避免参数错误导致的内存泄漏
        return ESP_ERR_INVALID_ARG;
    }

    char cmd_key_copy[32] = {0};
    bool found = false;

    // 1. 【核心优化】在锁保护下提取信息
    if (xSemaphoreTake(g_apex_state.lock, portMAX_DELAY) == pdTRUE)
    {
        for (int i = 0; i < APEX_MAX_PARALLEL_CMDS; i++)
        {
            if (g_apex_state.active_cmds[i].in_use &&
                strcmp(g_apex_state.active_cmds[i].msg_id, msg_id) == 0)
            {
                // 必须拷贝出来！不要在放锁后引用 slot 指针
                strncpy(cmd_key_copy, g_apex_state.active_cmds[i].cmd_key, sizeof(cmd_key_copy) - 1);
                found = true;
                break;
            }
        }
        xSemaphoreGive(g_apex_state.lock);
    }

    // 2. 没找到任务的处理
    if (!found)
    {
        ESP_LOGW("APEX", "终结失败：未找到或任务已结束 (ID: %s)", msg_id);
        if (data)
            cJSON_Delete(data);
        return ESP_ERR_NOT_FOUND;
    }

    // 3. 执行通讯响应
    // 此时即便别的线程把槽位占了也没关系，因为我们用的是局部变量 cmd_key_copy
    apex_cmd_send_async_done(cmd_key_copy, msg_id, code, data);

    // 4. 彻底解锁逻辑
    // apex_state_unlock 内部会再次拿锁，安全地修改 current_state 和 in_use 标志
    apex_state_unlock(msg_id);

    ESP_LOGI("APEX", "任务 [%s] 已手动终结 (ID: %s)", cmd_key_copy, msg_id);
    return ESP_OK;
}

esp_err_t apex_crypto_init_with_pwd(const char *pwd)
{
    if (pwd == NULL)
        return ESP_ERR_INVALID_ARG;

    uint8_t hash[32];
    size_t hash_len = 0;

    // 使用 PSA 接口计算 SHA-256
    psa_status_t status = psa_hash_compute(
        PSA_ALG_SHA_256,
        (const uint8_t *)pwd,
        strlen(pwd),
        hash,
        sizeof(hash),
        &hash_len);

    if (status != PSA_SUCCESS)
    {
        return ESP_FAIL;
    }

    // 截取前 16 字节
    uint8_t final_psk[16];
    memcpy(final_psk, hash, 16);

    return payload_crypto_init(final_psk);
}

/**
 * @brief 将 function_param_desc_t 数组构建为 JSON Schema 格式 (Draft-07)
 *
 * 输出格式 (符合 JSON Schema 规范):
 * {
 *   "type": "object",
 *   "properties": {
 *     "add":   {"type": "integer", "description": "第一个加数（单位：celsius）", "minimum": 0, "maximum": 100, "default": 50},
 *     "mode":  {"type": "string", "enum": ["cool","heat"], "description": "风扇模式"}
 *   },
 *   "required": ["mode"]
 * }
 */
char *build_function_param_desc_json(const function_param_desc_t *params, int count,
                                     char *out_buf, int buf_size)
{
    if (!params || !out_buf || buf_size < 4)
    {
        if (out_buf && buf_size > 0)
            out_buf[0] = '\0';
        return NULL;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        out_buf[0] = '\0';
        return NULL;
    }

    // 根: "type": "object"
    cJSON_AddStringToObject(root, "type", "object");

    // 无参 Tool：直接输出 {"type":"object","properties":{}}
    if (count == 0)
    {
        cJSON *empty_props = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "properties", empty_props);

        char *json_str = cJSON_PrintUnformatted(root);
        if (!json_str)
        {
            cJSON_Delete(root);
            out_buf[0] = '\0';
            return NULL;
        }
        strncpy(out_buf, json_str, buf_size - 1);
        out_buf[buf_size - 1] = '\0';
        free(json_str);
        cJSON_Delete(root);
        // 警告：buffer 太小会截断
        if (strlen(out_buf) > (size_t)(buf_size - 2))
            ESP_LOGW(TAG, "build_function_param_desc_json: buffer too small (%d bytes)", buf_size);
        return out_buf;
    }

    cJSON *properties = cJSON_CreateObject();
    cJSON *required = cJSON_CreateArray();
    bool has_required = false;

    for (int i = 0; i < count; i++)
    {
        const function_param_desc_t *p = &params[i];

        cJSON *obj = cJSON_CreateObject();
        if (!obj)
        {
            cJSON_Delete(properties);
            cJSON_Delete(required);
            cJSON_Delete(root);
            out_buf[0] = '\0';
            return NULL;
        }

        // 1. type 映射: "int"→"integer", "float"→"number", "bool"→"boolean"
        if (strcmp(p->type, "int") == 0)
            cJSON_AddStringToObject(obj, "type", "integer");
        else if (strcmp(p->type, "float") == 0)
            cJSON_AddStringToObject(obj, "type", "number");
        else if (strcmp(p->type, "bool") == 0)
            cJSON_AddStringToObject(obj, "type", "boolean");
        else
            cJSON_AddStringToObject(obj, "type", p->type); // "string" 等原样

        // 2. description（合并 unit 信息，unit 非 JSON Schema 标准字段）
        if (p->unit && p->unit[0] != '\0')
        {
            char desc_buf[128];
            if (p->description && p->description[0] != '\0')
                snprintf(desc_buf, sizeof(desc_buf), "%s（单位：%s）", p->description, p->unit);
            else
                snprintf(desc_buf, sizeof(desc_buf), "单位：%s", p->unit);
            cJSON_AddStringToObject(obj, "description", desc_buf);
        }
        else if (p->description && p->description[0] != '\0')
            cJSON_AddStringToObject(obj, "description", p->description);

        // 3. 枚举值: "values" → "enum"
        if (p->enum_count > 0 && p->enum_vals)
        {
            cJSON *arr = cJSON_CreateArray();
            for (int j = 0; j < p->enum_count; j++)
                cJSON_AddItemToArray(arr, cJSON_CreateString(p->enum_vals[j]));
            cJSON_AddItemToObject(obj, "enum", arr);
        }

        // 4. 数值范围
        if (strcmp(p->type, "int") == 0 || strcmp(p->type, "float") == 0)
        {
            if (p->has_min)
                cJSON_AddNumberToObject(obj, "minimum", p->min_val);
            if (p->has_max)
                cJSON_AddNumberToObject(obj, "maximum", p->max_val);
            if (p->has_multipleOf)
                cJSON_AddNumberToObject(obj, "multipleOf", p->multipleOf_val);
        }

        // 5. 默认值
        if (p->has_default)
        {
            if (strcmp(p->type, "string") == 0 && p->enum_count > 0)
                cJSON_AddStringToObject(obj, "default", p->enum_vals[p->default_val]);
            else if (strcmp(p->type, "bool") == 0)
                cJSON_AddBoolToObject(obj, "default", p->default_val ? cJSON_True : cJSON_False);
            else
                cJSON_AddNumberToObject(obj, "default", p->default_val);
        }
        else
        {
            // 无默认值 = 必填参数
            cJSON_AddItemToArray(required, cJSON_CreateString(p->key));
            has_required = true;
        }

        cJSON_AddItemToObject(properties, p->key, obj);
    }

    cJSON_AddItemToObject(root, "properties", properties);
    if (has_required)
        cJSON_AddItemToObject(root, "required", required);
    else
        cJSON_Delete(required);

    // 输出到缓冲区
    char *json_str = cJSON_PrintUnformatted(root);
    if (!json_str)
    {
        cJSON_Delete(root);
        out_buf[0] = '\0';
        return NULL;
    }

    strncpy(out_buf, json_str, buf_size - 1);
    out_buf[buf_size - 1] = '\0';

    free(json_str);
    cJSON_Delete(root);
    return out_buf;
}
