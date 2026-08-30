#define TAG "LCD_APP"
#include "esp_log.h"
#include "cJSON.h"
#include "string.h"
#include "esp_heap_caps.h"
#include "apex_cmd_executor.h"
#include "bsp_board.h"
#include "lcd_app.h"

#define KEY_TEXT "text"
#define KEY_X "x"
#define KEY_Y "y"
#define KEY_W "w"
#define KEY_H "h"
#define KEY_COLOR "color"
#define KEY_BG "bg"
#define KEY_PERCENT "percent"
#define KEY_PATTERN "pattern"
#define KEY_COLOR2 "color2"

// 常用颜色名 → RGB565
static uint16_t parse_color(const char *name)
{
    if (name == NULL)
        return 0xFFFF;
    if (strcmp(name, "black") == 0)
        return 0x0000;
    if (strcmp(name, "white") == 0)
        return 0xFFFF;
    if (strcmp(name, "red") == 0)
        return 0xF800;
    if (strcmp(name, "green") == 0)
        return 0x07E0;
    if (strcmp(name, "blue") == 0)
        return 0x001F;
    if (strcmp(name, "yellow") == 0)
        return 0xFFE0;
    if (strcmp(name, "cyan") == 0)
        return 0x07FF;
    if (strcmp(name, "magenta") == 0)
        return 0xF81F;
    if (strcmp(name, "gray") == 0)
        return 0x8410;
    if (strcmp(name, "orange") == 0)
        return 0xFD20;
    return 0xFFFF; // 默认白
}

// RGB565 分量混合（t/max 0~1）
static uint16_t blend565(uint16_t c1, uint16_t c2, int t, int max)
{
    int r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
    int r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
    int r = r1 + ((r2 - r1) * t) / max;
    int g = g1 + ((g2 - g1) * t) / max;
    int b = b1 + ((b2 - b1) * t) / max;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

// ==================== lcdShowText：显示文本 ====================
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

    uint16_t color = 0xFFFF, bg = 0x0000;
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
    cJSON_AddStringToObject(data, "status", "displayed");
    *res_data = data;
    return APEX_OK;
}

// ==================== lcdShowColor：整屏/区域填充 ====================
static int lcd_show_color_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    uint16_t color = 0xFFFF;
    cJSON *color_item = cJSON_GetObjectItem(params, KEY_COLOR);
    if (cJSON_IsString(color_item))
        color = parse_color(color_item->valuestring);
    else if (cJSON_IsNumber(color_item))
        color = (uint16_t)(color_item->valueint & 0xFFFF);
    else
        return APEX_ERR_PARAM;

    int x = 0, y = 0, w = BSP_LCD_H_RES, h = BSP_LCD_V_RES;
    cJSON *x_item = cJSON_GetObjectItem(params, KEY_X);
    cJSON *y_item = cJSON_GetObjectItem(params, KEY_Y);
    cJSON *w_item = cJSON_GetObjectItem(params, KEY_W);
    cJSON *h_item = cJSON_GetObjectItem(params, KEY_H);
    if (cJSON_IsNumber(x_item))
        x = x_item->valueint;
    if (cJSON_IsNumber(y_item))
        y = y_item->valueint;
    if (cJSON_IsNumber(w_item))
        w = w_item->valueint;
    if (cJSON_IsNumber(h_item))
        h = h_item->valueint;

    if (w <= 0 || h <= 0)
        return APEX_ERR_PARAM;

    if (x == 0 && y == 0 && w >= BSP_LCD_H_RES && h >= BSP_LCD_V_RES)
        bsp_lcd_fill(color);
    else
        bsp_lcd_fill_rect(x, y, x + w, y + h, color);

    cJSON *data = cJSON_CreateObject();
    if (data == NULL)
        return APEX_ERR_SYS;
    cJSON_AddStringToObject(data, "status", "displayed");
    *res_data = data;
    return APEX_OK;
}

