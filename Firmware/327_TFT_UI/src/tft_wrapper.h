#ifndef TFT_WRAPPER_H
#define TFT_WRAPPER_H

#include <stdint.h>

#define COLOR_BLACK    0x0000
#define COLOR_WHITE    0xFFFF
#define COLOR_RED      0xF800
#define COLOR_GREEN    0x07E0
#define COLOR_BLUE     0x001F
#define COLOR_DARKGREY 0x4208
#define COLOR_YELLOW   0xFFE0

#ifdef __cplusplus
extern "C" {
#endif

void tft_fill_rect(int x0, int y0, int w, int h, uint16_t color);
void tft_draw_line(int x0, int y0, int x1, int y1, uint16_t color);
void tft_fill_circle(int cx, int cy, int r, uint16_t color);
void tft_draw_string(int x, int y, const char *str, uint16_t color, uint8_t size);

#ifdef __cplusplus
}
#endif

#endif