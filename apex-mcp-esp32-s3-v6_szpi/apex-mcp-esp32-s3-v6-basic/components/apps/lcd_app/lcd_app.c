#define TAG "LCD_APP"
#include "esp_log.h"
#include "cJSON.h"
#include "string.h"
#include "apex_cmd_executor.h"
#include "bsp_board.h"
#include "lcd_app.h"

#define FUNCTION_KEY "lcdShowText"
#define KEY_TEXT "text"
#define KEY_X "x"
#define KEY_Y "y"
#define KEY_COLOR "color"
#define KEY_BG "bg"

// 常用颜色名 → RGB565（AI 传字符串颜色名更友好）
static uint16_t parse_color(const char *name)
{
    if (name == NULL)
        return 0xFFFF;
    if (strcmp(name, "black") == 0)   return 0x0000;
    if (strcmp(name, "white") == 0)   return 0xFFFF;
    if (strcmp(name, "red") == 0)     return 0xF800;
    if (strcmp(name, "green") == 0)   return 0x07E0;
    if (strcmp(name, "blue") == 0)    return 0x001F;
    if (strcmp(name, "yellow") == 0)  return 0xFFE0;
    if (strcmp(name, "cyan") == 0)    return 0x07FF;
    if (strcmp(name, "magenta") == 0) return 0xF81F;
    if (strcmp(name, "gray") == 0)    return 0x8410;
    if (strcmp(name, "orange") == 0)  return 0xFD20;
    return 0xFFFF; // 默认白
}

static int lcd_show_text_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    cJSON *text_item = cJSON_GetObjectItem(params, KEY_TEXT);
    if (!cJSON_IsString(text_item) || text_item->valuestring[0] == '\0')
        return APEX_ERR_PARAM;

    int x = 0, y = 0;
    cJSON *x_item = cJSON_GetObjectItem(params, KEY_X);
    cJSON *y_item = cJSON_GetObjectItem(params, KEY_Y);
    if (cJSON_IsNumber(x_item))
        x = x_item->valueint;
    if (cJSON_IsNumber(y_item))
        y = y_item->valueint;

    uint16_t color = parse_color(NULL);
    uint16_t bg = 0x0000;
    cJSON *color_item = cJSON_GetObjectItem(params, KEY_COLOR);
    cJSON *bg_item = cJSON_GetObjectItem(params, KEY_BG);
    if (cJSON_IsString(color_item))
        color = parse_color(color_item->valuestring);
    if (cJSON_IsString(bg_item))
        bg = parse_color(bg_item->valuestring);

    bsp_lcd_draw_text(x, y, text_item->valuestring, color, bg);

    cJSON *data = cJSON_CreateObject();
    if (data == NULL)
        return APEX_ERR_SYS;
    cJSON_AddStringToObject(data, "text", text_item->valuestring);
    cJSON_AddNumberToObject(data, "x", x);
    cJSON_AddNumberToObject(data, "y", y);
    cJSON_AddStringToObject(data, "status", "displayed");
    *res_data = data;
    return APEX_OK;
}

void lcd_app_init(void)
{
    apex_cmd_entry_t entry = {
        .cmd_key = FUNCTION_KEY,
        .function_name = "LCD 显示文本",
        .function_desc = "在 LCD 上显示英文文本（5x7 点阵），可指定坐标与颜色",
        .function_params = "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\",\"description\":\"要显示的文本\"},\"x\":{\"type\":\"integer\",\"description\":\"左上角 X\"},\"y\":{\"type\":\"integer\",\"description\":\"左上角 Y\"},\"color\":{\"type\":\"string\",\"description\":\"前景色: white/black/red/green/blue/yellow/cyan/magenta\"},\"bg\":{\"type\":\"string\",\"description\":\"背景色\"}},\"required\":[\"text\"]}",
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_PARALLEL,
        .handler = lcd_show_text_handler,
        .is_persistent = false,
    };
    apex_cmd_register(entry);
    ESP_LOGI(TAG, "组件注册成功: %s", entry.cmd_key);
}
