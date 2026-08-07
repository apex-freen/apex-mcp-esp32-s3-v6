#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef struct
{
    const char *command;
    const char *response;
    const char *notice;
} mqtt_topics_t;

/**
 * @brief 初始化 MQTT 模块
 */
esp_err_t apex_mqtt_init(void);

/**
 * @brief 获取 MQTT 发送缓冲区大小（用于超长报文截断判断）
 */
int apex_mqtt_get_out_size(void);

/**
 * @brief 发布消息
 */
esp_err_t apex_mqtt_publish(const char *topic, const char *payload, int len, int qos, int retain);

/**
 * @brief 订阅主题
 */
esp_err_t apex_mqtt_subscribe(const char *topic, int qos);

const mqtt_topics_t *mqtt_topics_get(void);
