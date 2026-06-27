#include "apex_mqtt.h"
#include "apex_event.h"
#include "apex_config.h" // 获取全局配置
#include "mqtt_client.h"
#include "apex_cmd_executor.h" // 依赖指令模块
#include "esp_log.h"
#include <string.h>
#include "esp_http_client.h"
#include "cJSON.h" // 如果移除了内置 cJSON，请确保组件库里有它
#include <sys/time.h>

// topic 服务
/* 静态缓冲区 */
static char s_cmd_buf[128];
static char s_rsp_buf[128];
static char s_ntc_buf[128];

/* 常量结构体（只读，全局共享） */
static const mqtt_topics_t s_topics = {
    .command = s_cmd_buf,
    .response = s_rsp_buf,
    .notice = s_ntc_buf};

static bool s_inited = false;
// topic 服务结束

static const char *TAG = "APEX_MQTT";
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_is_mqtt_connected = false;

// --- 步骤 1：处理 MQTT 原生事件 ---
void apex_http_time_sync_task(void *pvParameters);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT 已连接到 Broker!");
        s_is_mqtt_connected = true;
        // 广播 Sticky 事件，通知其他业务模块（比如传感器采数据的 Task）可以开始发数据了
        apex_event_send(APEX_EVENT_MQTT_CONNECTED, NULL, 0);

        esp_mqtt_client_subscribe(s_mqtt_client, s_topics.command, 0);
        ESP_LOGI(TAG, "已订阅主题: %s", s_topics.command); // 先订阅这一个
        // 可以在这里做默认的通配符订阅
        // esp_mqtt_client_subscribe(s_mqtt_client, "/apex/cmd/#", 1);
        ESP_LOGI(TAG, "开始请求服务端时间...");

        // 构造 API 地址，注意从 mqtt:// 转换成 http:// 或直接使用 IP
        // 假设 g_apex_config.mqtt.host 存的是 "192.168.1.100"
        char *sync_url = malloc(128);
        snprintf(sync_url, 128, "http://%s:%d/openSys/time/sync",
                 g_apex_config.mqtt.broker_addr, 80); // 端口改为 Rust 的 HTTP 端口

        xTaskCreate(apex_http_time_sync_task, "time_sync_task", 4096, sync_url, 5, NULL);
        break;

    case MQTT_EVENT_DISCONNECTED:
        if (s_is_mqtt_connected)
        {
            ESP_LOGW(TAG, "MQTT 连接断开");
            s_is_mqtt_connected = false;
            // 广播断开事件，清除 Sticky 状态
            apex_event_send(APEX_EVENT_MQTT_DISCONNECTED, NULL, 0);
        }
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "收到主题数据: %.*s", event->topic_len, event->topic);
        // 这里可以直接解析指令，或者再次通过 apex_event 把数据抛给业务层处理
        // 1. 严谨的 Topic 匹配
        bool is_command_topic = (event->topic_len == strlen(s_topics.command)) &&
                                (memcmp(event->topic, s_topics.command, event->topic_len) == 0);
        if (is_command_topic)
        {
            // 1. 提取原始密文 (Base64格式)
            // char *b64_encrypted_payload = strndup(event->data, event->data_len);
            // 2. 扔进“安全管道”：解码 -> 解密 -> 分发
            // 这里的 apex_process_incoming_cmd 是我们定义的管道入口
            apex_process_incoming_cmd(event->data, event->data_len);
            // free(b64_encrypted_payload);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT 发生错误");
        break;

    default:
        break;
    }
}

