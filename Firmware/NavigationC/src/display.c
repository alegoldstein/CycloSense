/*
 * display.c — ILI9341 driver using spi_master (pure ESP-IDF C, no Arduino)
 *
 * Implements: init, fill_rect, draw_line (Bresenham), fill_circle,
 *             draw_char / draw_string using a minimal 5x7 bitmap font.
 */

#include "display.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "display";
static spi_device_handle_t s_spi;

/* -------------------------------------------------------------------------
 * Minimal 5×7 ASCII bitmap font (chars 0x20–0x7E)
 * Each char is 5 bytes, one byte per column, bit0=top
 * ---------------------------------------------------------------------- */
static const uint8_t FONT5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x00,0x00,0x5F,0x00,0x00}, /* '!' */
    {0x00,0x07,0x00,0x07,0x00}, /* '"' */
    {0x14,0x7F,0x14,0x7F,0x14}, /* '#' */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* '$' */
    {0x23,0x13,0x08,0x64,0x62}, /* '%' */
    {0x36,0x49,0x55,0x22,0x50}, /* '&' */
    {0x00,0x05,0x03,0x00,0x00}, /* ''' */
    {0x00,0x1C,0x22,0x41,0x00}, /* '(' */
    {0x00,0x41,0x22,0x1C,0x00}, /* ')' */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* '*' */
    {0x08,0x08,0x3E,0x08,0x08}, /* '+' */
    {0x00,0x50,0x30,0x00,0x00}, /* ',' */
    {0x08,0x08,0x08,0x08,0x08}, /* '-' */
    {0x00,0x60,0x60,0x00,0x00}, /* '.' */
    {0x20,0x10,0x08,0x04,0x02}, /* '/' */
    {0x3E,0x51,0x49,0x45,0x3E}, /* '0' */
    {0x00,0x42,0x7F,0x40,0x00}, /* '1' */
    {0x42,0x61,0x51,0x49,0x46}, /* '2' */
    {0x21,0x41,0x45,0x4B,0x31}, /* '3' */
    {0x18,0x14,0x12,0x7F,0x10}, /* '4' */
    {0x27,0x45,0x45,0x45,0x39}, /* '5' */
    {0x3C,0x4A,0x49,0x49,0x30}, /* '6' */
    {0x01,0x71,0x09,0x05,0x03}, /* '7' */
    {0x36,0x49,0x49,0x49,0x36}, /* '8' */
    {0x06,0x49,0x49,0x29,0x1E}, /* '9' */
    {0x00,0x36,0x36,0x00,0x00}, /* ':' */
    {0x00,0x56,0x36,0x00,0x00}, /* ';' */
    {0x00,0x08,0x14,0x22,0x41}, /* '<' */
    {0x14,0x14,0x14,0x14,0x14}, /* '=' */
    {0x41,0x22,0x14,0x08,0x00}, /* '>' */
    {0x02,0x01,0x51,0x09,0x06}, /* '?' */
    {0x32,0x49,0x79,0x41,0x3E}, /* '@' */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 'A' */
    {0x7F,0x49,0x49,0x49,0x36}, /* 'B' */
    {0x3E,0x41,0x41,0x41,0x22}, /* 'C' */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 'D' */
    {0x7F,0x49,0x49,0x49,0x41}, /* 'E' */
    {0x7F,0x09,0x09,0x09,0x01}, /* 'F' */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 'G' */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 'H' */
    {0x00,0x41,0x7F,0x41,0x00}, /* 'I' */
    {0x20,0x40,0x41,0x3F,0x01}, /* 'J' */
    {0x7F,0x08,0x14,0x22,0x41}, /* 'K' */
    {0x7F,0x40,0x40,0x40,0x40}, /* 'L' */
    {0x7F,0x02,0x04,0x02,0x7F}, /* 'M' */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 'N' */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 'O' */
    {0x7F,0x09,0x09,0x09,0x06}, /* 'P' */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 'Q' */
    {0x7F,0x09,0x19,0x29,0x46}, /* 'R' */
    {0x46,0x49,0x49,0x49,0x31}, /* 'S' */
    {0x01,0x01,0x7F,0x01,0x01}, /* 'T' */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 'U' */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 'V' */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 'W' */
    {0x63,0x14,0x08,0x14,0x63}, /* 'X' */
    {0x07,0x08,0x70,0x08,0x07}, /* 'Y' */
    {0x61,0x51,0x49,0x45,0x43}, /* 'Z' */
    {0x00,0x7F,0x41,0x41,0x00}, /* '[' */
    {0x02,0x04,0x08,0x10,0x20}, /* '\' */
    {0x00,0x41,0x41,0x7F,0x00}, /* ']' */
    {0x04,0x02,0x01,0x02,0x04}, /* '^' */
    {0x40,0x40,0x40,0x40,0x40}, /* '_' */
    {0x00,0x01,0x02,0x04,0x00}, /* '`' */
    {0x20,0x54,0x54,0x54,0x78}, /* 'a' */
    {0x7F,0x48,0x44,0x44,0x38}, /* 'b' */
    {0x38,0x44,0x44,0x44,0x20}, /* 'c' */
    {0x38,0x44,0x44,0x48,0x7F}, /* 'd' */
    {0x38,0x54,0x54,0x54,0x18}, /* 'e' */
    {0x08,0x7E,0x09,0x01,0x02}, /* 'f' */
    {0x08,0x14,0x54,0x54,0x3C}, /* 'g' */
    {0x7F,0x08,0x04,0x04,0x78}, /* 'h' */
    {0x00,0x44,0x7D,0x40,0x00}, /* 'i' */
    {0x20,0x40,0x44,0x3D,0x00}, /* 'j' */
    {0x7F,0x10,0x28,0x44,0x00}, /* 'k' */
    {0x00,0x41,0x7F,0x40,0x00}, /* 'l' */
    {0x7C,0x04,0x18,0x04,0x78}, /* 'm' */
    {0x7C,0x08,0x04,0x04,0x78}, /* 'n' */
    {0x38,0x44,0x44,0x44,0x38}, /* 'o' */
    {0x7C,0x14,0x14,0x14,0x08}, /* 'p' */
    {0x08,0x14,0x14,0x18,0x7C}, /* 'q' */
    {0x7C,0x08,0x04,0x04,0x08}, /* 'r' */
    {0x48,0x54,0x54,0x54,0x20}, /* 's' */
    {0x04,0x3F,0x44,0x40,0x20}, /* 't' */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 'u' */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 'v' */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 'w' */
    {0x44,0x28,0x10,0x28,0x44}, /* 'x' */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 'y' */
    {0x44,0x64,0x54,0x4C,0x44}, /* 'z' */
    {0x00,0x08,0x36,0x41,0x00}, /* '{' */
    {0x00,0x00,0x7F,0x00,0x00}, /* '|' */
    {0x00,0x41,0x36,0x08,0x00}, /* '}' */
    {0x08,0x08,0x2A,0x1C,0x08}, /* '~' */
};

