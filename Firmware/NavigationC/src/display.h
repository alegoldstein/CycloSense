#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include "esp_err.h"

#define TFT_MOSI   23
#define TFT_MISO   -1
#define TFT_SCLK   18
#define TFT_CS      5
#define TFT_DC      2
#define TFT_RST     4
#define TFT_BL     -1

#define TFT_W  240
#define TFT_H  320

/* RGB565 */
#define COLOR_BLACK    0x0000
#define COLOR_WHITE    0xFFFF
#define COLOR_RED      0xF800
#define COLOR_GREEN    0x07E0
#define COLOR_BLUE     0x001F
#define COLOR_DARKGREY 0x4208
#define COLOR_YELLOW   0xFFE0

esp_err_t display_init(void);
void display_fill_rect(int x0, int y0, int x1, int y1, uint16_t color);
void display_draw_line(int x0, int y0, int x1, int y1, uint16_t color);
void display_fill_circle(int cx, int cy, int r, uint16_t color);
void display_draw_char(int x, int y, char c, uint16_t color, uint8_t size);
void display_draw_string(int x, int y, const char *s, uint16_t color, uint8_t size);

#endif