/**
 * @file payload_crypto.c
 * @brief MQTT 负载加解密模块 (基于 ESP-IDF v6.0 PSA Crypto)
 * * @note 当前通信协议内存布局：
 * [ 13 Bytes Nonce ] + [ 8 Bytes Timestamp ] + [ Ciphertext (N Bytes) ] + [ 16 Bytes MAC ]
 * * ----------------------------------------------------------------------------------
 * 🚀 MATTER 协议演进指南 (Matter Protocol Migration Guide) 🚀
 * * 当未来需要将此模块对齐或替换为标准 Matter 协议时，请参考以下演进路径：
 * * 1. 【密钥管理演进】
 * - 当前状态：使用硬编码或 NVS 读取的静态预共享密钥 (PSK)。
 * - Matter状态：不使用静态密钥。设备通过 PASE (配网) 或 CASE (操作) 握手后，
 * 基于 ECDH 动态协商出 16 字节的 Session Key。
 * - 替换动作：废弃 payload_crypto_init 中的静态挂载。暴露一个 `update_session_key()`
 * 接口，接收 Matter 协议栈协商出的临时 Session Key 并通过
 * `psa_import_key` 注入。
 * * 2. 【防重放机制演进 (Timestamp -> Message Counter)】
 * - 当前状态：使用 8 字节 Timestamp 作为 AAD 防止重放。依赖系统 NTP 时钟同步。
 * - Matter状态：Matter 标准不完全依赖时钟，而是使用严格的 `Message Counter` (消息计数器)。
 * - 替换动作：将本文档中的 Timestamp 替换为递增的 Message Counter。并在内存/闪存中
 * 维护一个防重放窗口 (Replay Window) 以记录已接收的 Counter。
 * * 3. 【Nonce 构造规则演进】
 * - 当前状态：使用 13 字节随机数 (TRNG)。
 * - Matter状态：Matter 强制要求 Nonce 是确定性的，构造公式为：
 * Nonce = [Security Flags(1B)] + [Session ID(2B)] + [Source Node ID(8B)] + [Message Counter(4B)]
 * - 替换动作：在 encrypt 函数中，停止调用 `esp_fill_random`，改为严格按照 Matter
 * 规范拼接上述结构体作为 Nonce。
 * * 4. 【安全架构与隔离】
 * - 若底层框架设计要求将 Agent 指令与安全 Token 强隔离，在未来可以利用
 * ESP32-S3 的 DS (数字签名) 外设。将 Matter 的 DAC 私钥固化在 eFuse 中，
 * 让应用层绝对无法接触到明文密钥，所有加解密强绑定硬件通道。
 * ----------------------------------------------------------------------------------
 */

#include "apex_crypto.h"
#include "psa/crypto.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_timer.h" // 获取系统时间戳
#include <string.h>
#include <time.h>
#include <sys/time.h> // 必须包含此头文件
#include <stdint.h>

// 保持局部静态，应用层框架无需直接接触 Key Handle，保障隔离性
static psa_key_id_t s_key_id = 0;

// 关键点：时间从哪里来？
// gettimeofday 获取的是“系统时钟”，但 ESP32 上电时的初始时间通常是 1970-01-01 00:00:00。

// 如果需要“真实世界时间”：
// 必须在配网成功后，先调用 ESP-IDF 的 SNTP 服务的 API 同步网络时间。同步成功后，gettimeofday 才会返回真实的 2026 年时间戳。

// 如果只需要“防重放”：
// 即便没有同步网络时间，只要 ESP32 和 Rust 服务端之间有一个“相对时间锚点”即可。但在物联网生产环境中，同步 SNTP 是标准做法。

// 3. 与 Matter 协议的对齐建议
// 在 Matter 协议中，为了提高安全性，通常不直接信任 gettimeofday（因为它可能被 NTP 攻击篡改），而是结合以下两种方式：

// Up Time (运行时间)：使用 esp_timer_get_time() 获取微秒级的开机运行时间。这在处理局部会话（Session）的防重放时非常有效，因为它不可回拨。

// UTC Time：Matter 的 Time Cluster 会专门同步一个加密校验过的时间。

// 如果现在想快速兼容，可以这样做：
// 在 apex_crypto.c 中，解密逻辑里加入一个判断：

// 在解密函数内部
// struct timeval tv_now;
// gettimeofday(&tv_now, NULL);

// // 只有当系统检测到时间已经同步(SNTP成功)后，才执行严格的时间戳窗口校验
// // 如果时间还是 1970 年，说明还没同步，可以暂时跳过校验或记录日志
// if (tv_now.tv_sec > 1000000000L) {
//     // 执行之前的 30秒 窗口校验逻辑...
// }

/**
 * 获取当前毫秒级时间戳
 * 逻辑：
 * 1. 调用 gettimeofday 获取秒(tv_sec)和微秒(tv_usec)
 * 2. 转换为统一的毫秒(ms)格式，用于加密 AAD 或时间戳校验
 */
// uint64_t get_current_timestamp_ms(void)
// {
//     struct timeval tv_now;

//     // gettimeofday 的第一个参数是存储时间的结构体，第二个参数是时区(通常传 NULL)
//     // 返回 0 表示成功，-1 表示失败
//     if (gettimeofday(&tv_now, NULL) != 0)
//     {
//         return 0;
//     }

//     // 将秒转换为毫秒 + 将微秒转换为毫秒
//     uint64_t time_in_ms = (uint64_t)tv_now.tv_sec * 1000 + (tv_now.tv_usec / 1000);

