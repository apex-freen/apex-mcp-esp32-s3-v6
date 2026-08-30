#define TAG "WEB_SERVER"
#include <time.h>
#include "apex_webserver.h"
#include "apex_webserver_private.h"
#include "apex_config.h"
#include "apex_event.h"
#include "apex_network.h"
#include "utils.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "string.h"
#include "ctype.h"
#include "freertos/portmacro.h"

// ============================================================================
// 配置
// ============================================================================
#define MAX_LOGIN_RETRY 3
#define MAX_SESSIONS 3        // 最多同时3个客户端
#define SESSION_TIMEOUT_S 300 // 5分钟无活动过期

// ============================================================================
// 全局状态
// ============================================================================

typedef struct
{
    bool active;
    uint8_t token[16];    // 随机会话ID
    uint32_t login_time;  // 登录时间戳
    uint32_t last_active; // 最后活动时间
    int retry_count;      // 该IP/会话的失败次数
    char client_ip[32];   // 记录IP（可选）
} session_entry_t;

static session_entry_t g_sessions[MAX_SESSIONS];
static portMUX_TYPE session_mux = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE config_mux = portMUX_INITIALIZER_UNLOCKED;

// ============================================================================
// 工具函数
// ============================================================================

// 创建新会话（登录成功时调用）
esp_err_t session_create(uint8_t *out_token)
{
    ESP_LOGI(TAG, "session_create called");
    portENTER_CRITICAL(&session_mux);

    // 先清理过期会话
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        if (g_sessions[i].active &&
            time(NULL) - g_sessions[i].last_active > SESSION_TIMEOUT_S)
        {
            g_sessions[i].active = false;
        }
    }

    // 找空闲槽位
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        if (!g_sessions[i].active)
        {
            g_sessions[i].active = true;
            g_sessions[i].login_time = time(NULL);
            g_sessions[i].last_active = time(NULL);
            g_sessions[i].retry_count = 0;

            generate_token(g_sessions[i].token);
            memcpy(out_token, g_sessions[i].token, 16);

            portEXIT_CRITICAL(&session_mux);
            return ESP_OK;
        }
    }

    portEXIT_CRITICAL(&session_mux);
    return ESP_ERR_NO_MEM; // 会话满
}

// 验证Token（每个请求都检查）
bool token_validate(const uint8_t *token)
{
    portENTER_CRITICAL(&session_mux);

    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        if (g_sessions[i].active &&
            memcmp(g_sessions[i].token, token, 16) == 0)
        {

            // 检查超时
            if (time(NULL) - g_sessions[i].last_active > SESSION_TIMEOUT_S)
            {
                g_sessions[i].active = false; // 过期清理
                portEXIT_CRITICAL(&session_mux);
                return false;
            }

            g_sessions[i].last_active = time(NULL); // 更新活动时间
            portEXIT_CRITICAL(&session_mux);
            return true;
        }
    }

    portEXIT_CRITICAL(&session_mux);
    return false;
}

// 验证中间件（每个受保护页面调用）
static bool check_auth(httpd_req_t *req)
{
    char cookie[512];
    if (httpd_req_get_hdr_value_str(req, "Cookie", cookie, sizeof(cookie)) != ESP_OK)
    {
        return false;
    }
    ESP_LOGI(TAG, "Cookie: %s", cookie);

    // 解析 session=xxx
    char *session_val = strstr(cookie, "session=");
    if (!session_val)
        return false;

    session_val += 8; // 跳过 "session="
    uint8_t token[16];
    hex_to_bytes(session_val, token);

    return token_validate(token); // 验证Token有效性
}

// 兼容性的 HTTP 重定向响应封装，比直接 httpd_resp_send(req, NULL, 0) 多了必要的 HTTP 头设置。
static esp_err_t httpd_resp_send_redirect_compat(httpd_req_t *req,
                                                 const char *uri, int code)
{
    // 1. 设置 Location 头：告诉浏览器跳转到哪里（核心）
    httpd_resp_set_hdr(req, "Location", uri);
    // 2. 设置状态码：302 临时重定向 / 301 永久重定向  200 500 400
    httpd_resp_set_status(req, code == 302 ? "302 Found" : "301 Moved Permanently");
    // 3. 设置 Content-Length: 0（表示无响应体）
    httpd_resp_set_hdr(req, "Content-Length", "0");
    return httpd_resp_send(req, NULL, 0);
}

