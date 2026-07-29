#include "apex_network.h"
#include "apex_event.h" // 我们的全局总线
#include "apex_config.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "lwip/ip_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "APEX_NET";

// 内部事件位
#define INTERNAL_BIT_GOT_IP BIT0
#define INTERNAL_BIT_DISCONNECTED BIT1
#define INTERNAL_BIT_CONFIG_UPDATED BIT2 // 新增：配置更新标志位

// --- 新增：定时器触发的事件 BIT ---
#define INTERNAL_BIT_TMR_AP_OFF BIT3    // 300秒不断网，关闭AP
#define INTERNAL_BIT_TMR_AP_ON BIT4     // 60秒断网，开启AP
#define INTERNAL_BIT_TMR_RECONNECT BIT5 // 10秒断网，尝试重连

#define ALL_FSM_BITS (INTERNAL_BIT_CONFIG_UPDATED | INTERNAL_BIT_GOT_IP |   \
                      INTERNAL_BIT_DISCONNECTED | INTERNAL_BIT_TMR_AP_OFF | \
                      INTERNAL_BIT_TMR_AP_ON | INTERNAL_BIT_TMR_RECONNECT)

// 定义三个定时器句柄
static TimerHandle_t s_tmr_ap_off = NULL;
static TimerHandle_t s_tmr_ap_on = NULL;
static TimerHandle_t s_tmr_reconnect = NULL;

static EventGroupHandle_t s_net_internal_eg = NULL;
static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif = NULL;

// --- 定时器回调函数（在 Timer Daemon 任务中执行，只负责发信号） ---
static void tmr_cb_ap_off(TimerHandle_t xTimer) { xEventGroupSetBits(s_net_internal_eg, INTERNAL_BIT_TMR_AP_OFF); }
static void tmr_cb_ap_on(TimerHandle_t xTimer) { xEventGroupSetBits(s_net_internal_eg, INTERNAL_BIT_TMR_AP_ON); }
static void tmr_cb_reconnect(TimerHandle_t xTimer) { xEventGroupSetBits(s_net_internal_eg, INTERNAL_BIT_TMR_RECONNECT); }

// --- 步骤 1：ESP-IDF 原生事件桥接 ---
// 这个回调极其轻量，只负责将底层中断事件转换为我们 Task 能读懂的信号
static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "启动联网");
        esp_wifi_connect();
    }
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
    {

        ESP_LOGI(TAG, "已断开连接！");
        apex_event_send(APEX_EVENT_NET_DISCONNECTED, NULL, 0);
        xEventGroupSetBits(s_net_internal_eg, INTERNAL_BIT_DISCONNECTED);
    }
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    {

        ESP_LOGI(TAG, "已联网！");
        xEventGroupSetBits(s_net_internal_eg, INTERNAL_BIT_GOT_IP);
    }
}

// 新增：监听全局的配置更新事件
static void config_update_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (id == APEX_EVENT_NETWORK_CONFIG_UPDATED)
    {
        xEventGroupSetBits(s_net_internal_eg, INTERNAL_BIT_CONFIG_UPDATED);
    }
}

// --- 步骤 2：静态 IP 配置应用 ---  // 暂时不用设备配置静态ip 所以这个函数就不用了 预留万一以后有这个需求。
// static void apply_static_ip(esp_netif_t *netif)
// {
//     if (strlen(g_apex_config.wifi_sta.ip_addr) > 0)
//     {
//         esp_netif_dhcpc_stop(netif);
//         esp_netif_ip_info_t ip_info;

//         // 修复 ESP-IDF v6.0 中的 IP 转换方法
//         ip4_addr_set_u32(&ip_info.ip, ipaddr_addr(g_apex_config.wifi_sta.ip_addr));
//         ip4_addr_set_u32(&ip_info.gw, ipaddr_addr(g_apex_config.wifi_sta.gw_addr));
//         ip4_addr_set_u32(&ip_info.netmask, ipaddr_addr(g_apex_config.wifi_sta.netmask_addr));

//         esp_netif_set_ip_info(netif, &ip_info);
//         ESP_LOGI(TAG, "已配置静态 IP: %s", g_apex_config.wifi_sta.ip_addr);
//     }
//     else
//     {
//         esp_netif_dhcpc_start(netif);
//     }
// }

// 新增：DHCP 配置应用 ，因为作为设备，应该不需要用户去配置 ip等等，除非有个别极端用户，更多的应该是希望设备自己连
esp_err_t apply_dhcp(esp_netif_t *netif)
{
    esp_err_t ret = esp_netif_dhcpc_start(netif);
    if (ret == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED)
    {
        ESP_LOGW(TAG, "DHCP已经在运行");
        return ESP_OK;
    }
    return ret;
}

