#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief SD 卡挂载/卸载（SDMMC 1 线模式，挂载点为 /sdcard）
 */
esp_err_t bsp_sd_mount(void);

esp_err_t bsp_sd_unmount(void);

bool bsp_sd_is_mounted(void);

/**
 * @brief 返回挂载点路径（"/sdcard"）
 */
const char *bsp_sd_mount_point(void);
