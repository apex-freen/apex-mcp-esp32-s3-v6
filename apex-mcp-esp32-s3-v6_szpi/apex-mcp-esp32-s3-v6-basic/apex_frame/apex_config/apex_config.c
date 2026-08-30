#define TAG "APEX_CONFIG"
#include <string.h>
#include "esp_mac.h" // 必须包含这个头文件来获取 MAC API
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "apex_event.h"
// #include "cJSON.h"
#include "esp_flash.h"
#include "esp_chip_info.h"

/* 自己的头文件 */
#include "apex_config.h"
// 配置文件 挂载路径
#define CONFIG_PATH "/storage/config.json"

// 静态全局变量
static SemaphoreHandle_t s_config_mutex = NULL;   // 配置读写互斥锁
static const char *NVS_NAMESPACE = "apex_config"; // NVS命名空间

static char G_DEVICE_ID[32];

// 内部函数：生成基于 MAC 的唯一 ID
static void generate_unique_id(void)
{
    uint8_t mac[6];
    // 获取 ESP32 默认基准 MAC 地址 (通常是 Wi-Fi STA 模式的 MAC)
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // 格式化为字符串，例如: APEX_ABC123
    // 使用最后三个字节即可保证局域网内的唯一性，或者用全部六个字节
    // 改名字 只要改前缀 就可以了  比如 改掉 APEX 就可以了
    snprintf(G_DEVICE_ID, sizeof(G_DEVICE_ID), "APEX_%02X%02X%02X",
             mac[3], mac[4], mac[5]);

    ESP_LOGI("SYS_INIT", "设备唯一标识符生成: %s", G_DEVICE_ID);
}

// 默认值 ，重置的时候使用
static apex_config_t s_default_config = { // 默认配置
    .sw_version = "1.0.0",                // 👈 初始版本
    .wifi_sta = {
        // sta 无线名称
        .ssid = "APEX_STA",
        // sta 密码
        .password = "apex123",
        // 弃用静态IP 配置
        // // ip 地址
        // .ip_addr = "192.168.1.101",
        // // 网关 地址
        // .gw_addr = "192.168.1.1",
        // // 掩码
        // .netmask_addr = "255.255.255.0"
    },
    .wifi_ap = {
        // AP 无线名称
        .ssid = "APEX_AP",
        // AP 密码
        .password = "12345678",
        // AP 通信频道
        .channel = 6
        // .ap_enable = true
    },
    .mqtt = {// MQTT Broker 地址
             .broker_addr = "agent-plat.local",
             // MQTT Broker 端口
             .port = 1883,
             // MQTT client_id
             .client_id = "mqtt_client_id",
             // MQTT 认证用户名
             .username = "mqtt_username",
             // MQTT 认证密码
             .password = "mqtt_password"},
    .device = {
        // 设备唯一ID 和client_id 一致
        .device_id = "apex_device_id",
        // 设备名称
        .device_name = "APEX基础测试设备",
        //  设备密码
        .device_pwd = "apex123",
        // 设备描述
        .device_desc = "这是APEX基础设备，作者是：APEX团队，里面并没有实际的集成硬件，而是拥有APEX基础功能模块以及两个方法，分别是同步指令加法和异步指令加法。让开发者知道两种方法的区别和使用场景。",
    },
    // .net_host_name = "apex_host_name", // 弃用
    .factory_reset = false};

// 缓存配置，不用每次都读取
apex_config_t g_apex_config;

