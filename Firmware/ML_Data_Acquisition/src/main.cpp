#include <Arduino.h>
#include <driver/i2s.h>

// ─────────────────────────────────────────────
//  INMP441 Pin Mapping  (change to match your wiring)
// ─────────────────────────────────────────────
#define I2S_WS   15   // Word Select  (L/R clock)
#define I2S_SCK  14   // Bit Clock
#define I2S_SD   32   // Serial Data  (SD / DOUT on sensor)

// ─────────────────────────────────────────────
//  Audio Parameters
//  Edge Impulse typically wants 16 kHz mono 16-bit PCM
// ─────────────────────────────────────────────
#define SAMPLE_RATE       16000
#define BITS_PER_SAMPLE   I2S_BITS_PER_SAMPLE_32BIT   // INMP441 outputs 24-bit in 32-bit frame
#define CHANNEL_FORMAT    I2S_CHANNEL_FMT_ONLY_LEFT    // INMP441 L/R pin tied LOW → left channel
#define DMA_BUF_COUNT     8
#define DMA_BUF_LEN       64     // samples per DMA buffer

// Edge Impulse Data Forwarder expects raw 16-bit signed samples over serial
// Capture duration per recording (ms) — adjust as needed
#define CAPTURE_DURATION_MS  1000
#define TOTAL_SAMPLES        (SAMPLE_RATE * CAPTURE_DURATION_MS / 1000)

// ─────────────────────────────────────────────
//  Globals
// ─────────────────────────────────────────────
static int32_t  rawBuffer[DMA_BUF_LEN];   // 32-bit I2S frames from INMP441
static int16_t  pcmBuffer[TOTAL_SAMPLES]; // downscaled 16-bit PCM

// ─────────────────────────────────────────────
//  I2S Initialisation
// ─────────────────────────────────────────────
void i2s_init() {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = BITS_PER_SAMPLE,
        .channel_format       = CHANNEL_FORMAT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = DMA_BUF_COUNT,
        .dma_buf_len          = DMA_BUF_LEN,
        .use_apll             = false,
        .tx_desc_auto_clear   = false,
        .fixed_mclk           = 0
    };

    i2s_pin_config_t pins = {
        .bck_io_num   = I2S_SCK,
        .ws_io_num    = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = I2S_SD
    };

    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM_0, &pins));
    ESP_ERROR_CHECK(i2s_zero_dma_buffer(I2S_NUM_0));

    Serial.println("[I2S] Driver installed OK");
}

// ─────────────────────────────────────────────
//  Capture one CAPTURE_DURATION_MS block of audio
//  Returns number of 16-bit samples captured.
// ─────────────────────────────────────────────
int capture_audio() {
    size_t bytesRead  = 0;
    int    sampleIdx  = 0;

    while (sampleIdx < TOTAL_SAMPLES) {
        size_t toRead = min((int)DMA_BUF_LEN, TOTAL_SAMPLES - sampleIdx);

        esp_err_t err = i2s_read(
            I2S_NUM_0,
            rawBuffer,
            toRead * sizeof(int32_t),
            &bytesRead,
            portMAX_DELAY
        );

        if (err != ESP_OK) {
            Serial.printf("[I2S] Read error: %d\n", err);
            break;
        }

        int samplesRead = bytesRead / sizeof(int32_t);

        for (int i = 0; i < samplesRead && sampleIdx < TOTAL_SAMPLES; i++) {
            // INMP441 data is left-justified in 32-bit word → shift down 14 bits
            // to centre the 24-bit value, then cast to 16-bit (keep upper 16 bits).
            int32_t val = rawBuffer[i] >> 8;
            pcmBuffer[sampleIdx++] = (int16_t)constrain(val, -32768, 32767);
        }
    }

    return sampleIdx;
}

// ─────────────────────────────────────────────
//  Send samples to Edge Impulse Data Forwarder
//  Format: one sample per line, plain ASCII integer
//  (The Data Forwarder expects comma-separated columns;
//   for mono audio, one column is fine.)
// ─────────────────────────────────────────────
void send_to_edge_impulse(int numSamples) {
    for (int i = 0; i < numSamples; i++) {
        Serial.println(pcmBuffer[i]);
    }
}

// ─────────────────────────────────────────────
//  Optional: send raw bytes (use with edge-impulse-cli
//  --format raw or a custom Python ingestion script)
// ─────────────────────────────────────────────
void send_raw_bytes(int numSamples) {
    // 2-byte little-endian per sample
    Serial.write((uint8_t*)pcmBuffer, numSamples * sizeof(int16_t));
}

// ─────────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println("=== INMP441 → Edge Impulse Data Forwarder ===");
    Serial.printf("Sample rate : %d Hz\n", SAMPLE_RATE);
    Serial.printf("Duration    : %d ms  (%d samples)\n", CAPTURE_DURATION_MS, TOTAL_SAMPLES);
    Serial.println("Send 'c' to capture, 'r' to capture+send raw bytes");

    i2s_init();
}

// ─────────────────────────────────────────────
//  Loop — command-driven captures
// ─────────────────────────────────────────────
void loop() {
    if (Serial.available()) {
        char cmd = Serial.read();

        if (cmd == 'c') {
            Serial.println("[*] Capturing...");
            int n = capture_audio();
            Serial.printf("[*] Captured %d samples — sending to Edge Impulse forwarder\n", n);
            send_to_edge_impulse(n);
            Serial.println("[*] Done.");

        } else if (cmd == 'r') {
            Serial.println("[*] Capturing (raw bytes)...");
            int n = capture_audio();
            send_raw_bytes(n);
            // No extra serial text — raw mode
        }
    }
}