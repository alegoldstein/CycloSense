/*
 * audio_classifier.cpp — continuous inference mode
 *
 * Uses run_classifier_continuous() with SLICE_SIZE (4000 samples).
 * RAM: 4000 × 4 × 2 = 32 KB instead of 128 KB for full window.
 */

#include "audio_classifier.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include <esp32-audio-classification_inferencing.h>

volatile bool  g_squeak_detected   = false;
volatile float g_squeak_confidence = 0.0f;
volatile float g_normal_confidence = 0.0f;

#define I2S_PORT        I2S_NUM_1
#define I2S_SAMPLE_RATE EI_CLASSIFIER_FREQUENCY
#define SLICE_LEN       EI_CLASSIFIER_SLICE_SIZE

static void i2s_init(void)
{
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = I2S_SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 8,
        .dma_buf_len          = 512,
        .use_apll             = false,
        .tx_desc_auto_clear   = false,
        .fixed_mclk           = 0,
    };
    i2s_pin_config_t pins = {
        .bck_io_num   = MIC_SCK_PIN,
        .ws_io_num    = MIC_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = MIC_SD_PIN,
    };
    i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
    i2s_set_pin(I2S_PORT, &pins);
    i2s_zero_dma_buffer(I2S_PORT);
    Serial.println("[AUDIO] I2S mic init OK");
}

static float *s_float_buf = nullptr;

static int get_signal_data(size_t offset, size_t length, float *out_ptr)
{
    if (!s_float_buf) return -1;
    for (size_t i = 0; i < length; i++)
        out_ptr[i] = s_float_buf[offset + i];
    return EIDSP_OK;
}

static void classifier_task(void *pv)
{
    Serial.printf("[AUDIO] Task started — slice=%d  heap=%d\n",
                  SLICE_LEN, (int)ESP.getFreeHeap());

    int32_t *raw_buf   = (int32_t *)malloc(SLICE_LEN * sizeof(int32_t));
    float   *float_buf = (float   *)malloc(SLICE_LEN * sizeof(float));

    if (!raw_buf || !float_buf) {
        Serial.printf("[AUDIO] OOM — heap=%d\n", (int)ESP.getFreeHeap());
        free(raw_buf);
        free(float_buf);
        vTaskDelete(NULL);
        return;
    }
    s_float_buf = float_buf;
    Serial.printf("[AUDIO] Buffers OK — heap=%d\n", (int)ESP.getFreeHeap());

    /* reset continuous inference rolling window */
    run_classifier_init();

    static uint32_t last_status_ms = 0;
    static int32_t  dc_offset      = 0;
    static uint32_t slices_seen    = 0;

    while (1) {
        /* read one slice */
        size_t bytes_read = 0;
        i2s_read(I2S_PORT,
                 raw_buf,
                 SLICE_LEN * sizeof(int32_t),
                 &bytes_read,
                 portMAX_DELAY);

        /* DC removal + >> 16 + normalise */
        size_t samples = bytes_read / sizeof(int32_t);
        for (size_t i = 0; i < samples && i < SLICE_LEN; i++) {
            dc_offset    = dc_offset + (raw_buf[i] - dc_offset) / 8;
            int32_t filt = raw_buf[i] - dc_offset;
            int16_t s16  = (int16_t)(filt >> 16);
            float_buf[i] = (float)s16 / 32768.0f;
        }

        signal_t signal;
        signal.total_length = SLICE_LEN;
        signal.get_data     = &get_signal_data;

        ei_impulse_result_t result = { 0 };
        EI_IMPULSE_ERROR err = run_classifier_continuous(&signal, &result, false, false);
        if (err != EI_IMPULSE_OK) {
            Serial.printf("[AUDIO] classifier error: %d\n", err);
            continue;
        }

        slices_seen++;

        /* wait until we have seen at least one full window (4 slices) */
        if (slices_seen < EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW) continue;

        float squeak_conf = 0.0f;
        float normal_conf = 0.0f;

        for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
            String label = String(result.classification[i].label);
            float  val   = result.classification[i].value;
            if (label == "squeaky")           squeak_conf = val;
            else if (label == "noise_normal") normal_conf = val;
        }

        g_squeak_confidence = squeak_conf;
        g_normal_confidence = normal_conf;
        g_squeak_detected   = (squeak_conf >= SQUEAK_THRESHOLD);

        uint32_t now = millis();
        if (now - last_status_ms >= 2000) {
            last_status_ms = now;
           Serial.printf("[AUDIO] squeaky=%.2f  normal=%.2f  → %s\n",
                         squeak_conf, normal_conf,
                         g_squeak_detected ? "*** SQUEAK ***" : "normal");
        }

        if (g_squeak_detected) {
            Serial.printf("[AUDIO] SQUEAK DETECTED  confidence=%.2f\n", squeak_conf);
        }
    }
}

void audio_classifier_init(void)
{
    i2s_init();
    xTaskCreatePinnedToCore(
        classifier_task,
        "audio_clf",
        16384,
        NULL,
        3,
        NULL,
        0
    );
    Serial.println("[AUDIO] Classifier task created");
}