#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// 结构常量对齐 Matter 标准
#define CRYPTO_NONCE_SIZE 13
#define CRYPTO_MAC_SIZE   16
#define CRYPTO_KEY_SIZE   16 // AES-128

// 初始化加密引擎 (挂载预共享密钥)
esp_err_t payload_crypto_init(const uint8_t *psk);

// MQTT Publish 时调用：明文 -> [Nonce + 密文 + MAC]
// out_buf 需要预留大小：plaintext_len + CRYPTO_NONCE_SIZE + CRYPTO_MAC_SIZE
esp_err_t payload_crypto_encrypt(const uint8_t *plaintext, size_t plaintext_len,
                                 uint8_t *out_buf, size_t *out_len);

// MQTT Receive 时调用：[Nonce + 密文 + MAC] -> 明文
esp_err_t payload_crypto_decrypt(const uint8_t *payload, size_t payload_len,
                                 uint8_t *out_plaintext, size_t *out_len);
