#define LOG_TAG "UTILS"
#include "utils.h"
#include <string.h>
#include "esp_log.h"
#include "esp_err.h" // 确保包含 esp_err_t 定义
#include "esp_system.h"
#include "esp_random.h" // 添加此行以声明 esp_random 函数
// #include "mbedtls/aes.h"
#include <stdio.h>
#include "mbedtls/base64.h"
// #include "mbedtls/sha256.h"
#include <string.h>
#include <stdlib.h>
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#define HEX_LEN 16
/**
 * 生成标准 UUID v4 格式的 msg_id
 * @param uuid_buf 存储 UUID 的缓冲区（需提前分配，至少 37 字节：36 位+1 位结束符）
 * @return 0 成功，-1 失败
 */
int generate_uuid_v4(char *uuid_buf)
{
    static const char *hex_chars = "0123456789abcdef";
    uint8_t raw[16];

    // 获取硬件随机数
    for (int i = 0; i < 16; i += 4)
    {
        uint32_t r = esp_random();
        memcpy(&raw[i], &r, 4);
    }

    // 设置 UUID v4 版本 (0100) 和变体 (10)
    raw[6] = (raw[6] & 0x0F) | 0x40;
    raw[8] = (raw[8] & 0x3F) | 0x80;

    char *p = uuid_buf;
    for (int i = 0; i < 16; i++)
    {
        // 修正：UUID 格式是 8-4-4-4-12，对应字节索引后的连字符
        if (i == 4 || i == 6 || i == 8 || i == 10)
        {
            *p++ = '-';
        }
        *p++ = hex_chars[raw[i] >> 4];
        *p++ = hex_chars[raw[i] & 0x0F];
    }
    *p = '\0';

    return 0; // 修复：添加返回值防止编译错误
}

// 生成随机Token
void generate_token(uint8_t *token)
{
    esp_fill_random(token, 16);
}

// 辅助函数：字节转十六进制
void bytes_to_hex(const uint8_t *src, size_t src_len, char *dst)
{
    static const char hex_chars[] = "0123456789abcdef";
    for (size_t i = 0; i < src_len; i++)
    {
        dst[i * 2] = hex_chars[src[i] >> 4];
        dst[i * 2 + 1] = hex_chars[src[i] & 0x0F];
    }
    dst[src_len * 2] = '\0';
}

// 辅助函数：十六进制转字节
size_t hex_to_bytes(const char *src, uint8_t *dst)
{
    size_t len = strlen(src);
    if (len % 2 != 0 || len > HEX_LEN * 2)
        return 0;

    for (size_t i = 0; i < len / 2; i++)
    {
        unsigned int byte;
        if (sscanf(src + i * 2, "%2x", &byte) != 1)
            return 0;
        dst[i] = (uint8_t)byte;
    }
    return len / 2;
}

void url_encode(const char *src, char *dest, size_t dest_size)
{
    static const char *hex = "0123456789ABCDEF";
    size_t i = 0;
    while (*src && i < dest_size - 4)
    {
        if (isalnum((unsigned char)*src) || *src == '-' || *src == '_' ||
            *src == '.' || *src == '~' || *src == '/' || *src == ':' ||
            *src == '?' || *src == '=' || *src == '&')
        {
            dest[i++] = *src;
        }
        else if (*src == ' ')
        {
            dest[i++] = '%';
            dest[i++] = '2';
            dest[i++] = '0';
        }
        else
        {
            dest[i++] = '%';
            dest[i++] = hex[((unsigned char)*src) >> 4];
            dest[i++] = hex[((unsigned char)*src) & 0xF];
        }
        src++;
    }
    dest[i] = '\0';
}

#define HTTP_BUFFER_SIZE (32768) // 32K 可放几百首歌
static const char *HTTP_TAG = "http_client";

/**
 * @brief  全 PSRAM 版 HTTP GET
 * @param  url         请求地址
 * @param  out_buf     输出数据（存在 PSRAM，必须 free）
 * @return 200=成功
 */
int http_client_get(const char *url, char **out_buf)
{
    *out_buf = NULL;

    static char encoded_url[512];
    url_encode(url, encoded_url, sizeof(encoded_url) - 1);

    esp_http_client_config_t config = {
        .url = encoded_url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 15000,
        .disable_auto_redirect = false,
        .buffer_size = 4096, // 内部缓冲区 4KB
        .buffer_size_tx = 1024,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(HTTP_TAG, "client init failed");
        return -1;
    }

    // ==========================
    // 🔥 关键修改：使用 open/fetch_headers/read 模式替代 perform
    // ==========================

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(HTTP_TAG, "open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return -2;
    }

    // 获取响应头（阻塞等待服务器响应）
    int content_len = esp_http_client_fetch_headers(client);
    ESP_LOGI(HTTP_TAG, "Server content_len: %d", content_len);

    int status = esp_http_client_get_status_code(client);
    if (status != 200)
    {
        ESP_LOGE(HTTP_TAG, "status error: %d", status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return status;
    }

    // 确定读取长度（优先使用 Content-Length，否则用固定缓冲区）
    int read_len = content_len;
    if (content_len <= 0 || content_len > HTTP_BUFFER_SIZE - 1)
    {
        read_len = HTTP_BUFFER_SIZE - 1; // 预留 \0 位置
        ESP_LOGW(HTTP_TAG, "Use max buffer: %d", read_len);
    }

    // ==========================
    // 🔥 从 PSRAM 申请 32KB 固定缓冲区
    // ==========================
    char *buf = (char *)heap_caps_malloc(HTTP_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf)
    {
        ESP_LOGE(HTTP_TAG, "PSRAM malloc failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return -3;
    }

    // 读取响应体（关键：这里才真正读到数据）
    int total_read = esp_http_client_read(client, buf, read_len);
    ESP_LOGI(HTTP_TAG, "Actually read: %d bytes", total_read);

    if (total_read < 0)
    {
        ESP_LOGE(HTTP_TAG, "read error: %d", total_read);
        heap_caps_free(buf);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return -4;
    }

    if (total_read == 0)
    {
        ESP_LOGW(HTTP_TAG, "No data read, content_len was: %d", content_len);
        // 尝试再读一次（某些服务器需要）
        vTaskDelay(pdMS_TO_TICKS(100));
        total_read = esp_http_client_read(client, buf, read_len);
        ESP_LOGI(HTTP_TAG, "Retry read: %d bytes", total_read);
    }

    buf[total_read] = '\0'; // 确保字符串结尾
    *out_buf = buf;

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    ESP_LOGI(HTTP_TAG, "请求成功，数据长度：%d bytes (PSRAM: 32KB)", total_read);
    return 200;
}

// ----------------------------
// PSRAM 分配 / 释放（纯 C 语法）
// ----------------------------
static void *cjson_psram_malloc(size_t sz)
{
    return heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static void cjson_psram_free(void *ptr)
{
    heap_caps_free(ptr);
}

// ----------------------------
// PSRAM 版 cJSON_Parse（纯 C）
// ----------------------------
cJSON *cJSON_Parse_PSRAM(const char *value)
{
    cJSON_Hooks hooks;
    hooks.malloc_fn = cjson_psram_malloc;
    hooks.free_fn = cjson_psram_free;

    cJSON_InitHooks(&hooks);
    return cJSON_Parse(value);
}
