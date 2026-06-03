#include "tft_wrapper.h"
#include <TFT_eSPI.h>

extern TFT_eSPI tft;

extern "C" {

void tft_fill_rect(int x0, int y0, int w, int h, uint16_t color)
{
    tft.fillRect(x0, y0, w, h, color);
}

void tft_draw_line(int x0, int y0, int x1, int y1, uint16_t color)
{
    tft.drawLine(x0, y0, x1, y1, color);
}

void tft_fill_circle(int cx, int cy, int r, uint16_t color)
{
    tft.fillCircle(cx, cy, r, color);
}

void tft_draw_string(int x, int y, const char *str, uint16_t color, uint8_t size)
{
    tft.setTextColor(color, TFT_BLACK);  // second arg = background
    tft.setTextSize(size);
    tft.setCursor(x, y);
    tft.print(str);
}

} /* extern "C" */