// 优化响应的响应方法（修复Content-Length参数错误）
static esp_err_t httpd_resp_send_compat(httpd_req_t *req, const char *content, size_t len, const char *cookie)
{
    // 修复核心：先将len格式化为字符串，再设置Content-Length
    char content_length[32] = {0};
    snprintf(content_length, sizeof(content_length), "%zu", len);
    httpd_resp_set_hdr(req, "Content-Length", content_length);

    // 发送响应内容
    return httpd_resp_send(req, content, len);
}

static bool dev_password_validate(const char *input_pwd)
{
    if (!input_pwd || strlen(input_pwd) == 0)
        return false;

    // 已经有 读锁 所以这里不用了
    // portENTER_CRITICAL(&config_mux);
    bool match = (strcmp(input_pwd, g_apex_config.device.device_pwd) == 0);
    // portEXIT_CRITICAL(&config_mux);

    return match;
}

// ============================================================================
// 登录页面
// ============================================================================
static const char *login_html =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<title>登录</title>"
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0;font-family:Arial}"
    "body{max-width:400px;margin:50px auto;padding:20px;background:#f5f5f5}"
    ".login-box{background:white;padding:30px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}"
    "h2{color:#333;margin-bottom:20px;text-align:center}"
    ".form-group{margin-bottom:15px}"
    "label{display:block;margin-bottom:5px;color:#555;font-weight:600}"
    "input{width:100%;padding:10px;border:1px solid #ddd;border-radius:4px}"
    "button{width:100%;padding:12px;background:#4CAF50;color:white;border:none;border-radius:4px;margin-top:10px}"
    "button:disabled{background:#ccc}"
    ".error-msg{background:#fff3f3;color:#d32f2f;padding:10px 14px;border:1px solid #ffcdd2;border-radius:4px;margin-bottom:16px;font-size:14px;display:none;text-align:center}"
    "</style>"
    "</head>"
    "<body>"
    "<div class='login-box'>"
    "<h2>设备配置登录</h2>"
    "<div id='errorMsg' class='error-msg'></div>"
    "<form action='/do-login' method='POST'>"
    "<div class='form-group'>"
    "<label  for='dev_pwd'>设备密码</label>"
    "<input type='password' name='dev_pwd' id='dev_pwd' required>"
    "</div>"
    "<button type='submit' >登录</button>"
    "</form>"
    "</div>"
    "<script>"
    "var p=new URLSearchParams(location.search);"
    "var err=p.get('error');"
    "if(err){"
    "var e=document.getElementById('errorMsg');"
    "var m={'wrong_pwd':'密码错误，请重试','auth_fail':'认证失败，请重新登录','session_expired':'登录已过期，请重新登录'};"
    "e.textContent=m[err]||err;"
    "e.style.display='block';"
    "}"
    "</script>"
    "</body>"
    "</html>";

esp_err_t favicon_get_handler(httpd_req_t *req)
{
    // 返回 204 No Content（比 404 更优雅）
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_set_hdr(req, "Content-Length", "0");
    return httpd_resp_send(req, NULL, 0);
}
static esp_err_t login_page_handler(httpd_req_t *req)
{
    if (check_auth(req))
    {
        return httpd_resp_send_redirect_compat(req, "/", 303);
    }
    httpd_resp_set_type(req, "text/html");
    esp_err_t ret = httpd_resp_send(req, login_html, HTTPD_RESP_USE_STRLEN);
    // free(response);
    return ret;
}

