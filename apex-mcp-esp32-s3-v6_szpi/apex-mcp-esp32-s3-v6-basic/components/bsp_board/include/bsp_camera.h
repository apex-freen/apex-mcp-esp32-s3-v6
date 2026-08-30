#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief OV2640 摄像头（8bit DVP + SCCB，电源走 PCA9557）
 */
esp_err_t bsp_camera_init(void);

/**
 * @brief 抓拍一帧 JPEG
 *
 * @param out 输出帧缓冲指针（esp32-camera 内部缓冲，操作完必须 bsp_camera_return_frame）
 * @param len 输出 JPEG 长度
 */
esp_err_t bsp_camera_capture_jpeg(uint8_t **out, size_t *len);

/** 归还帧缓冲（capture_jpeg 成功后必须调用） */
void bsp_camera_return_frame(void);

/** 获取传感器 PID（OV2640_PID / GC0308_PID 等） */
int bsp_camera_get_pid(void);

/** 摄像头是否已初始化 */
bool bsp_camera_ready(void);
