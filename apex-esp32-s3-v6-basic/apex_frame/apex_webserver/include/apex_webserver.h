#pragma once

#include "esp_err.h"

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