/* -------------------------------------------------------------------------
 * SPI helpers — DC pin selects command (0) vs data (1)
 * ---------------------------------------------------------------------- */
static void spi_cmd(uint8_t cmd)
{
    gpio_set_level(TFT_DC, 0);
    spi_transaction_t t = { .length = 8, .tx_buffer = &cmd };
    spi_device_polling_transmit(s_spi, &t);
}

static void spi_data(const uint8_t *data, size_t len)
{
    if (!len) return;
    gpio_set_level(TFT_DC, 1);
    spi_transaction_t t = { .length = len * 8, .tx_buffer = data };
    spi_device_polling_transmit(s_spi, &t);
}

static void spi_u8(uint8_t cmd, uint8_t val)
{
    spi_cmd(cmd);
    spi_data(&val, 1);
}

/* -------------------------------------------------------------------------
 * ILI9341 register init
 * ---------------------------------------------------------------------- */
static void ili9341_init(void)
{
    /* hardware reset */
    gpio_set_level(TFT_RST, 0); vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TFT_RST, 1); vTaskDelay(pdMS_TO_TICKS(120));

    spi_cmd(0x01); vTaskDelay(pdMS_TO_TICKS(150)); /* sw reset */

    spi_cmd(0xCB); spi_data((uint8_t[]){0x39,0x2C,0x00,0x34,0x02}, 5);
    spi_cmd(0xCF); spi_data((uint8_t[]){0x00,0xC1,0x30}, 3);
    spi_cmd(0xE8); spi_data((uint8_t[]){0x85,0x00,0x78}, 3);
    spi_cmd(0xEA); spi_data((uint8_t[]){0x00,0x00}, 2);
    spi_cmd(0xED); spi_data((uint8_t[]){0x64,0x03,0x12,0x81}, 4);
    spi_u8(0xF7, 0x20);
    spi_u8(0xC0, 0x23);
    spi_u8(0xC1, 0x10);
    spi_cmd(0xC5); spi_data((uint8_t[]){0x3E,0x28}, 2);
    spi_u8(0xC7, 0x86);
    spi_u8(0x36, 0x48);  /* MADCTL: BGR, portrait */
    spi_u8(0x3A, 0x55);  /* pixel format: 16-bit */
    spi_cmd(0xB1); spi_data((uint8_t[]){0x00,0x18}, 2);
    spi_cmd(0xB6); spi_data((uint8_t[]){0x08,0x82,0x27}, 3);
    spi_u8(0xF2, 0x00);
    spi_u8(0x26, 0x01);
    spi_cmd(0xE0);
    spi_data((uint8_t[]){0x0F,0x31,0x2B,0x0C,0x0E,0x08,
                          0x4E,0xF1,0x37,0x07,0x10,0x03,
                          0x0E,0x09,0x00}, 15);
    spi_cmd(0xE1);
    spi_data((uint8_t[]){0x00,0x0E,0x14,0x03,0x11,0x07,
                          0x31,0xC1,0x48,0x08,0x0F,0x0C,
                          0x31,0x36,0x0F}, 15);
    spi_cmd(0x11); vTaskDelay(pdMS_TO_TICKS(120)); /* sleep out */
    spi_cmd(0x29);                                  /* display on */
}

