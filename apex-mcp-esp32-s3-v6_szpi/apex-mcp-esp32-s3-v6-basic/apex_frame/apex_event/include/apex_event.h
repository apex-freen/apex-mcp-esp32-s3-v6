#pragma once

#include "esp_err.h"
#include "esp_event.h"

// ===================== 全局事件定义 =====================
// 事件基类（自定义）
ESP_EVENT_DECLARE_BASE(APEX_EVENTS);

// 具体事件ID
typedef enum
{
    // APEX_EVENT_WIFI_STA_CONNECTED,    // STA已连接路由器并获取IP
    // APEX_EVENT_WIFI_STA_DISCONNECTED, // STA断开连接 需要关闭 MQTT服务
    // // APEX_EVENT_WIFI_STA_CONNECT_FAIL, // STA连接失败  不需要这个事件， 失败就 慢速重连
    // APEX_EVENT_WIFI_AP_STARTED, // AP已开启
    // APEX_EVENT_WIFI_AP_STOPPED, // AP已停止
    // // APEX_EVENT_WIFI_SCAN_DONE,  // WIFI扫描完成  // 不需要这个

    // APEX_EVENT_MQTT_CONNECTED,      // MQTT已连接
    // APEX_EVENT_MQTT_DISCONNECTED,   // MQTT断开连接
    // APEX_EVENT_HTTP_SERVER_STARTED, // HTTP服务已开启 // 不需要标记关闭，因为 web服务要常开
    APEX_EVENT_NETWORK_CONFIG_UPDATED, // 系统网络配置已更新 需要重启STA 或者读取新配置并进行连接。
    APEX_EVENT_BASE_CONFIG_UPDATED,    // 系统基本配置已更新 需要重启 MQTT服务
    APEX_EVENT_MQTT_CONFIG_UPDATED,    // MQTT配置已更新 需要重启 MQTT服务

    APEX_EVENT_NET_CONNECTED,       // 状态性：已连网
    APEX_EVENT_NET_DISCONNECTED,    // 状态性：断开网络
    APEX_EVENT_MQTT_CONNECTED,      // 状态性：MQTT 已连接
    APEX_EVENT_MQTT_DISCONNECTED,   // 状态性：MQTT 已断开
    APEX_EVENT_HTTP_SERVER_STARTED, // 状态性：Web 服务已启动
                                    // ... 其他事件
    APEX_EVENT_MAX
} apex_event_id_t;

// ===================== 接口声明 =====================
/**
 * @brief 初始化全局事件循环
 * @return esp_err_t ESP_OK:成功 其他:失败
 */
esp_err_t apex_event_init(void);

/**
 * @brief 发送全局系统事件
 * @param event_id 事件ID
 * @param data 事件附带数据（可为NULL）
 * @param data_len 数据长度
 * @return esp_err_t ESP_OK:成功 其他:失败
 */

esp_err_t apex_event_send(apex_event_id_t event_id, void *data, size_t data_len);

/**
 * @brief 注册全局事件处理函数
 * @param event_id 事件ID（APEX_EVENT_XXX）
 * @param handler 事件处理函数
 * @param handler_arg 处理函数参数
 * @return esp_err_t ESP_OK:成功 其他:失败
 */
esp_err_t apex_event_register_handler(apex_event_id_t event_id, esp_event_handler_t handler, void *handler_arg);
