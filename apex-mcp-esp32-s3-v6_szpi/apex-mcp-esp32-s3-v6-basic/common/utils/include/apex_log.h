#pragma once

#include "esp_log.h"
#include <stddef.h>

/**
 * @brief 加解密链路联调日志宏（受 CONFIG_APEX_DEBUG_PAYLOAD 编译期开关控制）
 *
 * - 联调构建：CONFIG_APEX_DEBUG_PAYLOAD=y，在 DEBUG 级别打印明文/密文，
 *   便于与 apex_mcp_bridge 逐字节比对验证链路。
 * - 量产构建：CONFIG_APEX_DEBUG_PAYLOAD=n，宏展开为空语句，
 *   明文日志代码不参与编译，杜绝泄露风险。
 *
 * @param prefix 描述前缀（如 "INBOUND_PLAIN" / "OUTBOUND_PLAIN"）
 * @param data   明文/密文数据指针
 * @param len    数据长度
 */
#if CONFIG_APEX_DEBUG_PAYLOAD
#define APEX_LOG_PAYLOAD(prefix, data, len)                                    \
    ESP_LOGD("APEX_CRYPTO", "%s[%.200s] len=%d", prefix,                       \
             (const char *)(data), (int)(len))
#else
#define APEX_LOG_PAYLOAD(prefix, data, len) do {} while (0)
#endif