static esp_err_t do_login_handler(httpd_req_t *req)
{
    // 接收表单数据
    char content[256] = {0};
    ssize_t recv_len = httpd_req_recv(req, content, sizeof(content) - 1);
    if (recv_len <= 0)
    {
        ESP_LOGE(TAG, "接收登录密码失败: %zd", recv_len);
        return httpd_resp_send_redirect_compat(req, "/login?error=wrong_pwd", 303);
    }

    // 解析密码
    char input_pwd[64] = {0};
    if (param_parser_get_value(content, "dev_pwd", input_pwd, sizeof(input_pwd)) != ESP_OK)
    {
        ESP_LOGE(TAG, "解析登录密码失败");
        return httpd_resp_send_redirect_compat(req, "/login?error=wrong_pwd", 302);
    }

    // 验证密码
    if (dev_password_validate(input_pwd))
    {
        // 密码验证成功
        ESP_LOGI(TAG, "密码验证成功");
        uint8_t token[16];
        session_create(token); // 生成token

        // Token 转为 hex 字符串存入 Cookie
        char token_hex[33];
        bytes_to_hex(token, 16, token_hex);

        // 设置 Cookie: session=xxxx
        char cookie_header[64];
        snprintf(cookie_header, sizeof(cookie_header),
                 "session=%s; Path=/; HttpOnly", token_hex);
        ESP_LOGI(TAG, "密码验证成功，设置 Cookie: %s", cookie_header);
        httpd_resp_set_hdr(req, "Set-Cookie", cookie_header);

        return httpd_resp_send_redirect_compat(req, "/", 303);
    }
    else
    {
        // 密码错误，增加重试次数  // 后期可以增加 ，如果是某账户登录次数 超过， 那么锁定账户 ，这个是否要在这里
        // 花时间优化， 如果增加了账户锁定， 那么还要增加账户解锁，反而导致了目标，目标是针对硬件的，不是系统的
        // 所以 以后如果失败次数过多，可以增加 一个 警告， 向主控发送警告。已经连接的设备，如果多次被登录，那么向
        // 主控 发送警告，提醒有侵入。
        // portENTER_CRITICAL(&session_mux);
        // g_login_session.retry_count++;
        // portEXIT_CRITICAL(&session_mux);
        // ESP_LOGE(TAG, "设备登录失败，密码错误，剩余重试次数：%d", MAX_LOGIN_RETRY - g_login_session.retry_count);
        return httpd_resp_send_redirect_compat(req, "/login?error=wrong_pwd", 303);
    }
}

static esp_err_t logout_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "用户登出");

    // 清除 Cookie（关键：让浏览器删除 session）
    httpd_resp_set_hdr(req, "Set-Cookie",
                       "session=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT; HttpOnly");

    return httpd_resp_send_redirect_compat(req, "/login", 302);
}

