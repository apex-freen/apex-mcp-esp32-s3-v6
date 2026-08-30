#define TAG "BSP_SD"
#include "bsp_sd.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#define SD_MOUNT_POINT "/sdcard"

static sdmmc_card_t *s_sd_card = NULL;

esp_err_t bsp_sd_mount(void)
{
    if (s_sd_card)
        return ESP_OK; // 幂等

    // 参考实战派 03-micro_sd / 14-handheld：SDMMC 1 线模式
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, // 挂载失败不格式化
        .max_files = 5,
        .allocation_unit_size = 8 * 1024,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1; // 1 线 SD 模式
    slot.clk = BSP_SD_CLK;
    slot.cmd = BSP_SD_CMD;
    slot.d0 = BSP_SD_D0;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot, &mount_config, &s_sd_card);
    if (ret == ESP_OK)
    {
        sdmmc_card_print_info(stdout, s_sd_card);
        ESP_LOGI(TAG, "SD 卡已挂载: %s", SD_MOUNT_POINT);
    }
    else
    {
        s_sd_card = NULL;
        ESP_LOGW(TAG, "SD 卡挂载失败: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t bsp_sd_unmount(void)
{
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, s_sd_card);
    s_sd_card = NULL;
    return ret;
}

bool bsp_sd_is_mounted(void)
{
    return s_sd_card != NULL;
}

const char *bsp_sd_mount_point(void)
{
    return SD_MOUNT_POINT;
}