void apex_config_print(void)
{
    ESP_LOGI(TAG, "================ 当前系统配置 ================");
    ESP_LOGI(TAG, "系统版本号: [%s]", g_apex_config.sw_version);
    ESP_LOGI(TAG, "设备唯一ID: [%s]", g_apex_config.device.device_id);

    ESP_LOGI(TAG, "--- WiFi STA (连接路由器) ---");
    ESP_LOGI(TAG, "  SSID: [%s]", g_apex_config.wifi_sta.ssid);
    ESP_LOGI(TAG, "  PASS: [%s]", g_apex_config.wifi_sta.password);
    // 因为弃用了静态ip 所以下面的就不准确 不用了。
    // ESP_LOGI(TAG, "  IP: [%s]", g_apex_config.wifi_sta.ip_addr);
    // ESP_LOGI(TAG, "  GATEWAY: [%s]", g_apex_config.wifi_sta.gw_addr);
    // ESP_LOGI(TAG, "  NETMASK: [%s]", g_apex_config.wifi_sta.netmask_addr);

    ESP_LOGI(TAG, "--- WiFi AP (设备热点) ---");
    ESP_LOGI(TAG, "  SSID: [%s]", g_apex_config.wifi_ap.ssid);
    ESP_LOGI(TAG, "  PASS: [%s]", g_apex_config.wifi_ap.password);
    ESP_LOGI(TAG, "  CHANNEL: [%d]", g_apex_config.wifi_ap.channel);
    // ESP_LOGI(TAG, "  HOSTNAME: [%s]", g_apex_config.net_host_name); // 弃用

    ESP_LOGI(TAG, "--- MQTT 配置 ---");
    ESP_LOGI(TAG, "  Broker: [%s]:[%d]", g_apex_config.mqtt.broker_addr, g_apex_config.mqtt.port);
    ESP_LOGI(TAG, "  ClientID: [%s]", g_apex_config.mqtt.client_id);
    // ESP_LOGI(TAG, "  USERNAME: [%s]", g_apex_config.mqtt.username);
    ESP_LOGI(TAG, "  USERNAME: [%s]", g_apex_config.device.device_name);
    ESP_LOGI(TAG, "  PASSWORD: [%s]", g_apex_config.mqtt.password);

    ESP_LOGI(TAG, "--- DEVICE 配置 ---");
    ESP_LOGI(TAG, "  DEVICEID: [%s]", g_apex_config.device.device_id);
    ESP_LOGI(TAG, "  DEVICENAME: [%s]", g_apex_config.device.device_name);
    ESP_LOGI(TAG, "  DEVICEPWD: [%s]", g_apex_config.device.device_pwd);
    ESP_LOGI(TAG, "  DEVICEDESC: [%s]", g_apex_config.device.device_desc);
    ESP_LOGI(TAG, "==============================================");
}

/**
 * @brief 初始化配置互斥锁
 */