// ====================== 优化：配置页面核心逻辑 ======================
// 简化的配置页面HTML模板（单页面+双表单，优化用户体验）
static const char *config_html =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<title>ESP32配置中心</title>"
    "<style>"
    "* { box-sizing: border-box; margin: 0; padding: 0; font-family: Arial, sans-serif; }"
    ".header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }"
    ".logout-btn { padding: 8px 15px; background-color: #dc3545; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; }"
    ".container { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); margin-bottom: 20px; }"
    "h2 { color: #333; margin-bottom: 15px; text-align: center; }"
    "h3 { color: #4CAF50; margin: 15px 0; padding-bottom: 5px; border-bottom: 1px solid #ddd; }"
    ".form-group { margin-bottom: 12px; }"
    "label { display: block; margin-bottom: 4px; color: #555; font-weight: 600; }"
    "input, select { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px; }"
    "input[type='password'] { font-family: 'Courier New', monospace; }"
    "button { width: 100%; padding: 10px; background-color: #4CAF50; color: white; border: none; border-radius: 4px; font-size: 16px; cursor: pointer; margin-top: 10px; }"
    "button:disabled { background-color: #cccccc; cursor: not-allowed; }"
    ".scan-btn { background-color: #007bff; width: 48%; margin-right: 4%; }"
    ".save-btn { width: 48%; }"
    ".status { margin-top: 10px; padding: 10px; border-radius: 4px; text-align: center; font-weight: 600; }"
    ".success { background-color: #dff0d8; color: #3c763d; }"
    ".error { background-color: #f2dede; color: #a94442; }"
    ".wifi-status { margin-bottom: 15px; padding: 8px; background-color: #d9edf7; color: #31708f; border-radius: 4px; text-align: center; font-size: 14px; }"
    ".scan-group { display: flex; gap: 10px; margin-bottom: 10px; }"
    ".loading { display: inline-block; width: 16px; height: 16px; border: 2px solid #fff; border-radius: 50%; border-top-color: transparent; animation: spin 1s linear infinite; }"
    "@keyframes spin { to { transform: rotate(360deg); } }"
    "</style>"
    "</head>"
    "<body>"
    "<div class='header'>"
    "    <h2>设备配置中心</h2>"
    "</div>"
    "<div class='wifi-status'>%s</div>"
    "<div><button class='logout-btn' onclick='window.location.href=\"/logout\"'>退出登录</button></div>"

    "<!-- 基础配置表单 -->"
    "<div class='container'>"
    "<h3>基础配置</h3>"
    "<form action='/save-basic' method='POST' onsubmit='this.querySelector(\"button\").disabled=true; this.querySelector(\"button\").innerHTML=\"保存中<span class=loading></span>\";'>"
    // "    <div class='form-group'>"
    // "        <label>设备名称:</label>"
    // "        <input type='text' name='dev_name' value='%s' required placeholder='如:ESP32_Control' maxlength='32'>"
    // "    </div>"
    "    <div class='form-group'>"
    "        <label>设备密码:</label>"
    "        <input type='text' name='dev_pwd' value='%s' required placeholder='至少6位' minlength='6' maxlength='64'>"
    "    </div>"
    "    <button type='submit'>保存基础配置</button>"
    "    %s"
    "</form>"
    "</div>"

    "<!-- MQTT配置表单 -->"
    "<div class='container'>"
    "<h3>MQTT配置</h3>"
    "<form action='/save-mqtt' method='POST' onsubmit='this.querySelector(\"button\").disabled=true; this.querySelector(\"button\").innerHTML=\"保存中<span class=loading></span>\";'>"
    "    <div class='form-group'>"
    "        <label>主控IP地址:</label>"
    "        <input type='text' name='mst_ip' value='%s' required placeholder='如:192.168.1.100'>"
    "    </div>"
    // "    <div class='form-group'>"
    // "        <label>主控密码:</label>"
    // "        <input type='text' name='mst_pwd' value='%s' required placeholder='主控认证密码' maxlength='64'>"
    // "    </div>"

    "    <button type='submit'>保存MQTT配置</button>"
    "    %s"
    "</form>"
    "</div>"

    "<!-- 网络配置表单 -->"
    "<div class='container'>"
    "<h3>网络配置</h3>"
    "<form action='/save-wifi' method='POST' onsubmit='this.querySelector(\"button\").disabled=true; this.querySelector(\"button\").innerHTML=\"保存中<span class=loading></span>\";'>"
    "    <div class='form-group'>"
    "        <label>WiFi名称:</label>"
    "        <div class='scan-group'>"
    "            <select name='wifi_ssid' required style='flex:1;'>"
    "                <option value=''>-- 选择WiFi --</option>"
    "%s"
    "            </select>"
    "            <!-- 给按钮加唯一ID，移除onclick -->"
    "            <button type='button' class='scan-btn' id='scanWiFiBtn'>扫描WiFi</button>"
    "        </div>"
    "    </div>"
    "    <div class='form-group'>"
    "        <label>WiFi密码:</label>"
    "        <input type='text' name='wifi_pwd' value='%s' placeholder='至少8位' minlength='8' maxlength='64'>"
    "    </div>"
    // "    <div class='form-group'>"
    // "        <label>IP:</label>"
    // "        <input type='text' name='ip_addr' value='%s' placeholder='IP地址' minlength='8' maxlength='64'>"
    // "    </div>"
    // "    <div class='form-group'>"
    // "        <label>网关地址:</label>"
    // "        <input type='text' name='gw_addr' value='%s' placeholder='IP地址' minlength='8' maxlength='64'>"
    // "    </div>"
    // "    <div class='form-group'>"
    // "        <label>子网掩码:</label>"
    // "        <input type='text' name='netmask_addr' value='%s' placeholder='IP地址' minlength='8' maxlength='64'>"
    // "    </div>"
    "    <button type='submit'>保存网络配置</button>"
    "    %s"
    "</form>"
    "</div>"

    "<!-- JS代码单独成块，确保换行和空格正确 -->"
    "<script type='text/javascript'>"
    // 页面加载完成后初始化"
    "document.addEventListener('DOMContentLoaded', function() {"
    // 获取扫描WiFi按钮"
    "    var scanBtn = document.getElementById('scanWiFiBtn');"
    "    if (scanBtn) {"
    // 恢复按钮初始状态"
    "        scanBtn.disabled = false;"
    "        scanBtn.innerHTML = '扫描WiFi';"
    // 绑定点击事件"
    "        scanBtn.addEventListener('click', function() {"
    "            this.disabled = true;"
    "            this.innerHTML = '扫描中<span class=\"loading\"></span>';"
    // 跳转到扫描接口"
    "            window.location.href = '/?scan=1';"
    "        });"
    "    }"
    "});"
    "</script>"
    "</body>"
    "</html>";

