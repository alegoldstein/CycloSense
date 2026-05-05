#include <Arduino.h>
#include <driver/i2s.h>

// Replace with your actual Edge Impulse library header
#include <your-ei-project_inferencing.h>

// ── I2S config ──────────────────────────────────────────────────────────────
#define I2S_WS    15
#define I2S_SCK   14
#define I2S_SD    22
#define I2S_PORT  I2S_NUM_0

// EI expects 16 kHz mono audio
#define SAMPLE_RATE       EI_CLASSIFIER_FREQUENCY   // typically 16000
#define SAMPLE_BITS       32
#define READ_LEN          (EI_CLASSIFIER_RAW_SAMPLE_COUNT * 2) // 32-bit reads

static int16_t sampleBuffer[EI_CLASSIFIER_RAW_SAMPLE_COUNT];

// ── I2S init ────────────────────────────────────────────────────────────────
void i2s_init() {
    i2s_config_t config = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 8,
        .dma_buf_len          = 512,
        .use_apll             = false,
    };

    i2s_pin_config_t pins = {
        .bck_io_num   = I2S_SCK,
        .ws_io_num    = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = I2S_SD,
    };

    i2s_driver_install(I2S_PORT, &config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pins);
    i2s_zero_dma_buffer(I2S_PORT);
}

// ── Audio capture callback (called by EI SDK) ────────────────────────────────
static bool microphone_inference_record() {
    int32_t raw[EI_CLASSIFIER_RAW_SAMPLE_COUNT];
    size_t bytesRead = 0;

    i2s_read(I2S_PORT, raw, sizeof(raw), &bytesRead, portMAX_DELAY);

    int samplesRead = bytesRead / sizeof(int32_t);
    for (int i = 0; i < samplesRead; i++) {
        // INMP441 outputs data in top 18 bits of the 32-bit word
        sampleBuffer[i] = (int16_t)(raw[i] >> 14);
    }
    return true;
}

// ── EI signal wrapper ────────────────────────────────────────────────────────
static int get_signal_data(size_t offset, size_t length, float *out_ptr) {
    for (size_t i = 0; i < length; i++) {
        out_ptr[i] = (float)sampleBuffer[offset + i] / 32768.0f;
    }
    return EIDSP_OK;
}

// ── Setup & loop ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("Edge Impulse keyword spotting — ESP32");
    i2s_init();
    delay(500);
}

void loop() {
    Serial.println("Recording...");
    microphone_inference_record();

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
    signal.get_data     = &get_signal_data;

    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);

    if (err != EI_IMPULSE_OK) {
        Serial.printf("Classifier error: %d\n", err);
        return;
    }

    // Print results
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        Serial.printf("  %-12s: %.2f\n",
            result.classification[i].label,
            result.classification[i].value);
    }

    // Act on best prediction
    float best = 0;
    const char* bestLabel = "";
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > best) {
            best      = result.classification[i].value;
            bestLabel = result.classification[i].label;
        }
    }

    if (best > 0.7f) {  // confidence threshold — tune as needed
        Serial.printf(">>> Detected: %s (%.0f%%)\n", bestLabel, best * 100);
    }

    Serial.println();
}
