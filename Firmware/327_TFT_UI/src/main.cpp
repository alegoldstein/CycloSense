// #include <Arduino.h>
// #include <TFT_eSPI.h>
// #include <SPI.h>
// #include <Wire.h>
// #include <math.h>

// extern "C" {
//   #include "gps.h"
//   #include "hall.h"
// }

// TFT_eSPI tft = TFT_eSPI();

// // ── Pin config ────────────────────────────────────────────────
// #define I2C_SDA     21
// #define I2C_SCL     22

// // ── Screen (portrait) ─────────────────────────────────────────
// #define SCREEN_W    240
// #define SCREEN_H    320

// // ── Timing ────────────────────────────────────────────────────
// #define LOOP_PERIOD      35
// #define SPEED_REFRESH_MS 150

// // ── Layout ────────────────────────────────────────────────────
// #define HEADER_Y      8
// #define DIVIDER_Y    28

// // Speed block (always visible, top half)
// #define SPEED_Y      55
// #define UNIT_Y      115
// #define SPD_DIVIDER 145

// // GPS block (bottom half, shown when fix available)
// #define GPS_ROW_START  155
// #define GPS_ROW_H       30
// #define GPS_LABEL_X      8
// #define GPS_VALUE_X     60

// // ── Prototypes ────────────────────────────────────────────────
// void centerText(const char* text, int size, uint16_t fg, uint16_t bg, int cx, int cy);
// void drawHeader();
// void drawSpeedBlock();
// void drawGPSBlock(GPS_data &data);
// void clearGPSBlock();

// // ─────────────────────────────────────────────────────────────
// // Setup
// // ─────────────────────────────────────────────────────────────
// void setup() {
//   Serial.begin(115200);

//   tft.init();
//   tft.setRotation(2);           // portrait — unchanged
//   tft.fillScreen(TFT_BLACK);
//   drawHeader();

//   Wire.begin(I2C_SDA, I2C_SCL);
//   Wire.setClock(100000);

//   esp_err_t ret = hall_init(5);
//   if (ret != ESP_OK) {
//     Serial.printf("[ERROR] hall_init failed: 0x%X\n", ret);
//     tft.setTextSize(1);
//     tft.setTextColor(TFT_RED, TFT_BLACK);
//     tft.setCursor(8, DIVIDER_Y + 6);
//     tft.print("SENSOR INIT FAIL");
//   } else {
//     Serial.println("[OK] TMAG5273 ready");
//   }

//   gps_init();
//   delay(100);
//   gps_set_mode(GPS_RMC | GPS_GGA, 1);
//   gps_set_update_rate(1000);

//   Serial.println("=== TRACKER READY ===");
// }

// // ─────────────────────────────────────────────────────────────
// // Loop
// // ─────────────────────────────────────────────────────────────
// void loop() {
//   static GPS_data  data        = {};
//   static bool      hasFix      = false;
//   static uint32_t  lastSpeedDraw = 0;

//   // ── Speed block: refresh at SPEED_REFRESH_MS ─────────────
//   if (millis() - lastSpeedDraw >= SPEED_REFRESH_MS) {
//     lastSpeedDraw = millis();
//     drawSpeedBlock();
//   }

//   // ── GPS: parse when available, update bottom block ────────
//   char line[128];
//   if (gps_read_line(line, sizeof(line), LOOP_PERIOD) > 0) {
//     Serial.printf("[NMEA] %s\n", line);
//     if (gps_parse(line, &data)) {
//       if (data.valid && !hasFix) {
//         // Just got fix — draw GPS section for first time
//         hasFix = true;
//         tft.drawFastHLine(0, SPD_DIVIDER, SCREEN_W, TFT_YELLOW);
//       } else if (!data.valid && hasFix) {
//         // Lost fix — clear GPS section
//         hasFix = false;
//         clearGPSBlock();
//       }
//       if (data.valid) {
//         drawGPSBlock(data);
//       }
//     }
//   }

//   delay(LOOP_PERIOD);
// }

// // ─────────────────────────────────────────────────────────────
// // Header (drawn once at startup)
// // ─────────────────────────────────────────────────────────────
// void drawHeader() {
//   tft.fillRect(0, 0, SCREEN_W, DIVIDER_Y + 1, TFT_BLACK);
//   centerText("WHEEL SPEED", 2, TFT_YELLOW, TFT_BLACK, SCREEN_W / 2, HEADER_Y);
//   tft.drawFastHLine(0, DIVIDER_Y, SCREEN_W, TFT_YELLOW);
// }

// // ─────────────────────────────────────────────────────────────
// // Speed block — Hall sensor, always shown, refreshes independently
// // ─────────────────────────────────────────────────────────────
// void drawSpeedBlock() {
//   float    kmh   = hall_get_speed_kmh();
//   uint32_t delta = hall_get_last_delta_us();