// --- 步骤 2：启动/重置 MQTT 客户端 (适配 v6.0 结构) ---
static void start_mqtt_client(void)
{

    if (strlen(g_apex_config.mqtt.broker_addr) == 0)
    {
        ESP_LOGW(TAG, "MQTT Broker URI 未配置，暂不启动");
        return;
    }

    // ★ 关键：ESP-IDF v6.0 的嵌套配置格式 ★
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            // .address.uri = g_apex_config.mqtt.broker_addr,
            .address.hostname = g_apex_config.mqtt.broker_addr,
            .address.port = g_apex_config.mqtt.port,
            .address.transport = MQTT_TRANSPORT_OVER_TCP,
        },
        .buffer = {
            .size = 4096,     // 接收缓冲区大小（默认1024）
            .out_size = 4096, // 发送缓冲区大小（可选，默认等于size）
        },
        .credentials = {
            .client_id = g_apex_config.mqtt.client_id,
            // .username = g_apex_config.mqtt.username,
            .username = g_apex_config.device.device_name, // 默认使用设备名称
            .authentication.password = g_apex_config.mqtt.password,
        },
        .session = {
            .keepalive = 60,
        },
        .network = {
            .reconnect_timeout_ms = 10000, // 断线 10 秒后自动重连
        }};

    if (s_mqtt_client == NULL)
    {
        // 初次创建
        s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
        esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
        esp_mqtt_client_start(s_mqtt_client);
    }
    else
    {
        // 热重载配置：先停，重设配置，再启
        esp_mqtt_client_stop(s_mqtt_client);
        esp_mqtt_set_config(s_mqtt_client, &mqtt_cfg);
        esp_mqtt_client_start(s_mqtt_client);
    }
}

// --- 步骤 3：桥接网络层事件 (Sticky Event 显神威) ---
static void network_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (id == APEX_EVENT_NET_CONNECTED)
    {
        ESP_LOGI(TAG, "检测到网络已连接，正在拉起 MQTT...");
        // 只有网通了，才去启动或恢复 MQTT 连接
        start_mqtt_client();
    }
    else if (id == APEX_EVENT_NET_DISCONNECTED)
    {
        ESP_LOGW(TAG, "检测到网络断开，暂停 MQTT...");
        if (s_mqtt_client != NULL)
        {
            // 网断了直接停掉底层的 TCP 尝试，省电且防止无意义的 CPU 轮询
            esp_mqtt_client_stop(s_mqtt_client);
        }
    }
    else if (id == APEX_EVENT_MQTT_CONFIG_UPDATED)
    { // 假设加了这个配置更新事件
        ESP_LOGI(TAG, "检测到 MQTT 配置更新，正在重载...");
        // 检查当前是否有网，有网才直接拉起
        // （这里由于网络模块是独立运转的，如果当前没网，重新配置后它会等下一次 NET_CONNECTED 再启动）
        start_mqtt_client();
    }
}

// --- 步骤 4：暴露对外的 API ---
bool mqtt_topics_init(void);
esp_err_t apex_mqtt_init(void)
{
    mqtt_topics_init();
    // 【核心设计】不需要主动查网络状态，直接注册监听！
    // 感谢之前的 Sticky Event 机制，如果此时网络已经通了，
    // 这个 register 函数内部会 *立刻* 触发 network_event_handler，瞬间拉起 MQTT。
    apex_event_register_handler(APEX_EVENT_NET_CONNECTED, network_event_handler, NULL);
    apex_event_register_handler(APEX_EVENT_NET_DISCONNECTED, network_event_handler, NULL);
    // 监听我们的 Atom 框架配置更新事件
    apex_event_register_handler(APEX_EVENT_MQTT_CONFIG_UPDATED, network_event_handler, NULL);

    // 如果有配置更新事件，也挂载上
    // apex_event_register_handler(APEX_EVENT_MQTT_CONFIG_UPDATED, network_event_handler, NULL);

    return ESP_OK;
}

esp_err_t apex_mqtt_publish(const char *topic, const char *payload, int len, int qos, int retain)
{
    ESP_LOGI(TAG, "apex_mqtt_publish");
    if (s_mqtt_client == NULL || !s_is_mqtt_connected)
    {
        return ESP_FAIL;
    }
    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, payload, len, qos, retain);
    return msg_id >= 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t apex_mqtt_subscribe(const char *topic, int qos)
{
    if (s_mqtt_client == NULL || !s_is_mqtt_connected)
    {
        return ESP_FAIL;
    }
    int msg_id = esp_mqtt_client_subscribe(s_mqtt_client, topic, qos);
    return msg_id >= 0 ? ESP_OK : ESP_FAIL;
}

