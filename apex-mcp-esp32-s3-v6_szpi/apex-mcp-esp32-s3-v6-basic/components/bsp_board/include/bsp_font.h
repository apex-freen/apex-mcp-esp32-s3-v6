#pragma once

#include <stdint.h>

/** 5x7 字体字符宽/高（含 1 像素列间距） */
#define BSP_FONT_W 6
#define BSP_FONT_H 7

/**
 * @brief 在 LCD 上绘制文本（英文 5x7 点阵，小写自动转大写）
 *
 * @param x,y   左上角坐标
 * @param text  文本（支持 ASCII 32~126）
 * @param color 前景色 RGB565
 * @param bg    背景色 RGB565
 */
void bsp_lcd_draw_text(int x, int y, const char *text, uint16_t color, uint16_t bg);
