#define TAG "SD_FILE_APP"
#include "esp_log.h"
#include "cJSON.h"
#include "apex_cmd_executor.h"
#include "bsp_board.h"
#include "sd_file_app.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>

#define SD_READ_MAX 4096 // 单文件读取上限

// ==================== sdList：列出根目录文件 ====================
static int sd_list_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    if (!bsp_sd_is_mounted())
        return APEX_ERR_SYS;

    DIR *dir = opendir(bsp_sd_mount_point());
    if (dir == NULL)
        return APEX_ERR_SYS;

    cJSON *files = cJSON_CreateArray();
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL)
    {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", ent->d_name);
        cJSON_AddNumberToObject(item, "type", ent->d_type == DT_DIR ? 1 : 0); // 1=目录 0=文件
        cJSON_AddItemToArray(files, item);
    }
    closedir(dir);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "mount_point", bsp_sd_mount_point());
    cJSON_AddItemToObject(data, "files", files);
    *res_data = data;
    return APEX_OK;
}

// ==================== sdRead：读取文件内容 ====================
static int sd_read_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    if (!bsp_sd_is_mounted())
        return APEX_ERR_SYS;

    cJSON *path_item = cJSON_GetObjectItem(params, "path");
    if (!cJSON_IsString(path_item) || path_item->valuestring[0] != '/')
        return APEX_ERR_PARAM;

    // 拼完整路径（挂载点 + 相对路径）
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s%s", bsp_sd_mount_point(), path_item->valuestring);

    FILE *f = fopen(full_path, "r");
    if (f == NULL)
        return APEX_ERR_NOT_FOUND;

    char *buf = malloc(SD_READ_MAX);
    size_t len = fread(buf, 1, SD_READ_MAX - 1, f);
    fclose(f);
    buf[len] = '\0';

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "path", path_item->valuestring);
    cJSON_AddStringToObject(data, "content", buf);
    cJSON_AddNumberToObject(data, "size", len);
    *res_data = data;
    free(buf);
    return APEX_OK;
}

// ==================== sdWrite：写入/覆盖文件 ====================
static int sd_write_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    if (!bsp_sd_is_mounted())
        return APEX_ERR_SYS;

    cJSON *path_item = cJSON_GetObjectItem(params, "path");
    cJSON *content_item = cJSON_GetObjectItem(params, "content");
    if (!cJSON_IsString(path_item) || path_item->valuestring[0] != '/' ||
        !cJSON_IsString(content_item))
        return APEX_ERR_PARAM;

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s%s", bsp_sd_mount_point(), path_item->valuestring);

    FILE *f = fopen(full_path, "w");
    if (f == NULL)
        return APEX_ERR_SYS;

    size_t written = fwrite(content_item->valuestring, 1, strlen(content_item->valuestring), f);
    fclose(f);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "path", path_item->valuestring);
    cJSON_AddNumberToObject(data, "written", written);
    *res_data = data;
    return APEX_OK;
}

// ==================== 组件注册入口 ====================
void sd_file_app_init(void)
{
    apex_cmd_entry_t list_cmd = {
        .cmd_key = "sdList",
        .function_name = "SD 文件列表",
        .function_desc = "列出 SD 卡根目录下的文件与目录",
        .function_params = "{\"type\":\"object\",\"properties\":{}}",
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_ALWAYS_ALLOWED,
        .handler = sd_list_handler,
        .is_persistent = false,
    };
    apex_cmd_register(list_cmd);

    apex_cmd_entry_t read_cmd = {
        .cmd_key = "sdRead",
        .function_name = "SD 读文件",
        .function_desc = "读取 SD 卡上指定文件的内容（最大 4KB），path 为相对根目录路径",
        .function_params = "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"相对路径，如 /config.json\"}},\"required\":[\"path\"]}",
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_PARALLEL,
        .handler = sd_read_handler,
        .is_persistent = false,
    };
    apex_cmd_register(read_cmd);

    apex_cmd_entry_t write_cmd = {
        .cmd_key = "sdWrite",
        .function_name = "SD 写文件",
        .function_desc = "将内容写入/覆盖 SD 卡上指定文件",
        .function_params = "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"相对路径，如 /config.json\"},\"content\":{\"type\":\"string\",\"description\":\"文件内容\"}},\"required\":[\"path\",\"content\"]}",
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_PARALLEL,
        .handler = sd_write_handler,
        .is_persistent = false,
    };
    apex_cmd_register(write_cmd);

    ESP_LOGI(TAG, "组件注册成功: sdList / sdRead / sdWrite");
}