static esp_err_t apex_config_mutex_init(void)
{
    if (s_config_mutex == NULL)
    {
        s_config_mutex = xSemaphoreCreateMutex();
        if (s_config_mutex == NULL)
        {
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

/**
 * @brief 从NVS读取单个配置项
 */
static esp_err_t apex_config_read_item(nvs_handle_t handle, const char *key, void *value, size_t expected_len)
{
    if (key == NULL || value == NULL || expected_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    size_t actual_len = expected_len;
    esp_err_t err = nvs_get_blob(handle, key, value, &actual_len);

    if (err == ESP_OK && actual_len != expected_len)
    {
        ESP_LOGW(TAG, "Key '%s' size mismatch: expected %d, got %d",
                 key, expected_len, actual_len);
        return ESP_ERR_NVS_INVALID_LENGTH;
    }

    return err;
}

/**
 * @brief 向NVS写入单个配置项
 */
static esp_err_t apex_config_write_item(nvs_handle_t handle, const char *key, const void *value, size_t len)
{
    if (key == NULL || value == NULL || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }
    return nvs_set_blob(handle, key, value, len);
}

esp_err_t apex_config_read(apex_config_t *config)
{
    if (config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // 默认先填充默认配置
    memcpy(config, &s_default_config, sizeof(apex_config_t));

    // 加锁保证线程安全
    if (xSemaphoreTake(s_config_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK)
    {
        // 读取配置（不存在则保留默认值）
        apex_config_read_item(handle, "wifi_sta", &config->wifi_sta, sizeof(wifi_sta_config_s_t));
        apex_config_read_item(handle, "wifi_ap", &config->wifi_ap, sizeof(wifi_ap_config_s_t));
        apex_config_read_item(handle, "mqtt", &config->mqtt, sizeof(mqtt_config_t));
        apex_config_read_item(handle, "device", &config->device, sizeof(device_info_t));
        apex_config_read_item(handle, "factory_reset", &config->factory_reset, sizeof(bool));

        nvs_close(handle);
    }

    // 解锁
    xSemaphoreGive(s_config_mutex);
    return err;
}

esp_err_t apex_config_write(void)
{
    // 加锁
    if (xSemaphoreTake(s_config_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK)
    {
        // 写入配置（直接使用全局变量）
        apex_config_write_item(handle, "wifi_sta", &g_apex_config.wifi_sta, sizeof(wifi_sta_config_s_t));
        apex_config_write_item(handle, "wifi_ap", &g_apex_config.wifi_ap, sizeof(wifi_ap_config_s_t));
        apex_config_write_item(handle, "mqtt", &g_apex_config.mqtt, sizeof(mqtt_config_t));
        apex_config_write_item(handle, "device", &g_apex_config.device, sizeof(device_info_t));
        apex_config_write_item(handle, "factory_reset", &g_apex_config.factory_reset, sizeof(bool));

        // 提交更改
        err = nvs_commit(handle);
        nvs_close(handle);
    }

    // 打印当前配置
    apex_config_print();

    // 解锁
    xSemaphoreGive(s_config_mutex);
    return err;
}

void print_flash_info(void)
{

    // 方法B：芯片信息
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI("CHIP", "模型: %s",
             chip_info.model == CHIP_ESP32S3 ? "ESP32-S3" : "Other");
}
void get_physical_flash_size()
{
    uint32_t flash_size = 0;

    // 1. 获取 Flash 芯片的句柄 (默认芯片)
    esp_flash_t *flash_chip = esp_flash_default_chip;

    // 2. 读取 Flash ID (JEDEC ID)
    uint32_t flash_id;
    esp_err_t ret = esp_flash_read_id(flash_chip, &flash_id);

    if (ret != ESP_OK)
    {
        ESP_LOGE("PHY_FLASH", "读取 Flash ID 失败: %s", esp_err_to_name(ret));
        return;
    }

    // 3. 解析 ID 获取大小
    // JEDEC ID 格式: 1字节厂商ID + 2字节设备ID
    // 第3个字节（最低8位）通常代表容量
    uint8_t capacity_id = flash_id & 0xFF;

    // 根据容量 ID 计算大小 (2^capacity_id 字节)
    // 例如: 0x12 -> 2^18 = 256KB, 0x15 -> 2^21 = 2MB, 0x18 -> 2^24 = 16MB
    flash_size = 1 << capacity_id;

    ESP_LOGI("PHY_FLASH", "================物理 Flash 信息================");
    ESP_LOGI("PHY_FLASH", "Flash ID: 0x%06X", (unsigned int)flash_id);
    ESP_LOGI("PHY_FLASH", "厂商 ID : 0x%02X", (unsigned int)(flash_id >> 16));
    ESP_LOGI("PHY_FLASH", "物理大小: %d MB (%d bytes)",
             (unsigned int)(flash_size / (1024 * 1024)),
             (unsigned int)flash_size);
    ESP_LOGI("PHY_FLASH", "===============================================");
}

// --- 步骤 3：桥接网络层事件 (Sticky Event 显神威) ---
static void config_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS for saving: %s", esp_err_to_name(err));
        return;
    }

    switch (id)
    {
    case APEX_EVENT_NETWORK_CONFIG_UPDATED:
        ESP_LOGI(TAG, "检测到网络配置更新，保存 WiFi 配置...");

        // 只保存 WiFi STA 和 AP 配置
        apex_config_write_item(handle, "wifi_sta", &g_apex_config.wifi_sta, sizeof(wifi_sta_config_s_t));
        apex_config_write_item(handle, "wifi_ap", &g_apex_config.wifi_ap, sizeof(wifi_ap_config_s_t));
        break;

    case APEX_EVENT_BASE_CONFIG_UPDATED:
        ESP_LOGI(TAG, "检测到基础配置更新，保存设备信息...");

        // 只保存 Device 配置
        apex_config_write_item(handle, "device", &g_apex_config.device, sizeof(device_info_t));
        break;

    case APEX_EVENT_MQTT_CONFIG_UPDATED:
        ESP_LOGI(TAG, "检测到 MQTT 配置更新，保存 MQTT 配置...");

        // 只保存 MQTT 配置
        apex_config_write_item(handle, "mqtt", &g_apex_config.mqtt, sizeof(mqtt_config_t));
        break;

    default:
        ESP_LOGW(TAG, "未知配置更新事件: %ld", id);
        nvs_close(handle);
        return;
    }

    // 提交更改
    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "配置保存成功");
    }

    nvs_close(handle);
}

// 获取默认配置的函数
const apex_config_t *get_default_config(void)
{
    // 每次都用当前的 G_DEVICE_ID 更新动态字段
    if (strlen(G_DEVICE_ID) > 0)
    {
        strncpy((char *)s_default_config.wifi_ap.ssid, G_DEVICE_ID,
                sizeof(s_default_config.wifi_ap.ssid) - 1);
        s_default_config.wifi_ap.ssid[sizeof(s_default_config.wifi_ap.ssid) - 1] = '\0';

        strncpy(s_default_config.mqtt.client_id, G_DEVICE_ID,
                sizeof(s_default_config.mqtt.client_id) - 1);
        s_default_config.mqtt.client_id[sizeof(s_default_config.mqtt.client_id) - 1] = '\0';

        strncpy(s_default_config.device.device_id, G_DEVICE_ID,
                sizeof(s_default_config.device.device_id) - 1);
        s_default_config.device.device_id[sizeof(s_default_config.device.device_id) - 1] = '\0';
    }

    return &s_default_config;
}

// 新增：加载配置的核心函数
static esp_err_t load_config_from_nvs(void)
{
    // 获取默认配置 默认配置已经在 init的时候调用

    // 此时 s_default_config 已经更新为默认值
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return err;
    }

    bool has_any_config = false;

    // 读取各个配置项，失败则用默认值
    if (apex_config_read_item(handle, "wifi_sta", &g_apex_config.wifi_sta, sizeof(wifi_sta_config_s_t)) != ESP_OK)
    {
        memcpy(&g_apex_config.wifi_sta, &s_default_config.wifi_sta, sizeof(wifi_sta_config_s_t));
    }
    else
    {
        has_any_config = true;
    }

    if (apex_config_read_item(handle, "wifi_ap", &g_apex_config.wifi_ap, sizeof(wifi_ap_config_s_t)) != ESP_OK)
    {
        memcpy(&g_apex_config.wifi_ap, &s_default_config.wifi_ap, sizeof(wifi_ap_config_s_t));
    }
    else
    {
        has_any_config = true;
    }

    if (apex_config_read_item(handle, "mqtt", &g_apex_config.mqtt, sizeof(mqtt_config_t)) != ESP_OK)
    {
        memcpy(&g_apex_config.mqtt, &s_default_config.mqtt, sizeof(mqtt_config_t));
    }
    else
    {
        has_any_config = true;
    }

    if (apex_config_read_item(handle, "device", &g_apex_config.device, sizeof(device_info_t)) != ESP_OK)
    {
        memcpy(&g_apex_config.device, &s_default_config.device, sizeof(device_info_t));
    }
    else
    {
        has_any_config = true;
    }

    // factory_reset 是可选字段
    if (apex_config_read_item(handle, "factory_reset", &g_apex_config.factory_reset, sizeof(bool)) != ESP_OK)
    {
        g_apex_config.factory_reset = false;
    }
    else
    {
        has_any_config = true;
    }

    nvs_close(handle);

    // 首次启动：没有任何配置，保存默认配置到 NVS
    if (!has_any_config)
    {
        ESP_LOGI(TAG, "First boot, no config in NVS, saving default config");

        // 更新全局变量
        memcpy(&g_apex_config, &s_default_config, sizeof(apex_config_t));

        // 写入 NVS
        ESP_ERROR_CHECK(apex_config_write());
    }

    ESP_LOGI(TAG, "Config loaded from NVS successfully");
    return ESP_OK;
}

// 公开接口实现
esp_err_t apex_config_init(void)
{
    print_flash_info();
    get_physical_flash_size();
    // 2. 生成唯一设备 ID
    generate_unique_id();

    // 获取默认配置（作为备用）
    get_default_config();

    // 初始化NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "NVS flash needs erase, performing erase");
        // NVS分区需要擦除
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS flash init failed: %s", esp_err_to_name(err));
        return err;
    }

    // 4. 初始化配置互斥锁
    err = apex_config_mutex_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Config mutex init failed");
        return err;
    }
    // 4. 打印验证
    ESP_LOGI(TAG, "System config init success");

    // 重置一下配置  // 测试时候用 ， 发版 注销
    // ESP_ERROR_CHECK(apex_config_reset_factory());

    // 4. 【关键修复】加载配置（优先 NVS，无则用默认）
    ESP_ERROR_CHECK(load_config_from_nvs());

    apex_config_print();
    // 注册配置更新事件
    apex_event_register_handler(APEX_EVENT_NETWORK_CONFIG_UPDATED, config_event_handler, NULL);
    apex_event_register_handler(APEX_EVENT_BASE_CONFIG_UPDATED, config_event_handler, NULL);
    apex_event_register_handler(APEX_EVENT_MQTT_CONFIG_UPDATED, config_event_handler, NULL);
    return ESP_OK;
}

esp_err_t apex_config_reset_factory(void)
{
    // 直接写入默认配置

    // 更新全局变量
    memcpy(&g_apex_config, &s_default_config, sizeof(apex_config_t));

    // 写入 NVS
    ESP_ERROR_CHECK(apex_config_write());
    return ESP_OK;
}

SemaphoreHandle_t apex_config_get_mutex(void)
{
    return s_config_mutex;
}