// 生成WiFi下拉选项HTML（优化：去重+信号强度排序）
static void generate_wifi_options(char *buf, size_t buf_len)
{
    buf[0] = '\0';
    size_t offset = 0; // 用于记录当前写入缓冲区的偏移量
    // 直接使用全局变量 wifi_ap_list 和 ap_count
    uint16_t ap_count = 0;
    // 直接在生成页面逻辑里调用扫描
    wifi_ap_record_t *wifi_ap_list = apex_wifi_mgr_scan_and_get_results(&ap_count);

    // 简单排序（按信号强度从强到弱）
    int sorted_indices[32] = {0};
    int max_count = ap_count > 32 ? 32 : ap_count; // 防止越界

    for (int i = 0; i < max_count; i++)
    {
        sorted_indices[i] = i;
    }

    for (int i = 0; i < max_count - 1; i++)
    {
        for (int j = i + 1; j < max_count; j++)
        {
            if (wifi_ap_list[sorted_indices[j]].rssi > wifi_ap_list[sorted_indices[i]].rssi)
            {
                int tmp = sorted_indices[i];
                sorted_indices[i] = sorted_indices[j];
                sorted_indices[j] = tmp;
            }
        }
    }

    // 生成选项
    for (int i = 0; i < max_count; i++)
    {
        int idx = sorted_indices[i];
        const char *current_ssid = (const char *)wifi_ap_list[idx].ssid;

        // 跳过空SSID
        if (strlen(current_ssid) == 0)
            continue;

        // 【优化2】：直接在当前循环中往回找，判断是否重复，省去 1KB 的 ssid_list 栈内存
        bool exists = false;
        for (int j = 0; j < i; j++)
        {
            int prev_idx = sorted_indices[j];
            if (strcmp(current_ssid, (const char *)wifi_ap_list[prev_idx].ssid) == 0)
            {
                exists = true;
                break;
            }
        }
        if (exists)
            continue;

        // 信号强度显示
        int rssi = wifi_ap_list[idx].rssi;
        const char *signal_level;
        if (rssi >= -50)
            signal_level = "强";
        else if (rssi >= -70)
            signal_level = "中";
        else
            signal_level = "弱";

        // 【优化3】：利用 snprintf 的返回值，直接在 buf 上追加，O(1) 复杂度
        int written = snprintf(buf + offset, buf_len - offset,
                               "<option value='%.32s' %s>%.32s (%d dBm - %s)</option>",
                               current_ssid,
                               strcmp(current_ssid, g_apex_config.wifi_sta.ssid) == 0 ? "selected" : "",
                               current_ssid,
                               rssi,
                               signal_level);

        // 如果写入成功且没有截断，则更新偏移量；否则说明缓冲区满了，立刻退出
        if (written > 0 && written < buf_len - offset)
        {
            offset += written;
        }
        else
        {
            break; // 缓冲区已满，停止添加
        }
    }

    // 【关键修复】：循环全部结束后，统放内存
    free(wifi_ap_list);
    ESP_LOGI(TAG, "WiFi 选项生成完毕，内存已释放");
}