// --- 私有辅助函数：从配置管理器加载并应用到底层 ---
static void reload_and_apply_wifi_config(void)
{

    // 应用配置
    // 新增：DHCP 配置应用 ，因为作为设备，应该不需要用户去配置 ip等等，除非有个别极端用户，更多的应该是希望设备自己连
    apply_dhcp(s_sta_netif);
    // apply_static_ip(s_sta_netif); // 如果有极端用户 ，那么注释掉上面的 apply_dhcp 启用这个就行了

    wifi_config_t esp_sta_cfg = {0};
    strncpy((char *)esp_sta_cfg.sta.ssid, g_apex_config.wifi_sta.ssid, sizeof(esp_sta_cfg.sta.ssid));
    strncpy((char *)esp_sta_cfg.sta.password, g_apex_config.wifi_sta.password, sizeof(esp_sta_cfg.sta.password));
    esp_wifi_set_config(WIFI_IF_STA, &esp_sta_cfg);

    // 目前 不计划 启动 AP配置 ，因为 首先 AP 不是主要服务， 过段时间会断开 ，即使有人知道密码连上， 还需要知道设备密码才能进入配置之后就是 多一个配置 ，用户学习成本会增加，而且还是一个消费级的设备。如果是工业或者要求高的，自然会有 更复杂的版本
    // wifi_config_t esp_ap_cfg = {0};
    // strncpy((char *)esp_ap_cfg.ap.ssid, g_apex_config.wifi_ap.ssid, sizeof(esp_ap_cfg.ap.ssid));
    // strncpy((char *)esp_ap_cfg.ap.password, g_apex_config.wifi_ap.password, sizeof(esp_ap_cfg.ap.password));
    // esp_ap_cfg.ap.channel = g_apex_config.wifi_ap.channel == 0 ? 1 : g_apex_config.wifi_ap.channel;
    // esp_ap_cfg.ap.max_connection = 4;
    // esp_ap_cfg.ap.authmode = strlen(g_apex_config.wifi_ap.password) == 0 ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    // esp_wifi_set_config(WIFI_IF_AP, &esp_ap_cfg);
}

// --- 步骤 3：核心状态机 Task (处理所有启停与超时逻辑) ---
static void network_fsm_task(void *pv)
{
    wifi_mode_t current_mode = WIFI_MODE_APSTA;

    while (1)
    {
        // 纯事件驱动：死等，不消耗任何不必要的 CPU 周期
        EventBits_t bits = xEventGroupWaitBits(s_net_internal_eg, ALL_FSM_BITS, pdTRUE, pdFALSE, portMAX_DELAY);

        // ==================== [外部事件处理] ====================

        if (bits & INTERNAL_BIT_CONFIG_UPDATED)
        {
            ESP_LOGI(TAG, "配置变更，热重载...");
            esp_wifi_disconnect();

            // 停止所有运行中的定时器
            xTimerStop(s_tmr_ap_off, 0);
            xTimerStop(s_tmr_ap_on, 0);
            xTimerStop(s_tmr_reconnect, 0);

            reload_and_apply_wifi_config();
            esp_wifi_connect();

            if (current_mode != WIFI_MODE_APSTA)
            {
                esp_wifi_set_mode(WIFI_MODE_APSTA);
                current_mode = WIFI_MODE_APSTA;
            }
        }

        if (bits & INTERNAL_BIT_GOT_IP)
        {
            ESP_LOGI(TAG, "已连接！开始 300 秒 AP 关闭倒计时");
            apex_event_send(APEX_EVENT_NET_CONNECTED, NULL, 0);

            // 状态切换：停止断网相关的定时器，启动在线定时器
            xTimerStop(s_tmr_reconnect, 0);
            xTimerStop(s_tmr_ap_on, 0);
            xTimerStart(s_tmr_ap_off, 0); // 启动 300s 倒计时
        }

        if (bits & INTERNAL_BIT_DISCONNECTED)
        {
            ESP_LOGW(TAG, "连接断开，进入离线处理逻辑");
            apex_event_send(APEX_EVENT_NET_DISCONNECTED, NULL, 0);

            // 状态切换：停止在线定时器，启动离线和重连定时器
            xTimerStop(s_tmr_ap_off, 0);
            xTimerStart(s_tmr_ap_on, 0);     // 启动 60s 开启 AP 倒计时
            xTimerStart(s_tmr_reconnect, 0); // 启动 10s 循环重连
        }

        // ==================== [定时器超时事件处理] ====================

        if (bits & INTERNAL_BIT_TMR_AP_OFF)
        {
            if (current_mode == WIFI_MODE_APSTA)
            {
                ESP_LOGI(TAG, "稳定运行 300 秒，自动关闭 AP");
                esp_wifi_set_mode(WIFI_MODE_STA);
                current_mode = WIFI_MODE_STA;
            }
        }

        if (bits & INTERNAL_BIT_TMR_AP_ON)
        {
            if (current_mode == WIFI_MODE_STA)
            {
                ESP_LOGW(TAG, "离线超过 60 秒，唤醒 AP 模式进行配网");
                esp_wifi_set_mode(WIFI_MODE_APSTA);
                current_mode = WIFI_MODE_APSTA;
            }
        }

        if (bits & INTERNAL_BIT_TMR_RECONNECT)
        {
            ESP_LOGI(TAG, "10秒重连 Tick 触发，尝试重连...");
            esp_wifi_connect();
        }
    }
}

