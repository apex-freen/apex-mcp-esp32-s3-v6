#include "apex_webserver_private.h"
#include "ctype.h"
#include "string.h"

// URL解码
void url_decode(char *dst, const char *src, size_t max_len)
{
    if (max_len == 0)
        return;

    size_t written = 0;
    while (*src && (written < max_len - 1))
    {
        if (*src == '%')
        {
            if (isxdigit((int)src[1]) && isxdigit((int)src[2]))
            {
                int hi = toupper((int)src[1]);
                int lo = toupper((int)src[2]);
                hi = (hi >= 'A') ? (hi - 'A' + 10) : (hi - '0');
                lo = (lo >= 'A') ? (lo - 'A' + 10) : (lo - '0');

                *dst++ = (char)((hi << 4) | lo);
                src += 3;
            }
            else
            {
                *dst++ = *src++; // 格式非法则保留原样
            }
        }
        else if (*src == '+')
        {
            *dst++ = ' ';
            src++;
        }
        else
        {
            *dst++ = *src++;
        }
        written++;
    }
    *dst = '\0';
}

// 参数解析
esp_err_t param_parser_get_value(const char *content, const char *key,
                                 char *value, size_t val_max_len)
{
    if (!content || !key || !value || val_max_len == 0)
        return ESP_ERR_INVALID_ARG;

    size_t key_len = strlen(key);
    const char *p = content;

    while (*p)
    {
        const char *amp = strchr(p, '&');
        const char *end = amp ? amp : p + strlen(p);
        const char *eq = strchr(p, '=');

        if (!eq || eq >= end)
        {
            p = end + (amp ? 1 : 0);
            continue;
        }

        if ((size_t)(eq - p) == key_len && strncmp(p, key, key_len) == 0)
        {
            char tmp[256] = {0};
            size_t vlen = end - eq - 1;
            if (vlen >= sizeof(tmp))
                vlen = sizeof(tmp) - 1;
            strncpy(tmp, eq + 1, vlen);
            url_decode(value, tmp, val_max_len);
            value[val_max_len - 1] = 0;
            return ESP_OK;
        }

        p = end + (amp ? 1 : 0);
    }
    return ESP_ERR_NOT_FOUND;
}

// 兼容重定向
// esp_err_t httpd_resp_send_redirect_compat(httpd_req_t *req,
//                                           const char *uri, int code)
// {
//     httpd_resp_set_hdr(req, "Location", uri);
//     httpd_resp_set_status(req, code == 302 ? "302 Found" : "301 Moved Permanently");
//     httpd_resp_set_hdr(req, "Content-Length", "0");
//     return httpd_resp_send(req, NULL, 0);
// }
