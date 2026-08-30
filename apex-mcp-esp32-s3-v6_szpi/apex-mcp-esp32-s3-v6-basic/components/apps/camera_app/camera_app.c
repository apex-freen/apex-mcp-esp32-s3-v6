#define TAG "CAMERA_APP"
#include "esp_log.h"
#include "cJSON.h"
#include "stdio.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_camera.h"
#include "apex_cmd_executor.h"
#include "bsp_board.h"
#include "apex_webserver.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "camera_app.h"

#define SNAPSHOT_URI "/snapshot.jpg"
#define SNAPSHOT_PATH "/sdcard/snapshot.jpg"

#define STREAM_PORT 8080 // 独立 httpd 实例（避免阻塞 80 端口 webserver）
#define STREAM_URI "/stream"

static volatile bool s_streaming = false; // MJPEG 流运行标志（httpd task / worker 跨核读）
static httpd_handle_t g_stream_server = NULL; // 8080 端口独立流服务器

// ==================== HTTP 端点：/snapshot.jpg（80 端口） ====================
static esp_err_t snapshot_http_handler(httpd_req_t *req)
{
    uint8_t *jpeg = NULL;
    size_t len = 0;

    if (bsp_camera_capture_jpeg(&jpeg, &len) != ESP_OK)
    {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, "capture failed", HTTPD_RESP_USE_STRLEN);
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    esp_err_t ret = httpd_resp_send(req, (const char *)jpeg, len);
    bsp_camera_return_frame();
    return ret;
}

static const httpd_uri_t uri_snapshot = {
    .uri = SNAPSHOT_URI,
    .method = HTTP_GET,
    .handler = snapshot_http_handler,
};

// ==================== HTTP 端点：/stream（8080 端口，MJPEG 流） ====================
static esp_err_t stream_http_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*"); // 浏览器直连调试

    while (s_streaming)
    {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb == NULL)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        char part_hdr[64];
        int hlen = snprintf(part_hdr, sizeof(part_hdr),
                            "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                            (unsigned)fb->len);

        if (httpd_resp_send_chunk(req, "--frame\r\n", 9) != ESP_OK ||
            httpd_resp_send_chunk(req, part_hdr, hlen) != ESP_OK ||
            httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len) != ESP_OK ||
            httpd_resp_send_chunk(req, "\r\n", 2) != ESP_OK)
        {
            esp_camera_fb_return(fb);
            break; // 客户端断开
        }

        esp_camera_fb_return(fb);
        // 简单帧率控制：约 10fps
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    httpd_resp_send_chunk(req, NULL, 0); // 结束流
    return ESP_OK;
}

static const httpd_uri_t uri_stream = {
    .uri = STREAM_URI,
    .method = HTTP_GET,
    .handler = stream_http_handler,
};

// ==================== URL 构造 ====================
static void build_url(const char *path, int port, char *buf, size_t buf_len)
{
    esp_netif_ip_info_t ip;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr != 0)
    {
        snprintf(buf, buf_len, "http://" IPSTR ":%d%s", IP2STR(&ip.ip), port, path);
        return;
    }
    netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK)
    {
        snprintf(buf, buf_len, "http://" IPSTR ":%d%s", IP2STR(&ip.ip), port, path);
        return;
    }
    snprintf(buf, buf_len, "http://192.168.4.1:%d%s", port, path);
}

// ==================== cameraCapture：抓拍 ====================
static int camera_capture_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    if (!bsp_camera_ready())
        return APEX_ERR_SYS;
    if (s_streaming)
        return APEX_ERR_BUSY; // 流运行中，帧缓冲被占用

    uint8_t *jpeg = NULL;
    size_t len = 0;
    if (bsp_camera_capture_jpeg(&jpeg, &len) != ESP_OK)
        return APEX_ERR_SYS;

    // 可选：保存到 SD
    bool saved = false;
    cJSON *save_item = cJSON_GetObjectItem(params, "save");
    if (cJSON_IsTrue(save_item) && bsp_sd_is_mounted())
    {
        FILE *f = fopen(SNAPSHOT_PATH, "wb");
        if (f)
        {
            fwrite(jpeg, 1, len, f);
            fclose(f);
            saved = true;
        }
    }
    bsp_camera_return_frame();

    char url[96];
    build_url(SNAPSHOT_URI, 80, url, sizeof(url));

    cJSON *data = cJSON_CreateObject();
    if (data == NULL)
        return APEX_ERR_SYS;
    cJSON_AddStringToObject(data, "url", url);
    cJSON_AddNumberToObject(data, "size", len);
    cJSON_AddBoolToObject(data, "saved", saved);
    cJSON_AddStringToObject(data, "tip", "通过 HTTP GET 访问 url 获取 JPEG");
    *res_data = data;
    return APEX_OK;
}