static esp_err_t config_page_handler(httpd_req_t *req)
{
    // 未登录则重定向到登录页面
    if (!check_auth(req))
    {
        return httpd_resp_send_redirect_compat(req, "/login", 302);
    }

    // 获取WiFi状态
    char wifi_states[64];
    apex_get_wifi_status_text(wifi_states, sizeof(wifi_states));

    // 仅当用户点击"扫描WiFi"按钮时才执行扫描
    char wifi_options[1024] = {0};
    if (strstr(req->uri, "scan=1"))
    {
        generate_wifi_options(wifi_options, sizeof(wifi_options));
    }

    // 解析URL参数（状态提示）
    char basic_status[128] = "";
    char mqtt_status[128] = "";
    char network_status[128] = "";

    // 页面占位符 basic_status network_status 报错提示
    if (strstr(req->uri, "basic=success"))
    {
        snprintf(basic_status, sizeof(basic_status),
                 "<div class='status success'>基础配置保存成功！</div>");
    }
    else if (strstr(req->uri, "basic=error"))
    {
        snprintf(basic_status, sizeof(basic_status),
                 "<div class='status error'>基础配置保存失败！</div>");
    }

    if (strstr(req->uri, "mqtt=success"))
    {
        snprintf(mqtt_status, sizeof(mqtt_status),
                 "<div class='status success'>MQTT配置保存成功！</div>");
    }
    else if (strstr(req->uri, "basic=error"))
    {
        snprintf(mqtt_status, sizeof(mqtt_status),
                 "<div class='status error'>MQTT配置保存失败！</div>");
    }

    if (strstr(req->uri, "network=success"))
    {
        snprintf(network_status, sizeof(network_status),
                 "<div class='status success'>网络配置保存成功！</div>");
    }
    else if (strstr(req->uri, "network=error"))
    {
        snprintf(network_status, sizeof(network_status),
                 "<div class='status error'>网络配置保存失败！</div>");
    }
    // 动态分配响应缓冲区（长度校验）
    size_t resp_len = strlen(config_html) +
                      strlen(wifi_states) +
                      //   strlen(g_apex_config.device.device_name) +
                      strlen(g_apex_config.device.device_pwd) +
                      strlen(mqtt_status) +
                      strlen(g_apex_config.mqtt.broker_addr) +
                      //   strlen(g_apex_config.mqtt.password) +
                      strlen(wifi_options) +
                      strlen(g_apex_config.wifi_sta.password) +
                      // 新增：DHCP 配置应用 ，因为作为设备，应该不需要用户去配置 ip等等，除非有个别极端用户，更多的应该是希望设备自己连
                      // strlen(g_apex_config.wifi_sta.ip_addr) +
                      // strlen(g_apex_config.wifi_sta.gw_addr) +
                      // strlen(g_apex_config.wifi_sta.netmask_addr) +
                      strlen(basic_status) +
                      strlen(network_status) + 100;
    char *response = (char *)malloc(resp_len);
    if (!response)
    {
        ESP_LOGE(TAG, "分配配置页面缓冲区失败");
        return ESP_ERR_NO_MEM;
    }

    // 填充HTML模板（临界区保护配置访问）

    snprintf(response, resp_len, config_html,
             wifi_states,
             //  g_apex_config.device.device_name,
             g_apex_config.device.device_pwd,
             basic_status,
             g_apex_config.mqtt.broker_addr,
             //  g_apex_config.mqtt.password,
             mqtt_status,
             wifi_options,
             g_apex_config.wifi_sta.password,
             // 新增：DHCP 配置应用 ，因为作为设备，应该不需要用户去配置 ip等等，除非有个别极端用户，更多的应该是希望设备自己连
             // g_apex_config.wifi_sta.ip_addr,
             // g_apex_config.wifi_sta.gw_addr,
             // g_apex_config.wifi_sta.netmask_addr,
             network_status);

    httpd_resp_set_type(req, "text/html");
    esp_err_t ret = httpd_resp_send_compat(req, response, strlen(response), NULL);
    free(response);
    return ret;
}

// ============================================================================
// 保存WiFi（核心：和 wifi_mgr 联动）
// ============================================================================
static esp_err_t save_wifi_handler(httpd_req_t *req)
{
    if (!check_auth(req))
    {
        return httpd_resp_send_redirect_compat(req, "/login", 302);
    }

    char content[512] = {0};
    ssize_t rlen = httpd_req_recv(req, content, sizeof(content) - 1);
    if (rlen <= 0)
    {
        return httpd_resp_send_redirect_compat(req, "/?network=error", 302);
    }

    char wifi_ssid[64] = {0};
    char wifi_pwd[64] = {0};
    // 新增：DHCP 配置应用 ，因为作为设备，应该不需要用户去配置 ip等等，除非有个别极端用户，更多的应该是希望设备自己连
    // char ip_addr[64] = {0};
    // char gw_addr[64] = {0};
    // char netmask_addr[64] = {0};
    param_parser_get_value(content, "wifi_ssid", wifi_ssid, sizeof(wifi_ssid));
    param_parser_get_value(content, "wifi_pwd", wifi_pwd, sizeof(wifi_pwd));
    // 新增：DHCP 配置应用 ，因为作为设备，应该不需要用户去配置 ip等等，除非有个别极端用户，更多的应该是希望设备自己连
    // param_parser_get_value(content, "ip_addr", ip_addr, sizeof(ip_addr));
    // param_parser_get_value(content, "gw_addr", gw_addr, sizeof(gw_addr));
    // param_parser_get_value(content, "netmask_addr", netmask_addr, sizeof(netmask_addr));

    portENTER_CRITICAL(&config_mux);
    strncpy(g_apex_config.wifi_sta.ssid, wifi_ssid, sizeof(g_apex_config.wifi_sta.ssid) - 1);
    strncpy(g_apex_config.wifi_sta.password, wifi_pwd, sizeof(g_apex_config.wifi_sta.password) - 1);
    // 新增：DHCP 配置应用 ，因为作为设备，应该不需要用户去配置 ip等等，除非有个别极端用户，更多的应该是希望设备自己连
    // strncpy(g_apex_config.wifi_sta.ip_addr, ip_addr, sizeof(g_apex_config.wifi_sta.ip_addr) - 1);
    // strncpy(g_apex_config.wifi_sta.gw_addr, gw_addr, sizeof(g_apex_config.wifi_sta.gw_addr) - 1);
    // strncpy(g_apex_config.wifi_sta.netmask_addr, netmask_addr, sizeof(g_apex_config.wifi_sta.netmask_addr) - 1);
    portEXIT_CRITICAL(&config_mux);

    // if (apex_config_write(&g_apex_config) != ESP_OK)
    // {
    //     return httpd_resp_send_redirect_compat(req, "/?network=error", 302);
    // }

    // ===================== 关键：事件触发 WiFi 重新加载 去main 主控面板后续动作=====================
    ESP_LOGE(TAG, "通知网络配置更新");
    apex_event_send(APEX_EVENT_NETWORK_CONFIG_UPDATED, NULL, 0);

    return httpd_resp_send_redirect_compat(req, "/?network=success", 302);
}

