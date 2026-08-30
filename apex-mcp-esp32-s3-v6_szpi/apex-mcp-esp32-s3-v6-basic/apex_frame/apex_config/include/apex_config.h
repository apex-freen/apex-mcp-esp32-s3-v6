#pragma once

#include "esp_err.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ===================== 统一配置结构体 =====================
// WiFi STA 配置  不要了。 用官方的
typedef struct
{
    char ssid[32];     // 路由器SSID
    char password[64]; // 路由器密码
    // bool sta_enable;   // 是否启用STA模式
    char ip_addr[64];      // 路由器密码
    char gw_addr[64];      // 路由器密码
    char netmask_addr[64]; // 路由器密码
} wifi_sta_config_s_t;

// WiFi AP 配置 不要了。 用官方的
typedef struct
{
    char ssid[32];     // AP名称
    char password[64]; // AP密码
    uint8_t channel;   // 信道(1-13)
    // bool ap_enable;    // 是否启用AP模式
} wifi_ap_config_s_t;

// MQTT 配置
typedef struct
{
    char broker_addr[64]; // MQTT服务器地址
    uint16_t port;        // MQTT端口
    char client_id[32];   // 客户端ID
    char username[32];    // 用户名
    char password[64];    // 密码
    // bool mqtt_enable;     // 是否启用MQTT
} mqtt_config_t;

// 设备信息配置
typedef struct
{
    char device_id[64];    // 设备唯一ID
    char device_name[32];  // 设备名称
    char device_pwd[32];   // 设备密码
    char device_desc[512]; // 设备作者
    // char device_nick_name[32];     // 设备名称昵称  暂时不要
    // char device_nick_desc[512];    // 设备描述昵称
    // uint8_t version[3];   // 固件版本 (major, minor, patch)
} device_info_t;

// 全局统一配置
typedef struct
{
    char sw_version[16]; // 👈 软件版本号，例如 "1.0.1"
    wifi_sta_config_s_t wifi_sta;

    wifi_ap_config_s_t wifi_ap;

    mqtt_config_t mqtt;
    device_info_t device;

    // char net_host_name[32]; // 网络名称 // 弃用
    bool factory_reset; // 恢复出厂设置标记
} apex_config_t;

// 声明外部变量和初始化函数
extern apex_config_t g_apex_config;

// ===================== 接口声明 =====================
/**
 * @brief 初始化系统配置模块（NVS + 互斥锁）
 * @return esp_err_t ESP_OK:成功 其他:失败
 */
esp_err_t apex_config_init(void);

/**
 * @brief 读取所有配置到内存
 * @param config 配置结构体指针
 * @return esp_err_t ESP_OK:成功 其他:失败
 */
// esp_err_t apex_config_read(apex_config_t *config);  // 不用这个了用 配置的全局变量 方便

/**
 * @brief 将内存中的配置写入NVS（线程安全）
 * @param config 配置结构体指针
 * @return esp_err_t ESP_OK:成功 其他:失败
 */
esp_err_t apex_config_write();

/**
 * @brief 恢复出厂默认配置（写入NVS）
 * @return esp_err_t ESP_OK:成功 其他:失败
 */
esp_err_t apex_config_reset_factory(void);

/**
 * @brief 获取配置互斥锁（用于手动保护配置操作）
 * @return SemaphoreHandle_t 互斥锁句柄
 */
SemaphoreHandle_t apex_config_get_mutex(void);
