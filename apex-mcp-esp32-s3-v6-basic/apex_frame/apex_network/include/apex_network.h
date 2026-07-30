#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_wifi.h" // 引入 wifi_ap_record_t 等底层类型

// // STA 配置 (对应定义)
// typedef struct {
//     char ssid[32];         // 路由器 SSID
//     char password[64];     // 路由器密码
//     char ip_addr[64];      // 静态 IP (若为空则使用 DHCP)
//     char gw_addr[64];      // 网关地址
//     char netmask_addr[64]; // 子网掩码
// } wifi_sta_config_s_t;

// // AP 配置 (对应定义)
// typedef struct {
//     char ssid[32];         // AP 名称
//     char password[64];     // AP 密码
//     uint8_t channel;       // 信道(1-13)
// } wifi_ap_config_s_t;

/**
 * @brief 初始化网络模块的基础环境（LwIP, Event Loop）
 * @note  应在 main 函数早期，apex_core 初始化时调用
 */
esp_err_t apex_network_init(void);

/**
 * @brief 加载配置并启动网络状态机
 * @param sta_cfg STA 配置项
 * @param ap_cfg  AP 配置项
 * @note  调用后模块接管重连与 AP 启停的 5 分钟逻辑
 */
esp_err_t apex_network_start(void);

// --- 对外提供的工具类 API ---

/**
 * @brief 获取 WiFi 状态的文本描述（供 UI 或日志查询）
 * @param status_text 输出缓冲
 * @param len 缓冲大小
 */
void apex_get_wifi_status_text(char *status_text, size_t len);

/**
 * @brief 执行扫描并返回结果数组的指针
 * @param out_count 输出参数，用于接收找到的 AP 数量
 * @return 指向 wifi_ap_record_t 数组的指针，失败或无结果返回 NULL
 * @note 调用者必须负责 free() 返回的指针 !!
 */
wifi_ap_record_t *apex_wifi_mgr_scan_and_get_results(uint16_t *out_count);