// 保存基础配置处理（优化：参数校验+临界区保护）
static esp_err_t save_basic_handler(httpd_req_t *req)
{
    // 登录验证
    if (!check_auth(req))
    {
        return httpd_resp_send_redirect_compat(req, "/login", 302);
    }

    // 1. 接收表单数据（超时处理优化）
    char content[2048] = {0};
    ssize_t recv_len = httpd_req_recv(req, content, sizeof(content) - 1);
    if (recv_len <= 0)
    {
        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
        {
            ESP_LOGE(TAG, "接收基础配置表单超时");
        }
        else
        {
            ESP_LOGE(TAG, "接收基础配置表单失败: %zd", recv_len);
        }
        return httpd_resp_send_redirect_compat(req, "/?basic=error", 302);
    }
    ESP_LOGI(TAG, "接收基础配置数据: %s", content);

    // 2. 解析并校验参数
    // char dev_name[33] = {0};
    char dev_pwd[65] = {0};

    // 解析设备名称
    // if (param_parser_get_value(content, "dev_name", dev_name, sizeof(dev_name) - 1) != ESP_OK || strlen(dev_name) == 0)
    // {
    //     ESP_LOGE(TAG, "设备名称解析失败或为空");
    //     return httpd_resp_send_redirect_compat(req, "/?basic=error", 302);
    // }

    // 解析并校验设备密码（至少6位）
    if (param_parser_get_value(content, "dev_pwd", dev_pwd, sizeof(dev_pwd) - 1) != ESP_OK || strlen(dev_pwd) < 6)
    {
        ESP_LOGE(TAG, "设备密码解析失败或长度不足6位");
        return httpd_resp_send_redirect_compat(req, "/?basic=error", 302);
    }

    // 3. 写入全局配置（临界区保护）
    portENTER_CRITICAL(&config_mux);
    // strncpy(g_apex_config.device.device_name, dev_name, sizeof(g_apex_config.device.device_name) - 1);
    strncpy(g_apex_config.device.device_pwd, dev_pwd, sizeof(g_apex_config.device.device_pwd) - 1);
    portEXIT_CRITICAL(&config_mux);

    // if (apex_config_write(&g_apex_config) != ESP_OK)
    // {
    //     return httpd_resp_send_redirect_compat(req, "/?basic=error", 302);
    // }

    // ===================== 关键：触发 WiFi 重新加载 =====================
    ESP_LOGE(TAG, "通知基础配置更新");
    apex_event_send(APEX_EVENT_BASE_CONFIG_UPDATED, NULL, 0);

    return ESP_OK;
}

