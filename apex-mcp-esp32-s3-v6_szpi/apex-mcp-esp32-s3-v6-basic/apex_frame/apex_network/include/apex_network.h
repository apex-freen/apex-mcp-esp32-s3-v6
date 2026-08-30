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

/**
 * @brief WiFi 扫描（三级策略，智能体友好）
 *
 * - force=false 且已连接 STA：返回扫描缓存（不断网，可能陈旧）
 * - force=true 且已连接：执行"原子扫描窗口"（短暂断连 1~2s，BSSID 快速重连秒级恢复，
 *   MQTT 不闪断）；扫描期间状态机暂停，扫描后自动快速重连，失败由 10s 周期重连兜底
 * - 未连接：直接实时扫描（无连接损失）
 *
 * @param records 输出 AP 数组（调用方必须 free）
 * @param count   输出 AP 数量
 * @param force   true=强制实时扫描（已连接时接受短暂断连）
 * @return ESP_OK / ESP_ERR_NOT_FOUND(无结果) / 其他错误
 */
esp_err_t apex_wifi_scan(wifi_ap_record_t **records, uint16_t *count, bool force);