// ==================== cameraInfo：型号信息 ====================
static int camera_info_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    if (!bsp_camera_ready())
        return APEX_ERR_SYS;

    int pid = bsp_camera_get_pid();
    cJSON *data = cJSON_CreateObject();
    if (data == NULL)
        return APEX_ERR_SYS;
    cJSON_AddNumberToObject(data, "pid", pid);
    cJSON_AddStringToObject(data, "model", (pid == GC0308_PID) ? "GC0308" : "OV2640");
    cJSON_AddStringToObject(data, "resolution", "QVGA(320x240)");
    cJSON_AddStringToObject(data, "format", "JPEG");
    cJSON_AddStringToObject(data, "snapshot_url", SNAPSHOT_URI);
    cJSON_AddStringToObject(data, "stream_url", STREAM_URI);
    *res_data = data;
    return APEX_OK;
}

// ==================== cameraStreamStart：MJPEG 流（持久化 + stop） ====================
static int stream_start_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    if (!bsp_camera_ready())
        return APEX_ERR_SYS;
    if (s_streaming)
        return APEX_ERR_BUSY;

    s_streaming = true;

    char url[96];
    build_url(STREAM_URI, STREAM_PORT, url, sizeof(url));

    cJSON *data = cJSON_CreateObject();
    if (data == NULL)
        return APEX_ERR_SYS;
    cJSON_AddStringToObject(data, "url", url);
    cJSON_AddStringToObject(data, "status", "streaming");
    cJSON_AddNumberToObject(data, "fps", 10);
    cJSON_AddStringToObject(data, "tip", "浏览器访问 url 查看实时画面，通过 stop 指令停止");
    *res_data = data;
    return APEX_OK; // is_persistent=true → 框架保持锁定，stop 终止
}

static int stream_stop_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    s_streaming = false;
    // 解锁与响应由框架 apex_stop 统一处理
    return APEX_OK;
}

// ==================== 组件注册入口 ====================
void camera_app_init(void)
{
    // 1. 80 端口：/snapshot.jpg
    httpd_handle_t server = apex_webserver_get_handle();
    if (server != NULL)
    {
        if (httpd_register_uri_handler(server, &uri_snapshot) == ESP_OK)
            ESP_LOGI(TAG, "HTTP 端点已注册: %s", SNAPSHOT_URI);
        else
            ESP_LOGW(TAG, "HTTP 端点注册失败: %s", SNAPSHOT_URI);
    }

    // 2. 8080 端口：/stream（独立 httpd，避免阻塞 80 端口 webserver）
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = STREAM_PORT;
    cfg.stack_size = 16384; // 流 handler 栈需容纳 JPEG 收发
    cfg.max_uri_handlers = 4;
    cfg.max_open_sockets = 2;
    if (httpd_start(&g_stream_server, &cfg) != ESP_OK)
    {
        ESP_LOGE(TAG, "%s 流服务器启动失败 (端口 %d)", STREAM_URI, STREAM_PORT);
    }
    else if (httpd_register_uri_handler(g_stream_server, &uri_stream) == ESP_OK)
    {
        ESP_LOGI(TAG, "MJPEG 流端点已注册: 端口 %d %s", STREAM_PORT, STREAM_URI);
    }

    // 3. 注册 MQTT 指令
    apex_cmd_entry_t capture_cmd = {
        .cmd_key = "cameraCapture",
        .function_name = "摄像头抓拍",
        .function_desc = "抓拍一帧 JPEG，返回可访问的 HTTP URL（可选保存到 SD）",
        .function_params = "{\"type\":\"object\",\"properties\":{\"save\":{\"type\":\"boolean\",\"description\":\"是否同时保存到 SD 卡\"}},\"required\":[]}",
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_PARALLEL,
        .handler = camera_capture_handler,
        .is_persistent = false,
    };
    apex_cmd_register(capture_cmd);

    apex_cmd_entry_t info_cmd = {
        .cmd_key = "cameraInfo",
        .function_name = "摄像头信息",
        .function_desc = "查询摄像头型号、分辨率与格式",
        .function_params = "{\"type\":\"object\",\"properties\":{}}",
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_ALWAYS_ALLOWED,
        .handler = camera_info_handler,
        .is_persistent = false,
    };
    apex_cmd_register(info_cmd);

    apex_cmd_entry_t stream_cmd = {
        .cmd_key = "cameraStreamStart",
        .function_name = "摄像头实时预览",
        .function_desc = "启动 MJPEG 实时视频流，返回可访问的流 URL，通过 stop 指令停止",
        .function_params = "{\"type\":\"object\",\"properties\":{}}",
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_EXCLUSIVE, // 独占：流运行期间排除其他摄像头操作
        .handler = stream_start_handler,
        .is_persistent = true,
        .stop_handler = stream_stop_handler,
    };
    apex_cmd_register(stream_cmd);

    ESP_LOGI(TAG, "组件注册成功: cameraCapture / cameraInfo / cameraStreamStart");
}
