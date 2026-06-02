#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <math.h>

extern "C" {
  #include "gps.h"
  #include "hall.h"
}

TFT_eSPI tft = TFT_eSPI();

// ── Pin config — adjust SDA/SCL to match your wiring ─────────
#define I2C_SDA     21
#define I2C_SCL     22

#define LOOP_PERIOD 35
#define SCREEN_W    320
#define SCREEN_H    240

// ── Row layout ────────────────────────────────────────────────
#define ROW_START   55
#define ROW_H       30
#define LABEL_X     8
#define VALUE_X     60
#define BADGE_X     240

// ── Prototypes ────────────────────────────────────────────────
void centerText(const char* text, int size, uint16_t fg, uint16_t bg, int cx, int cy);
void drawHeader();
void drawNoFix();
void drawSpeedBadge(bool fromHall, int y);
void drawGPSDisplay(GPS_data &data);
void drawInitError(const char* msg);

// ─────────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // ── TFT ──────────────────────────────────────────────────
  tft.init();
  tft.setRotation(2);
  tft.fillScreen(TFT_BLACK);
  drawHeader();

  // ── I2C via Arduino Wire (no ESP-IDF I2C driver conflict) ─
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);

  // ── Hall / TMAG5273 ───────────────────────────────────────
  // hall_init() creates the FreeRTOS spike task, verifies the device
  // over Wire, configures TMAG5273 registers, and installs the GPIO ISR.
  esp_err_t hall_ret = hall_init(5);
  if (hall_ret != ESP_OK) {
    Serial.printf("[ERROR] hall_init failed: 0x%X\n", hall_ret);
    drawInitError("HALL INIT FAIL");
    // Continue anyway — GPS will still work, speed shows GPS fallback
  } else {
    Serial.println("[OK] TMAG5273 ready");
  }

  // ── GPS ───────────────────────────────────────────────────
  gps_init();
  delay(100);
  gps_set_mode(GPS_RMC | GPS_GGA, 1);
  gps_set_update_rate(1000);
  drawNoFix();

  Serial.println("=== GPS + WHEEL TRACKER READY ===");
}

// ─────────────────────────────────────────────────────────────
// Loop
// ─────────────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────
// Header
// ─────────────────────────────────────────────────────────────
void drawHeader() {
  tft.fillScreen(TFT_BLACK);
  tft.drawFastHLine(0, 18, SCREEN_W, TFT_YELLOW);
  centerText("GPS + WHEEL TRACKER", 2, TFT_YELLOW, TFT_BLACK, SCREEN_W / 2, 3);
}

// ─────────────────────────────────────────────────────────────
// No fix screen
// ─────────────────────────────────────────────────────────────
void drawNoFix() {
  tft.fillRect(0, 20, SCREEN_W, SCREEN_H - 20, TFT_BLACK);
  centerText("NO FIX", 3, TFT_YELLOW, TFT_BLACK, SCREEN_W / 2, SCREEN_H / 2 - 12);
  Serial.println("[STATUS] Waiting for GPS fix...");
}

// ─────────────────────────────────────────────────────────────
// Init error screen (non-fatal — shows message, keeps running)
// ─────────────────────────────────────────────────────────────
void drawInitError(const char* msg) {
  tft.fillRect(0, 20, SCREEN_W, 30, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setCursor(LABEL_X, 24);
  tft.print(msg);
}

// ─────────────────────────────────────────────────────────────
// Speed source badge  WHL (cyan) = Hall sensor   GPS (dim) = fallback
// ─────────────────────────────────────────────────────────────
void drawSpeedBadge(bool fromHall, int y) {
  tft.setTextSize(1);
  if (fromHall) {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(BADGE_X, y + 4);
    tft.print("WHL");
  } else {
    tft.setTextColor(0x4208, TFT_BLACK);  // dim grey
    tft.setCursor(BADGE_X, y + 4);
    tft.print("GPS");
  }
}

// ─────────────────────────────────────────────────────────────
// Main display
// ─────────────────────────────────────────────────────────────
void drawGPSDisplay(GPS_data &data) {
  tft.fillRect(0, 20, SCREEN_W, SCREEN_H - 20, TFT_BLACK);

  // Fix badge — top right
  const char* fixStr = data.valid ? "FIX" : "---";
  tft.setTextSize(2);
  tft.setTextColor(data.valid ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.setCursor(SCREEN_W - 48, 3);
  tft.print(fixStr);

  tft.drawFastHLine(0, 18, SCREEN_W, TFT_YELLOW);

  // ── Speed source: Hall if available, else GPS ─────────────
  float hall_kmh = hall_get_speed_kmh();
  bool  useHall  = (hall_kmh > 0.0f);
  float disp_kmh = useHall ? hall_kmh : (data.speed_knots * 1.852f);

  // ── Rows ──────────────────────────────────────────────────
  struct Row { const char* label; char value[32]; } rows[6];

  snprintf(rows[0].value, sizeof(rows[0].value), "%.5f %c",
           fabs(data.latitude),  data.latitude  >= 0 ? 'N' : 'S');
  rows[0].label = "LAT";

  snprintf(rows[1].value, sizeof(rows[1].value), "%.5f %c",
           fabs(data.longitude), data.longitude >= 0 ? 'E' : 'W');
  rows[1].label = "LON";

  snprintf(rows[2].value, sizeof(rows[2].value), "%.1f km/h", disp_kmh);
  rows[2].label = "SPD";

  snprintf(rows[3].value, sizeof(rows[3].value), "%d sats", data.satellites);
  rows[3].label = "SAT";

  snprintf(rows[4].value, sizeof(rows[4].value), "%02d:%02d:%02d",
           data.hours, data.minutes, data.seconds);
  rows[4].label = "UTC";

  // REV row: last revolution period in ms (diagnostic)
  uint32_t delta_us = hall_get_last_delta_us();
  if (delta_us > 0)
    snprintf(rows[5].value, sizeof(rows[5].value), "%lu ms", (unsigned long)(delta_us / 1000));
  else
    snprintf(rows[5].value, sizeof(rows[5].value), "---");
  rows[5].label = "REV";

  // ── Draw ──────────────────────────────────────────────────
  for (int i = 0; i < 6; i++) {
    int y = ROW_START + i * ROW_H;

    tft.setTextSize(2);
    tft.setTextColor(0x8400, TFT_BLACK);   // amber label
    tft.setCursor(LABEL_X, y);
    tft.print(rows[i].label);

    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(VALUE_X, y);
    tft.print(rows[i].value);

    if (i == 2) drawSpeedBadge(useHall, y);

    if (i < 5) tft.drawFastHLine(0, y + 22, SCREEN_W, 0x2104);
  }

  // ── Serial mirror ─────────────────────────────────────────
  Serial.println("---------------------");
  Serial.printf("  FIX    : %s\n", fixStr);
  for (int i = 0; i < 6; i++)
    Serial.printf("  %-4s   : %s\n", rows[i].label, rows[i].value);
  Serial.printf("  SPD SRC : %s\n", useHall ? "HALL" : "GPS");
  Serial.println("---------------------");
}

// ─────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────
void centerText(const char* text, int size, uint16_t fg, uint16_t bg, int cx, int cy) {
  tft.setTextColor(fg, bg);
  tft.setTextSize(size);
  tft.setCursor(cx - (tft.textWidth(text) / 2), cy);
  tft.print(text);
}