bool mqtt_topics_init(void)
{
    if (s_inited)
    {
        ESP_LOGW(TAG, "已初始化，忽略");
        return true;
    }

    if (strlen(g_apex_config.mqtt.client_id) == 0)
    {
        ESP_LOGE(TAG, "client_id为空");
        return false;
    }

    int n;

    n = snprintf(s_cmd_buf, sizeof(s_cmd_buf),
                 "apex/%s/command", g_apex_config.mqtt.client_id);
    if (n < 0 || n >= sizeof(s_cmd_buf))
        goto fail;

    n = snprintf(s_rsp_buf, sizeof(s_rsp_buf),
                 "apex/%s/response", g_apex_config.mqtt.client_id);
    if (n < 0 || n >= sizeof(s_rsp_buf))
        goto fail;

    n = snprintf(s_ntc_buf, sizeof(s_ntc_buf),
                 "apex/%s/notify", g_apex_config.mqtt.client_id);
    if (n < 0 || n >= sizeof(s_ntc_buf))
        goto fail;

    ESP_LOGI(TAG, "TOPIC初始化:");
    ESP_LOGI(TAG, "  cmd: %s", s_cmd_buf);
    ESP_LOGI(TAG, "  rsp: %s", s_rsp_buf);
    ESP_LOGI(TAG, "  ntc: %s", s_ntc_buf);

    s_inited = true;
    return true;

fail:
    ESP_LOGE(TAG, "TOPIC格式化失败");
    return false;
}

const mqtt_topics_t *mqtt_topics_get(void)
{
    // 启动后永远有效，无需检查
    return &s_topics;
}

// 设置系统时间的辅助函数
void apex_set_system_time(uint64_t ms_timestamp)
{
    struct timeval tv;
    tv.tv_sec = (time_t)(ms_timestamp / 1000);
    tv.tv_usec = (suseconds_t)((ms_timestamp % 1000) * 1000);
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "系统时间已手动对齐: %llu", ms_timestamp);
}

// HTTP 任务：请求并解析时间
void apex_http_time_sync_task(void *pvParameters)
{
    char *url = (char *)pvParameters;
    ESP_LOGI(TAG, "HTTP 开始同步 url: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // 1. 打开连接并发送请求
    esp_err_t err = esp_http_client_open(client, 0);
    if (err == ESP_OK)
    {
        // 2. 获取响应头
        int content_length = esp_http_client_fetch_headers(client);
        int status_code = esp_http_client_get_status_code(client);

        ESP_LOGI(TAG, "HTTP 状态码: %d, 内容长度: %d", status_code, content_length);

        if (status_code == 200)
        {
            // 准备缓冲区 (27 字节的数据，申请 128 足够了)
            char *buffer = malloc(256);
            // 3. 手动读取数据
            int read_len = esp_http_client_read(client, buffer, 256);

            if (read_len > 0)
            {
                buffer[read_len] = '\0';
                ESP_LOGI(TAG, "成功获取原始 JSON: %s", buffer);

                // 解析 JSON
                cJSON *root = cJSON_Parse(buffer);
                if (root)
                {
                    cJSON *ts_item = cJSON_GetObjectItem(root, "timestamp");
                    if (cJSON_IsNumber(ts_item))
                    {
                        uint64_t ts = (uint64_t)ts_item->valuedouble;
                        apex_set_system_time(ts); // 设置系统时间
                    }
                    cJSON_Delete(root);
                }
            }
            else
            {
                ESP_LOGE(TAG, "读取数据失败或为空");
            }
            free(buffer);
        }
    }
    else
    {
        ESP_LOGE(TAG, "无法打开 HTTP 连接: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(url);
    vTaskDelete(NULL);
}