//   // Clear speed area
//   tft.fillRect(0, DIVIDER_Y + 1, SCREEN_W, SPD_DIVIDER - DIVIDER_Y - 1, TFT_BLACK);

//   // Large speed number
//   char spd[16];
//   snprintf(spd, sizeof(spd), "%.1f", kmh);
//   tft.setTextSize(6);
//   tft.setTextColor(kmh > 0.0f ? TFT_CYAN : TFT_WHITE, TFT_BLACK);
//   tft.setCursor((SCREEN_W - tft.textWidth(spd)) / 2, SPEED_Y);
//   tft.print(spd);

//   // km/h label
//   tft.setTextSize(2);
//   tft.setTextColor(TFT_YELLOW, TFT_BLACK);
//   tft.setCursor((SCREEN_W - tft.textWidth("mph")) / 2, UNIT_Y);
//   tft.print("mph");

//   // REV period (small, right-aligned under unit)
//   char rev[20];
//   if (delta > 0)
//     snprintf(rev, sizeof(rev), "rev %lu ms", (unsigned long)(delta / 1000));
//   else
//     snprintf(rev, sizeof(rev), "no pulse");
//   tft.setTextSize(1);
//   tft.setTextColor(kmh > 0.0f ? TFT_GREEN : 0x4208, TFT_BLACK);
//   tft.setCursor((SCREEN_W - tft.textWidth(rev)) / 2, UNIT_Y + 20);
//   tft.print(rev);

//   Serial.printf("[HALL] %.1f km/h  delta=%lu us\n", kmh, (unsigned long)delta);
// }

// // ─────────────────────────────────────────────────────────────
// // GPS block — only drawn when fix is valid
// // ─────────────────────────────────────────────────────────────
// void drawGPSBlock(GPS_data &data) {
//   // Rows: LAT, LON, SAT, UTC
//   struct { const char* label; char value[32]; } rows[4];

//   snprintf(rows[0].value, sizeof(rows[0].value), "%.5f %c",
//            fabs(data.latitude),  data.latitude  >= 0 ? 'N' : 'S');
//   rows[0].label = "LAT";

//   snprintf(rows[1].value, sizeof(rows[1].value), "%.5f %c",
//            fabs(data.longitude), data.longitude >= 0 ? 'E' : 'W');
//   rows[1].label = "LON";

//   snprintf(rows[2].value, sizeof(rows[2].value), "%d sats", data.satellites);
//   rows[2].label = "SAT";

//   snprintf(rows[3].value, sizeof(rows[3].value), "%02d:%02d:%02d",
//            data.hours, data.minutes, data.seconds);
//   rows[3].label = "UTC";

//   for (int i = 0; i < 4; i++) {
//     int y = GPS_ROW_START + i * GPS_ROW_H;

//     tft.fillRect(0, y, SCREEN_W, GPS_ROW_H - 2, TFT_BLACK);

//     tft.setTextSize(2);
//     tft.setTextColor(0x8400, TFT_BLACK);   // amber label
//     tft.setCursor(GPS_LABEL_X, y + 4);
//     tft.print(rows[i].label);

//     tft.setTextColor(TFT_YELLOW, TFT_BLACK);
//     tft.setCursor(GPS_VALUE_X, y + 4);
//     tft.print(rows[i].value);

//     if (i < 3) tft.drawFastHLine(0, y + GPS_ROW_H - 2, SCREEN_W, 0x2104);
//   }
// }

// // ─────────────────────────────────────────────────────────────
// // Clear GPS block when fix is lost
// // ─────────────────────────────────────────────────────────────
// void clearGPSBlock() {
//   tft.fillRect(0, SPD_DIVIDER, SCREEN_W, SCREEN_H - SPD_DIVIDER, TFT_BLACK);
//   centerText("NO GPS FIX", 2, 0x4208, TFT_BLACK, SCREEN_W / 2, GPS_ROW_START + 20);
// }

// // ─────────────────────────────────────────────────────────────
// // Helper
// // ─────────────────────────────────────────────────────────────
// void centerText(const char* text, int size, uint16_t fg, uint16_t bg, int cx, int cy) {
//   tft.setTextColor(fg, bg);
//   tft.setTextSize(size);
//   tft.setCursor(cx - (tft.textWidth(text) / 2), cy);
//   tft.print(text);
// }

    /*
    * main.cpp — Arduino entry point
    *
    * Runs on Arduino framework so TFT_eSPI works natively.
    * FreeRTOS is still fully available (xTaskCreate etc.)
    *
    * Boot sequence:
    *   1. TFT init + black screen
    *   2. SPIFFS mount
    *   3. A* load graph + find route
    *   4. Draw map centred on map origin
    *   5. loop() updates metrics bar with speed + GPS time
    */

  #include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPIFFS.h>
