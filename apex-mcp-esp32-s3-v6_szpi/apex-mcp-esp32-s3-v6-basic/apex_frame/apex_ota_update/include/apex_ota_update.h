#ifndef APEX_OTA_UPDATE_H
#define APEX_OTA_UPDATE_H

typedef struct
{
    char url[256];   // 存储 OTA 固件的下载地址
    char msg_id[64]; // 用于异步回调的任务 ID
} apex_ota_ctx_t;

esp_err_t apex_ota_update_init(void);

#endif
