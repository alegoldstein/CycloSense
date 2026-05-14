#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <math.h>

extern "C" {
  #include "gps.h"
}

TFT_eSPI tft = TFT_eSPI();

#define LOOP_PERIOD 35
#define SCREEN_W    320
#define SCREEN_H    240

// ── Row spacing ───────────────────────────────────────────────
#define ROW_START   55    // y of first data row
#define ROW_H       35    // pixels between rows
#define LABEL_X     8     // label left edge
#define VALUE_X     60    // value left edge

void centerText(const char* text, int size, uint16_t fgcolor, uint16_t bgcolor, int cx, int cy);
void drawGPSDisplay(GPS_data &data);
void drawNoFix();
void drawHeader();

// ── Setup ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(2);
  tft.fillScreen(TFT_BLACK);
  drawHeader();
  gps_init();
  delay(100);
  gps_set_mode(GPS_RMC | GPS_GGA, 1);
  gps_set_update_rate(1000);
  drawNoFix();
  Serial.println("=== GPS TRACKER READY ===");
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
  static GPS_data data = {};
  char line[128];
  if (gps_read_line(line, sizeof(line), LOOP_PERIOD) > 0) {
    Serial.printf("[NMEA] %s\n", line);
    if (gps_parse(line, &data)) {
      drawGPSDisplay(data);
    }
  }
  delay(LOOP_PERIOD);
}

// ── Header ────────────────────────────────────────────────────
void drawHeader() {
  tft.fillScreen(TFT_BLACK);
  tft.drawFastHLine(0, 18, SCREEN_W, TFT_YELLOW);
  centerText("GPS TRACKER", 2, TFT_YELLOW, TFT_BLACK, SCREEN_W / 2, 3);
}

// ── No fix ────────────────────────────────────────────────────
void drawNoFix() {
  tft.fillRect(0, 20, SCREEN_W, SCREEN_H - 20, TFT_BLACK);
  centerText("NO FIX", 3, TFT_YELLOW, TFT_BLACK, SCREEN_W / 2, SCREEN_H / 2 - 12);
  Serial.println("[STATUS] Waiting for fix...");
}

// ── Main display ──────────────────────────────────────────────
void drawGPSDisplay(GPS_data &data) {
  tft.fillRect(0, 20, SCREEN_W, SCREEN_H - 20, TFT_BLACK);

  // Fix badge — top right corner
  const char* fixStr = data.valid ? "FIX" : "---";
  tft.setTextSize(2);
  tft.setTextColor(data.valid ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.setCursor(SCREEN_W - 48, 3);
  tft.print(fixStr);

  // Separator under header
  tft.drawFastHLine(0, 18, SCREEN_W, TFT_YELLOW);

  // ── Build rows ────────────────────────────────────────────
  struct Row { const char* label; char value[32]; } rows[5];

  snprintf(rows[0].value, sizeof(rows[0].value), "%.5f %c",
           fabs(data.latitude),  data.latitude  >= 0 ? 'N' : 'S');
  rows[0].label = "LAT";

  snprintf(rows[1].value, sizeof(rows[1].value), "%.5f %c",
           fabs(data.longitude), data.longitude >= 0 ? 'E' : 'W');
  rows[1].label = "LON";

  snprintf(rows[2].value, sizeof(rows[2].value), "%.1f km/h",
           data.speed_knots * 1.852f);
  rows[2].label = "SPD";

  snprintf(rows[3].value, sizeof(rows[3].value), "%d sats",
           data.satellites);
  rows[3].label = "SAT";

  snprintf(rows[4].value, sizeof(rows[4].value), "%02d:%02d:%02d",
           data.hours, data.minutes, data.seconds);
  rows[4].label = "UTC";

  // ── Draw rows ─────────────────────────────────────────────
  for (int i = 0; i < 5; i++) {
    int y = ROW_START + i * ROW_H;

    // Label — dim yellow
    tft.setTextSize(2);
    tft.setTextColor(0x8400, TFT_BLACK);   // dark yellow / amber
    tft.setCursor(LABEL_X, y);
    tft.print(rows[i].label);

    // Value — bright yellow
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(VALUE_X, y);
    tft.print(rows[i].value);

    // Thin divider
    if (i < 4) tft.drawFastHLine(0, y + 26, SCREEN_W, 0x2104);
  }

  // ── Serial mirror ─────────────────────────────────────────
  Serial.println("---------------------");
  Serial.printf("  FIX: %s\n", fixStr);
  for (int i = 0; i < 5; i++)
    Serial.printf("  %s: %s\n", rows[i].label, rows[i].value);
  Serial.println("---------------------");
}

// ── Helpers ───────────────────────────────────────────────────
void centerText(const char* text, int size, uint16_t fgcolor, uint16_t bgcolor, int cx, int cy) {
  tft.setTextColor(fgcolor, bgcolor);
  tft.setTextSize(size);
  int w = tft.textWidth(text);
  tft.setCursor(cx - (w / 2), cy);
  tft.print(text);
}