// 保存基础配置处理（优化：参数校验+临界区保护）
static esp_err_t save_mqtt_handler(httpd_req_t *req)
{
    // 登录验证
    if (!check_auth(req))
    {
        return httpd_resp_send_redirect_compat(req, "/login", 302);
    }

    // 1. 接收表单数据（超时处理优化）
    char content[2048] = {0};
    ssize_t recv_len = httpd_req_recv(req, content, sizeof(content) - 1);
    if (recv_len <= 0)
    {
        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
        {
            ESP_LOGE(TAG, "接收MQTT配置表单超时");
        }
        else
        {
            ESP_LOGE(TAG, "接收MQTT配置表单失败: %zd", recv_len);
        }
        return httpd_resp_send_redirect_compat(req, "/?basic=error", 302);
    }
    ESP_LOGI(TAG, "接收MQTT配置数据: %s", content);

    // 2. 解析并校验参数
    char mst_ip[64] = {0};
    // char mst_pwd[65] = {0};

    // 解析并校验主控IP
    if (param_parser_get_value(content, "mst_ip", mst_ip, sizeof(mst_ip) - 1) != ESP_OK)
    {
        ESP_LOGE(TAG, "主控IP格式错误: %s", mst_ip);
        return httpd_resp_send_redirect_compat(req, "/?basic=error", 302);
    }

    // 解析主控密码
    // if (param_parser_get_value(content, "mst_pwd", mst_pwd, sizeof(mst_pwd) - 1) != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "主控密码解析失败");
    //     return httpd_resp_send_redirect_compat(req, "/?basic=error", 302);
    // }

    // 3. 写入全局配置（临界区保护）
    portENTER_CRITICAL(&config_mux);
    strncpy(g_apex_config.mqtt.broker_addr, mst_ip, sizeof(g_apex_config.mqtt.broker_addr) - 1);
    // strncpy(g_apex_config.mqtt.password, mst_pwd, sizeof(g_apex_config.mqtt.password) - 1);
    portEXIT_CRITICAL(&config_mux);

    // if (apex_config_write(&g_apex_config) != ESP_OK)
    // {
    //     return httpd_resp_send_redirect_compat(req, "/?basic=error", 302);
    // }

    // ===================== 关键：触发 WiFi 重新加载 =====================
    ESP_LOGE(TAG, "通知MQTT配置更新");
    apex_event_send(APEX_EVENT_MQTT_CONFIG_UPDATED, NULL, 0);

    return ESP_OK;
}

// ============================================================================
// 404
// ============================================================================
static esp_err_t not_found_handler(httpd_req_t *req)
{
    const char *html = "<h1>404</h1><a href='/login'>登录</a>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_status(req, "404 Not Found");
    return httpd_resp_send(req, html, strlen(html));
}

// ============================================================================
// 路由
// ============================================================================
static const httpd_uri_t favicon_uri = {
    .uri = "/favicon.ico",
    .method = HTTP_GET,
    .handler = favicon_get_handler,
    .user_ctx = NULL};
static const httpd_uri_t uri_login = {.uri = "/login", .method = HTTP_GET, .handler = login_page_handler};
static const httpd_uri_t uri_do_login = {.uri = "/do-login", .method = HTTP_POST, .handler = do_login_handler};
static const httpd_uri_t uri_logout = {.uri = "/logout", .method = HTTP_GET, .handler = logout_handler};
static const httpd_uri_t uri_config = {.uri = "/", .method = HTTP_GET, .handler = config_page_handler};
static const httpd_uri_t uri_save_wifi = {.uri = "/save-wifi", .method = HTTP_POST, .handler = save_wifi_handler};
static const httpd_uri_t uri_404 = {.uri = "*", .method = HTTP_GET, .handler = not_found_handler};
static const httpd_uri_t uri_save_basic = {.uri = "/save-basic", .method = HTTP_POST, .handler = save_basic_handler};
static const httpd_uri_t uri_save_mqtt = {.uri = "/save-mqtt", .method = HTTP_POST, .handler = save_mqtt_handler};

// ============================================================================
// 启动 / 停止
// ============================================================================
static httpd_handle_t g_server = NULL;

esp_err_t web_server_start(void)
{
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    c.stack_size = 8192;
    c.server_port = 80;
    c.max_open_sockets = 4;
    c.max_uri_handlers = 15;

    if (httpd_start(&g_server, &c) != ESP_OK)
    {
        ESP_LOGE(TAG, "web start failed");
        return ESP_FAIL;
    }
    httpd_register_uri_handler(g_server, &favicon_uri);
    httpd_register_uri_handler(g_server, &uri_login);
    httpd_register_uri_handler(g_server, &uri_do_login);
    httpd_register_uri_handler(g_server, &uri_logout);
    httpd_register_uri_handler(g_server, &uri_config);
    httpd_register_uri_handler(g_server, &uri_save_wifi);
    httpd_register_uri_handler(g_server, &uri_save_basic);
    httpd_register_uri_handler(g_server, &uri_save_mqtt);
    httpd_register_uri_handler(g_server, &uri_404);
    apex_event_send(APEX_EVENT_HTTP_SERVER_STARTED, NULL, 0);

    ESP_LOGI(TAG, "web server running at :80");
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    if (g_server)
    {
        httpd_stop(g_server);
        ESP_LOGI(TAG, "web stopped");
    }
    return ESP_OK;
}