#include <Wire.h>
#include "audio_classifier.h"
#include "tft_wrapper.h"

extern "C" {
#include "astar.h"
#include "graphing.h"
}

#include "gps.h"
#include "hall.h"
#include "ws_telemetry.h"

TFT_eSPI tft = TFT_eSPI();

Graph     g_graph;
uint32_t *g_path     = NULL;
int       g_path_len = 0;

#define ROUTE_START_NODE  7751u
#define ROUTE_END_NODE    2377u
#define MAX_PATH          2048

GPS_data     s_gps     = {};
portMUX_TYPE s_gps_mux = portMUX_INITIALIZER_UNLOCKED;

static void gps_task(void *pv)
{
    char buf[128];
    while (1) {
        if (gps_read_line(buf, sizeof(buf), 1100) > 0) {
            GPS_data tmp = {};
            if (gps_parse(buf, &tmp)) {
                portENTER_CRITICAL(&s_gps_mux);
                s_gps = tmp;
                portEXIT_CRITICAL(&s_gps_mux);
            }
        }
    }
}

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== BOOTING ===");

    /* TFT first — before I2S touches DMA */
    tft.init();
    tft.setRotation(2);
    tft.fillScreen(TFT_BLACK);
   Serial.println("TFT ready");

    Wire.begin(21, 22);
    Wire.setClock(HALL_I2C_SCL_SPEED);
    hall_init(5);

    gps_init();
    gps_set_mode(GPS_RMC | GPS_GGA, 1);
    gps_set_update_rate(1000);
    xTaskCreate(gps_task, "gps", 4096, NULL, 4, NULL);

    if (!SPIFFS.begin(true)) { Serial.println("SPIFFS failed"); return; }
    Serial.println("SPIFFS mounted");

    if (astar_load_graph(&g_graph, "/spiffs/graph.bin") != 0) {
        Serial.println("graph load failed"); return;
    }

    g_path = (uint32_t *)malloc(MAX_PATH * sizeof(uint32_t));
    g_path_len = astar_find(&g_graph, ROUTE_START_NODE, ROUTE_END_NODE,
                            g_path, MAX_PATH);

    draw_background(&g_graph);
    draw_route(&g_graph, g_path, g_path_len);
    tft.fillCircle(120, 120, 5, TFT_RED);
    tft.drawFastHLine(0, 240, 240, TFT_BLUE);
    tft.fillRect(0, 241, 240, 79, TFT_BLACK);
    draw_metrics(0, 0, 0);

    /* audio last — after display is fully drawn, uses I2S_NUM_1 */
    audio_classifier_init();

    Serial.println("display ready");

    ws_telemetry_start("iPhone", "mrt4mb0urin3m4n", "10.105.247.133", 8000);
}

void loop()
{
    static unsigned long last_update = 0;
    static uint32_t      last_spd    = 9999;
    static bool          last_squeak = false;

    if (millis() - last_update < 250) return;
    last_update = millis();

    /* squeak banner */
    bool squeak = (bool)g_squeak_detected;
    if (squeak != last_squeak) {
        last_squeak = squeak;
        tft_show_squeak_warning(squeak ? 1 : 0);
       Serial.printf("[DISPLAY] squeak banner: %s  confidence=%.2f\n",
                     squeak ? "ON" : "OFF", (float)g_squeak_confidence);
    }

    /* speed + GPS */
    float speed_mph = hall_get_speed_kmh();
    uint32_t spd = (uint32_t)(speed_mph * 10);

    portENTER_CRITICAL(&s_gps_mux);
    GPS_data gps = s_gps;
    portEXIT_CRITICAL(&s_gps_mux);

    if (spd != last_spd) {
        last_spd = spd;

        tft.fillRect(0, 241, 240, 79, TFT_BLACK);

        char spd_str[16];
        snprintf(spd_str, sizeof(spd_str), "%.1f", speed_mph);
        tft.setTextFont(4);
        tft.setTextColor(speed_mph > 0.0f ? TFT_CYAN : TFT_WHITE, TFT_BLACK);
        tft.setCursor(10, 248);
        tft.print(spd_str);

        tft.setTextFont(1);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setCursor(10, 292);
        tft.print("mph");

        if (gps.valid) {
            char time_str[12];
            snprintf(time_str, sizeof(time_str), "%02u:%02u", gps.hours, gps.minutes);
            tft.setTextFont(2);
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            tft.setCursor(170, 292);
            tft.print(time_str);
        }

        Serial.printf("[DISPLAY] speed=%.1f mph  squeaky=%.2f  normal=%.2f\n",
                      speed_mph,
                      (float)g_squeak_confidence,
                      (float)g_normal_confidence);
       
    }
}