// ==================== lcdShowImage：生成图案（checker/gradient/bars） ====================
static int lcd_show_image_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    cJSON *pattern_item = cJSON_GetObjectItem(params, KEY_PATTERN);
    if (!cJSON_IsString(pattern_item))
        return APEX_ERR_PARAM;
    const char *pattern = pattern_item->valuestring;

    uint16_t c1 = 0xFFFF, c2 = 0x0000;
    cJSON *color1_item = cJSON_GetObjectItem(params, KEY_COLOR);
    cJSON *color2_item = cJSON_GetObjectItem(params, KEY_COLOR2);
    if (cJSON_IsString(color1_item))
        c1 = parse_color(color1_item->valuestring);
    if (cJSON_IsString(color2_item))
        c2 = parse_color(color2_item->valuestring);

    const int W = BSP_LCD_H_RES, H = BSP_LCD_V_RES;
    uint16_t *buf = (uint16_t *)heap_caps_malloc(W * H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf == NULL)
        return APEX_ERR_SYS;

    if (strcmp(pattern, "checker") == 0)
    {
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                buf[y * W + x] = ((x / 16 + y / 16) % 2) ? c1 : c2;
    }
    else if (strcmp(pattern, "gradient") == 0)
    {
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                buf[y * W + x] = blend565(c1, c2, x, W - 1);
    }
    else if (strcmp(pattern, "bars") == 0)
    {
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                buf[y * W + x] = ((x / 40) % 2) ? c1 : c2;
    }
    else
    {
        heap_caps_free(buf);
        return APEX_ERR_PARAM;
    }

    bsp_lcd_draw_bitmap(0, 0, W, H, buf);
    heap_caps_free(buf);

    cJSON *data = cJSON_CreateObject();
    if (data == NULL)
        return APEX_ERR_SYS;
    cJSON_AddStringToObject(data, "pattern", pattern);
    cJSON_AddStringToObject(data, "status", "displayed");
    *res_data = data;
    return APEX_OK;
}

// ==================== lcdBacklight：背光亮度 ====================
static int lcd_backlight_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    cJSON *percent_item = cJSON_GetObjectItem(params, KEY_PERCENT);
    if (!cJSON_IsNumber(percent_item))
        return APEX_ERR_PARAM;
    int percent = percent_item->valueint;
    if (percent < 0 || percent > 100)
        return APEX_ERR_PARAM;

    bsp_lcd_set_backlight(percent);

    cJSON *data = cJSON_CreateObject();
    if (data == NULL)
        return APEX_ERR_SYS;
    cJSON_AddNumberToObject(data, "percent", percent);
    *res_data = data;
    return APEX_OK;
}

void lcd_app_init(void)
{
    apex_cmd_entry_t text_cmd = {
        .cmd_key = "lcdShowText",
        .function_name = "LCD 显示文本",
        .function_desc = "在 LCD 上显示英文文本（5x7 点阵），可指定坐标与颜色",
        .function_params = "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\",\"description\":\"要显示的文本\"},\"x\":{\"type\":\"integer\"},\"y\":{\"type\":\"integer\"},\"color\":{\"type\":\"string\",\"description\":\"white/black/red/green/blue/yellow/cyan/magenta/gray/orange\"},\"bg\":{\"type\":\"string\"}},\"required\":[\"text\"]}",
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_PARALLEL,
        .handler = lcd_show_text_handler,
        .is_persistent = false,
    };
    apex_cmd_register(text_cmd);

    apex_cmd_entry_t color_cmd = {
        .cmd_key = "lcdShowColor",
        .function_name = "LCD 填充颜色",
        .function_desc = "LCD 整屏或指定区域填充颜色，可传颜色名或 RGB565 数值",
        .function_params = "{\"type\":\"object\",\"properties\":{\"color\":{\"type\":\"string\",\"description\":\"颜色名或 0xRRGGBB\"},\"x\":{\"type\":\"integer\"},\"y\":{\"type\":\"integer\"},\"w\":{\"type\":\"integer\"},\"h\":{\"type\":\"integer\"}},\"required\":[\"color\"]}",
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_PARALLEL,
        .handler = lcd_show_color_handler,
        .is_persistent = false,
    };
    apex_cmd_register(color_cmd);

    apex_cmd_entry_t image_cmd = {
        .cmd_key = "lcdShowImage",
        .function_name = "LCD 显示图案",
        .function_desc = "LCD 显示测试图案：checker(棋盘格)/gradient(渐变)/bars(色条)，可指定两色",
        .function_params = "{\"type\":\"object\",\"properties\":{\"pattern\":{\"type\":\"string\",\"description\":\"checker/gradient/bars\"},\"color\":{\"type\":\"string\"},\"color2\":{\"type\":\"string\"}},\"required\":[\"pattern\"]}",
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_PARALLEL,
        .handler = lcd_show_image_handler,
        .is_persistent = false,
    };
    apex_cmd_register(image_cmd);

    apex_cmd_entry_t bl_cmd = {
        .cmd_key = "lcdBacklight",
        .function_name = "LCD 背光亮度",
        .function_desc = "设置 LCD 背光亮度 0~100",
        .function_params = "{\"type\":\"object\",\"properties\":{\"percent\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":100}},\"required\":[\"percent\"]}",
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_PARALLEL,
        .handler = lcd_backlight_handler,
        .is_persistent = false,
    };
    apex_cmd_register(bl_cmd);

    ESP_LOGI(TAG, "组件注册成功: lcdShowText / lcdShowColor / lcdShowImage / lcdBacklight");
}
