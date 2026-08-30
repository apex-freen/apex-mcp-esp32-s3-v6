#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include "esp_err.h" // 包含 esp_err_t 定义
#include "cJSON.h"

int generate_uuid_v4(char *uuid_buf);

void generate_token(uint8_t *token);

void bytes_to_hex(const uint8_t *src, size_t src_len, char *dst);

size_t hex_to_bytes(const char *src, uint8_t *dst);

int http_client_get(const char *url, char **out_buf);

cJSON *cJSON_Parse_PSRAM(const char *value);

void url_encode(const char *src, char *dest, size_t dest_size);

#endif // UTILS_MGR_H