//     return time_in_ms;
// }

/**
 * @brief 初始化加密引擎
 * @param psk 预共享密钥 (16字节)
 */
esp_err_t payload_crypto_init(const uint8_t *psk)
{
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS)
        return ESP_FAIL;

    // 清除旧密钥（如果是动态刷新）
    if (s_key_id != 0)
    {
        psa_destroy_key(s_key_id);
    }

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CCM);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 128);

    status = psa_import_key(&attributes, psk, CRYPTO_KEY_SIZE, &s_key_id);
    return (status == PSA_SUCCESS) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief 加密并打包 MQTT 负载 (加入时间戳 AAD)
 */
esp_err_t payload_crypto_encrypt(const uint8_t *plaintext, size_t plaintext_len,
                                 uint8_t *out_buf, size_t *out_len)
{
    if (!plaintext || !out_buf || !out_len)
        return ESP_ERR_INVALID_ARG;

    // 1. 生成 13 字节随机 Nonce
    uint8_t nonce[CRYPTO_NONCE_SIZE];
    esp_fill_random(nonce, sizeof(nonce));

    // 2. 获取当前系统时间戳作为 AAD (8 bytes)
    // 提示: 实际使用前请确保设备已通过 NTP 获取到真实时间，否则初始时间戳为 0
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t timestamp = (uint64_t)tv_now.tv_sec * 1000 + (tv_now.tv_usec / 1000);

    // 3. 组装头部明文：[Nonce (13B)] + [Timestamp (8B)]
    memcpy(out_buf, nonce, CRYPTO_NONCE_SIZE);
    memcpy(out_buf + CRYPTO_NONCE_SIZE, &timestamp, sizeof(uint64_t));

    // 4. 执行 AES-CCM 加密 (Timestamp 作为 AAD 输入，不加密但参与 MAC 签名)
    size_t ciphertext_len = 0;
    psa_status_t status = psa_aead_encrypt(
        s_key_id, PSA_ALG_CCM,
        nonce, CRYPTO_NONCE_SIZE,
        (const uint8_t *)&timestamp, sizeof(uint64_t), // AAD 输入
        plaintext, plaintext_len,
        out_buf + CRYPTO_NONCE_SIZE + sizeof(uint64_t), // 密文+MAC写入位置
        plaintext_len + CRYPTO_MAC_SIZE,
        &ciphertext_len);

    if (status != PSA_SUCCESS)
        return ESP_FAIL;

    // 总长度 = Nonce + Timestamp + (密文 + MAC)
    *out_len = CRYPTO_NONCE_SIZE + sizeof(uint64_t) + ciphertext_len;
    return ESP_OK;
}

// 隐式同步（最推荐，无额外开销）
// 既然指令包里已经含有 received_timestamp，且这个时间戳是被 AES-CCM MAC 保护过的（无法被伪造），可以直接信任它。

// 逻辑： 如果设备发现当前年份小于 2020 年（说明没对时），则在第一次成功解密消息后，直接用消息里的时间戳更新系统时钟。

/**
 * @brief 解密并验证 MQTT 负载 (防重放校验)
 */
esp_err_t payload_crypto_decrypt(const uint8_t *payload, size_t payload_len,
                                 uint8_t *out_plaintext, size_t *out_len)
{
    // 基本长度校验
    const size_t header_size = CRYPTO_NONCE_SIZE + sizeof(uint64_t);
    if (payload_len <= header_size + CRYPTO_MAC_SIZE)
        return ESP_ERR_INVALID_ARG;

    const uint8_t *nonce = payload;
    const uint8_t *timestamp_ptr = payload + CRYPTO_NONCE_SIZE;
    const uint8_t *ciphertext = payload + header_size;
    size_t ciphertext_len = payload_len - header_size; // 包含 MAC 的长度

    uint64_t received_timestamp = 0;
    memcpy(&received_timestamp, timestamp_ptr, sizeof(uint64_t));

    // 【防重放拦截】检查时间戳是否在合理窗口内 (例如：相差不超过 30 秒)
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t current_timestamp = (uint64_t)tv_now.tv_sec * 1000 + (tv_now.tv_usec / 1000);

    ESP_LOGI("DEBUG", "本地时间: %llu, 收到时间: %llu", current_timestamp, received_timestamp);

    // 如果系统时间未同步，此处逻辑可能误判，建议增加系统时间有效性标志判断
    if (current_timestamp > received_timestamp + 30000 ||
        received_timestamp > current_timestamp + 30000)
    {
        // 时间戳异常，可能是重放攻击或时钟漂移
        return ESP_ERR_TIMEOUT;
    }

    // 执行 AES-CCM 解密，底层会自动验证 MAC 是否匹配，以及 AAD(时间戳) 是否被篡改
    psa_status_t status = psa_aead_decrypt(
        s_key_id, PSA_ALG_CCM,
        nonce, CRYPTO_NONCE_SIZE,
        (const uint8_t *)&received_timestamp, sizeof(uint64_t), // 传入提取的 AAD 供校验
        ciphertext, ciphertext_len,
        out_plaintext, payload_len,
        out_len);
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE("CORE", "PSA 解密失败原因码: %d", status);
        // 如果返回 -144 (PSA_ERROR_INVALID_SIGNATURE)，说明是 KEY 错了或者数据被篡改（MAC校验失败）
        // 如果在前面 return ESP_ERR_TIMEOUT，说明是时间戳对不上
    }

    return (status == PSA_SUCCESS) ? ESP_OK : ESP_ERR_INVALID_MAC;
}
