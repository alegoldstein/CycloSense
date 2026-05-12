#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>


TFT_eSPI tft = TFT_eSPI();

#define LOOP_PERIOD 35 // Display updates every 35 ms


// put function declarations here:
int myFunction(int, int);

void setup() {
  tft.init();
  tft.setRotation(2);
  Serial.begin(115200); // For debug
  tft.fillScreen(TFT_BLACK);
  Serial.println("Setup complete");
}

void loop() {
  centerText("Hello, World!", 2, TFT_WHITE, TFT_BLACK, tft.width() / 2, tft.height() / 2);
  delay(LOOP_PERIOD);
}




void centerText(String text, int size, uint16_t fgcolor, uint16_t bgcolor, int cx, int cy) {
  tft.setTextColor(fgcolor, bgcolor);
  tft.setTextSize(size);
  int w = tft.textWidth(text);
  tft.setCursor(cx - (w / 2), cy);
  tft.print(text);
}