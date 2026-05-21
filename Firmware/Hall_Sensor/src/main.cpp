#include <Arduino.h>
#include <Adafruit_TMAG5273.h>
#include <esp32-audio-classification_inferencing.h>

#define MIC_PIN         34      // GPIO34 = ADC1_CH6, connect SPW2430 DC pin here
#define SAMPLE_RATE     EI_CLASSIFIER_FREQUENCY   // typically 16000 Hz
#define SAMPLE_COUNT    EI_CLASSIFIER_RAW_SAMPLE_COUNT

static int16_t sampleBuffer[SAMPLE_COUNT];
Adafruit_TMAG5273 mag;

static bool microphone_inference_record() {
    uint32_t intervalUs = 1000000 / SAMPLE_RATE;

    for (int i = 0; i < SAMPLE_COUNT; i++) {
        uint32_t start = micros();

        // ADC gives 0–4095 (12-bit). SPW2430 DC bias is ~0.67V (~830 counts).
        // Subtract bias and scale to int16 range.
        int raw = analogRead(MIC_PIN);
        // Serial.printf("Sample %4d: raw=%4d\n", i, raw);
        sampleBuffer[i] = (int16_t)((raw - 830) * 32);

        // Busy-wait for the remainder of the sample interval
        while ((micros() - start) < intervalUs);
    }
    return true;
}

static int get_signal_data(size_t offset, size_t length, float *out_ptr) {
    for (size_t i = 0; i < length; i++) {
        out_ptr[i] = (float)sampleBuffer[offset + i] / 32768.0f;
    }
    return EIDSP_OK;
}

void setup() {
    Serial.begin(9600);
    while (!Serial) delay(10);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    Serial.println("Edge Impulse keyword spotting — SPW2430 analog mic");

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

    Serial.println("Recording...");
    microphone_inference_record();

    signal_t signal;
    signal.total_length = SAMPLE_COUNT;
    signal.get_data     = &get_signal_data;

    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);

    if (err != EI_IMPULSE_OK) {
        Serial.printf("Classifier error: %d\n", err);
        return;
    }

    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        Serial.printf("  %-12s: %.2f\n",
            result.classification[i].label,
            result.classification[i].value);
    }

    float best = 0;
    const char *bestLabel = "";
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > best) {
            best = result.classification[i].value;
            bestLabel = result.classification[i].label;
        }
    }

    if (best > 0.7f) {
        Serial.printf(">>> Detected: %s (%.0f%%)\n", bestLabel, best * 100);
    }

    Serial.println();

    //delay(100);
}