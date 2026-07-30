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
 * @note 内部会自动订阅 APEX_EVENT_NET_CONNECTED 等网络事件
 */
esp_err_t apex_mqtt_init(void);

/**
 * @brief 发布消息的通用接口
 */
esp_err_t apex_mqtt_publish(const char *topic, const char *payload, int len, int qos, int retain);

/**
 * @brief 订阅主题的通用接口
 */
esp_err_t apex_mqtt_subscribe(const char *topic, int qos);

const mqtt_topics_t *mqtt_topics_get(void);