/* -------------------------------------------------------------------------
 * Set pixel window
 * ---------------------------------------------------------------------- */
static void set_window(int x0, int y0, int x1, int y1)
{
    spi_cmd(0x2A);
    spi_data((uint8_t[]){x0>>8, x0&0xFF, (x1-1)>>8, (x1-1)&0xFF}, 4);
    spi_cmd(0x2B);
    spi_data((uint8_t[]){y0>>8, y0&0xFF, (y1-1)>>8, (y1-1)&0xFF}, 4);
    spi_cmd(0x2C);
}

/* -------------------------------------------------------------------------
 * Init
 * ---------------------------------------------------------------------- */
esp_err_t display_init(void)
{
    gpio_set_direction(TFT_DC,  GPIO_MODE_OUTPUT);
    gpio_set_direction(TFT_RST, GPIO_MODE_OUTPUT);
    if (TFT_BL >= 0) {
        gpio_set_direction(TFT_BL, GPIO_MODE_OUTPUT);
        gpio_set_level(TFT_BL, 1);
    }

    spi_bus_config_t bus = {
        .mosi_io_num     = TFT_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = TFT_SCLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = TFT_W * 16 * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev = {
    .clock_speed_hz = 26 * 1000 * 1000,
    .mode           = 0,
    .spics_io_num   = TFT_CS,
    .queue_size     = 7,
    .flags          = SPI_DEVICE_HALFDUPLEX,
};
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev, &s_spi));

    ili9341_init();
    display_fill_rect(0, 0, TFT_W, TFT_H, COLOR_BLACK);

    ESP_LOGI(TAG, "ILI9341 ready");
    return ESP_OK;
}

/* -------------------------------------------------------------------------
 * Fill rectangle — pushes one row at a time to limit stack usage
 * ---------------------------------------------------------------------- */
void display_fill_rect(int x0, int y0, int x1, int y1, uint16_t color)
{
    int w = x1 - x0;
    int h = y1 - y0;
    if (w <= 0 || h <= 0) return;

    uint16_t *row = (uint16_t *)malloc(w * sizeof(uint16_t));
    if (!row) return;

    /* ILI9341 expects big-endian RGB565 */
    uint16_t be = (color >> 8) | (color << 8);
    for (int i = 0; i < w; i++) row[i] = be;

    set_window(x0, y0, x1, y1);
    gpio_set_level(TFT_DC, 1);
    for (int y = 0; y < h; y++) {
        spi_transaction_t t = {
            .length    = w * 16,
            .tx_buffer = row,
        };
        spi_device_polling_transmit(s_spi, &t);
    }
    free(row);
}

/* -------------------------------------------------------------------------
 * Single pixel
 * ---------------------------------------------------------------------- */
static inline void put_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || x >= TFT_W || y < 0 || y >= TFT_H) return;
    uint16_t be = (color >> 8) | (color << 8);
    set_window(x, y, x + 1, y + 1);
    spi_data((uint8_t *)&be, 2);
}

/* -------------------------------------------------------------------------
 * Bresenham line
 * ---------------------------------------------------------------------- */
void display_draw_line(int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx =  abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* -------------------------------------------------------------------------
 * Filled circle
 * ---------------------------------------------------------------------- */
void display_fill_circle(int cx, int cy, int r, uint16_t color)
{
    for (int dy = -r; dy <= r; dy++) {
        int half_w = (int)sqrt((double)(r*r - dy*dy));
        int x0 = cx - half_w;
        int x1 = cx + half_w + 1;
        int py = cy + dy;
        if (py < 0 || py >= TFT_H) continue;
        if (x0 < 0)     x0 = 0;
        if (x1 > TFT_W) x1 = TFT_W;
        if (x1 > x0)    display_fill_rect(x0, py, x1, py + 1, color);
    }
}

/* -------------------------------------------------------------------------
 * Draw one character from 5×7 font, scaled by `size`
 * ---------------------------------------------------------------------- */
void display_draw_char(int x, int y, char c, uint16_t color, uint8_t size)
{
    if (c < 0x20 || c > 0x7E) c = '?';
    const uint8_t *glyph = FONT5x7[c - 0x20];

    uint16_t fg = (color >> 8) | (color << 8);
    uint16_t bg = 0x0000;  /* black background, big-endian */

    for (int col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 7; row++) {
            uint16_t px = (bits & (1 << row)) ? fg : bg;
            if (size == 1) {
                put_pixel(x + col, y + row, (px >> 8) | (px << 8));
            } else {
                display_fill_rect(
                    x + col * size, y + row * size,
                    x + col * size + size, y + row * size + size,
                    (px >> 8) | (px << 8));
            }
        }
    }
}

/* -------------------------------------------------------------------------
 * Draw string
 * ---------------------------------------------------------------------- */
void display_draw_string(int x, int y, const char *s, uint16_t color, uint8_t size)
{
    int cx = x;
    while (*s) {
        display_draw_char(cx, y, *s++, color, size);
        cx += (5 + 1) * size;  /* 5px glyph + 1px gap */
    }
}