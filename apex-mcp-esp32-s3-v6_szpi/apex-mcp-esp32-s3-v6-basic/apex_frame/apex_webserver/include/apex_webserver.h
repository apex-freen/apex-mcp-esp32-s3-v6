#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief 启动HTTP Web服务器
 *
 * @return esp_err_t ESP_OK表示成功，其他值表示失败
 */
esp_err_t web_server_start(void);

/**
 * @brief 停止HTTP Web服务器
 *
 * @return esp_err_t ESP_OK表示成功，其他值表示失败
 */
esp_err_t web_server_stop(void);

/**
 * @brief 获取 Web 服务器句柄，供外设组件扩展 URI 端点（如 /snapshot.jpg）
 *
 * @return httpd_handle_t 未启动时返回 NULL
 */
httpd_handle_t apex_webserver_get_handle(void);
