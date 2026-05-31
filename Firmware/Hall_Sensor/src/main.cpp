#include <Arduino.h>
#include <Adafruit_TMAG5273.h>

Adafruit_TMAG5273 mag;

void setup() {
    Serial.begin(9600);
    while (!Serial) delay(10);

    if (!mag.begin()) {
        Serial.println("TMAG5273 not found. Check wiring!");
        while (1) delay(10);
    }

    Serial.println("TMAG5273 ready");
}

void loop() {
    float x = mag.readMagneticX();
    float y = mag.readMagneticY();
    float z = mag.readMagneticZ();

    Serial.printf("X: %.1f  Y: %.1f  Z: %.1f  uT\n", x, y, z);
}