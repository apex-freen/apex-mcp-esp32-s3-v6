#ifndef APEX_NOTIFY_H
#define APEX_NOTIFY_H

/**
 * @brief 初始化并注册通知示例组件
 *
 * 演示设备主动事件通知能力：
 *   1. 设备触发事件（如超温报警、状态变更）
 *   2. 调用 apex_cmd_send_notify() 推送 JSON-RPC 2.0 通知
 *   3. 通知通过 MQTT notice topic 发送到服务端
 *
 * 输出格式（JSON-RPC 2.0 Notification）：
 * {
 *   "jsonrpc": "2.0",
 *   "method": "device.notify",
 *   "params": {
 *     "function_key": "notifyDemo",
 *     "event": "alarm",
 *     "data": {"type": "temp_high", "value": 85.5}
 *   }
 * }
 */
esp_err_t apex_notify_init(void);

#endif // APEX_NOTIFY_H
