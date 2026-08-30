#pragma once

/**
 * @brief 注册摄像头指令：
 *   - cameraCapture：抓拍 JPEG → 返回可访问 URL（可选存 SD）
 *   - cameraInfo：摄像头型号/分辨率/格式
 * 并在 Web 服务器注册 /snapshot.jpg 端点（HTTP 取图）。
 * 必须在 apex_core_init()（含 webserver 启动）之后调用。
 */
void camera_app_init(void);