// --- 步骤 4：暴露给 Atom 核心的 API ---

esp_err_t apex_network_init(void)
{
    // 注册原生系统事件
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    // 监听我们的 Atom 框架配置更新事件
    apex_event_register_handler(APEX_EVENT_NETWORK_CONFIG_UPDATED, &config_update_handler, NULL);

    ESP_ERROR_CHECK(esp_netif_init());

    s_net_internal_eg = xEventGroupCreate();
    s_sta_netif = esp_netif_create_default_wifi_sta();

    // 自定义主机名，注意最长 32 字节
    ESP_ERROR_CHECK(esp_netif_set_hostname(s_sta_netif, g_apex_config.device.device_id));

    s_ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    return ESP_OK;
}

esp_err_t apex_network_start(void)
{
    // 1. 初始化定时器 (名称, 周期ticks, 是否自动重载, ID, 回调)
    s_tmr_ap_off = xTimerCreate("tmr_ap_off", pdMS_TO_TICKS(300000), pdFALSE, NULL, tmr_cb_ap_off);
    s_tmr_ap_on = xTimerCreate("tmr_ap_on", pdMS_TO_TICKS(60000), pdFALSE, NULL, tmr_cb_ap_on);
    s_tmr_reconnect = xTimerCreate("tmr_recon", pdMS_TO_TICKS(10000), pdTRUE, NULL, tmr_cb_reconnect); // 这个是周期性的

    // 2. 主动加载配置并初始化 Wi-Fi
    reload_and_apply_wifi_config();
    esp_wifi_set_mode(WIFI_MODE_APSTA);

    xTaskCreate(network_fsm_task, "net_fsm", 4096, NULL, 5, NULL);

    // 4. 触发底层 Wi-Fi 启动 (产生 WIFI_EVENT_STA_START 从而开始连接)
    esp_wifi_start();
    return ESP_OK;
}

// ==========================================
// 以下为提供的两个工具函数，已整合并适配
// ==========================================

void apex_get_wifi_status_text(char *status_text, size_t len)
{
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_OK)
    {
        snprintf(status_text, len, "WiFi未初始化");
        return;
    }

    if (mode & WIFI_MODE_STA)
    {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
        {
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(s_sta_netif, &ip_info) == ESP_OK)
            { // 直接使用保存的 s_sta_netif 句柄更高效
                snprintf(status_text, len, "已连接WiFi: %s | STA IP: " IPSTR " | 信号强度: %d dBm",
                         ap_info.ssid, IP2STR(&ip_info.ip), ap_info.rssi);
                return;
            }
        }
        snprintf(status_text, len, "STA模式 | 未连接WiFi");
        return;
    }

    if (mode & WIFI_MODE_AP)
    {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(s_ap_netif, &ip_info) == ESP_OK)
        {
            snprintf(status_text, len, "AP模式 | AP IP: " IPSTR " | 请连接AP后配置", IP2STR(&ip_info.ip));
            return;
        }
    }

    snprintf(status_text, len, "WiFi状态未知");
}

wifi_ap_record_t *apex_wifi_mgr_scan_and_get_results(uint16_t *out_count)
{
    *out_count = 0;
    uint16_t ap_num = 0;
    wifi_scan_config_t scan_config = {.show_hidden = false};

    ESP_LOGI(TAG, "开始 Wi-Fi 扫描...");

    // 阻塞扫描（注意：如果设备正在通过 STA 传输大量数据，扫描会引发短暂的延迟或掉线）
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "扫描失败: %s", esp_err_to_name(err));
        return NULL;
    }

    esp_wifi_scan_get_ap_num(&ap_num);
    if (ap_num == 0)
        return NULL;

    if (ap_num > 15)
        ap_num = 15; // 限制最大处理数量

    wifi_ap_record_t *records = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_num);
    if (records == NULL)
    {
        ESP_LOGE(TAG, "内存分配失败");
        return NULL;
    }

    if (esp_wifi_scan_get_ap_records(&ap_num, records) == ESP_OK)
    {
        *out_count = ap_num;
        return records;
    }
    else
    {
        free(records);
        return NULL;
    }
}
