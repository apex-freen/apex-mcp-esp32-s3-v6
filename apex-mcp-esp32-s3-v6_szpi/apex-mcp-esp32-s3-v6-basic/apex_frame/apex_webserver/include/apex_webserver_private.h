#pragma once

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"

// 仅内部使用的宏
// #define SESSION_COOKIE_NAME "TestEqp_sess"
// #define SESSION_EXPIRE_SEC  1800
// #define MAX_LOGIN_RETRY     3

// 仅内部使用的结构体
// typedef struct {
//     bool     is_login;
//     uint32_t login_time;
//     int      retry_count;
// } login_session_t;

// 仅内部使用的全局变量声明（用extern）
// extern login_session_t g_login_session;
// extern portMUX_TYPE session_mux;
// extern httpd_handle_t g_server;

// 仅内部使用的函数声明
// bool is_session_valid(void);
// void set_login_status(bool status);
// bool verify_dev_pwd(const char *input_pwd);
void url_decode(char *dst, const char *src, size_t max_len);
esp_err_t param_parser_get_value(const char *content, const char *key,
                                 char *value, size_t val_max_len);
// esp_err_t httpd_resp_send_redirect_compat(httpd_req_t *req,
//                                          const char *uri, int code);
